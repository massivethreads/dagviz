#include "dagviz.h"

/****** GNUPLOT graphs ******/

void
dv_statistics_graph_delay_distribution() {
  char * filename;
  FILE * out;
  
  /* generate plots */
  filename = CS->SD->fn;
  if (!filename || strlen(filename) == 0) {
    fprintf(stderr, "Error: no file name to output.");
    return ;
  }
  out = fopen(filename, "w");
  dv_check(out);
  fprintf(out,
          "width=%d\n"
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_dis.png\n"
          "hist(x,width)=width*floor(x/width)+width/2.0\n"
          "set xrange [%ld:%ld]\n"
          "set yrange [0:]\n"
          "set boxwidth width\n"
          "set style fill solid 1.0 noborder\n"
          "set xlabel \"clocks\"\n"
          "set ylabel \"count\"\n"
          //          "plot \"-\" u (hist($1,width)):(1.0) smooth frequency w boxes lc rgb\"red\" t \"spawn\"\n");
          "plot ",
          CS->SD->bar_width,
          CS->SD->xrange_from,
          CS->SD->xrange_to);
  dv_stat_distribution_entry_t * e;
  int i;
  int count_e = 0;
  for (i = 0; i < CS->SD->ne; i++) {
    e = &CS->SD->e[i];
    if (e->dag_id >= 0)
      count_e++;
  }    
  for (i = 0; i < CS->SD->ne; i++) {
    e = &CS->SD->e[i];
    if (e->dag_id >= 0) {
      fprintf(out, "\"-\" u (hist($1,width)):(1.0) smooth frequency w boxes t \"%s\"", CS->SD->e[i].title);
      if (count_e > 1)
        fprintf(out, ", ");
      else
        fprintf(out, "\n");
      count_e--;
    }
  }
  for (i = 0; i < CS->SD->ne; i++) {
    e = &CS->SD->e[i];
    if (e->dag_id < 0)
      continue;
    dv_dag_t * D = CS->D + e->dag_id;
    if (e->type == 0 || e->type == 1)
      dv_dag_collect_delays_r(D, D->rt, out, e);
    else if (e->type == 2)
      dv_dag_collect_sync_delays_r(D, D->rt, out, e);
    else if (e->type == 3)
      dv_dag_collect_intervals_r(D, D->rt, out, e);
    fprintf(out,
            "e\n");
  }
  fprintf(out,
          "pause -1\n");
  fclose(out);
  fprintf(stdout, "generated distribution of delays to %s\n", filename);
  
  /* call gnuplot */
  GPid pid;
  char * argv[4];
  argv[0] = "gnuplot";
  argv[1] = "-persist";
  argv[2] = filename;
  argv[3] = NULL;
  int ret = g_spawn_async_with_pipes(NULL, argv, NULL,
                                     G_SPAWN_SEARCH_PATH, //G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH,
                                     NULL, NULL, &pid,
                                     NULL, NULL, NULL, NULL);
  if (!ret) {
    fprintf(stderr, "g_spawn_async_with_pipes() failed.\n");
  }
}

typedef struct {
  void (*process_event)(chronological_traverser * ct, dr_event evt);
  dr_pi_dag * G;
  dr_clock_t cum_running;
  dr_clock_t cum_delay;
  dr_clock_t cum_no_work;
  dr_clock_t t;
  long n_running;
  long n_ready;
  long n_workers;
  dr_clock_t total_elapsed;
  dr_clock_t total_t_1;
  long * edge_counts;		/* kind,u,v */
} dr_basic_stat;

static void 
dr_basic_stat_process_event(chronological_traverser * ct, 
			    dr_event evt);

static void
dr_calc_inner_delay(dr_basic_stat * bs, dr_pi_dag * G) {
  long n = G->n;
  long i;
  dr_clock_t total_elapsed = 0;
  dr_clock_t total_t_1 = 0;
  dr_pi_dag_node * T = G->T;
  long n_negative_inner_delays = 0;
  for (i = 0; i < n; i++) {
    dr_pi_dag_node * t = &T[i];
    dr_clock_t t_1 = t->info.t_1;
    dr_clock_t elapsed = t->info.end.t - t->info.start.t;
    if (t->info.kind < dr_dag_node_kind_section
	|| t->subgraphs_begin_offset == t->subgraphs_end_offset) {
      total_elapsed += elapsed;
      total_t_1 += t_1;
      if (elapsed < t_1 && t->info.worker != -1) {
	if (1 || (n_negative_inner_delays == 0)) {
	  fprintf(stderr,
		  "warning: node %ld has negative"
		  " inner delay (worker=%d, start=%llu, end=%llu,"
		  " t_1=%llu, end - start - t_1 = -%llu\n",
		  i, t->info.worker,
		  t->info.start.t, t->info.end.t, t->info.t_1,
		  t_1 - elapsed);
	}
	n_negative_inner_delays++;
      }
    }
  }
  if (n_negative_inner_delays > 0) {
    fprintf(stderr,
	    "warning: there are %ld nodes that have negative delays",
	    n_negative_inner_delays);
  }
  bs->total_elapsed = total_elapsed;
  bs->total_t_1 = total_t_1;
}

static void
dr_calc_edges(dr_basic_stat * bs, dr_pi_dag * G) {
  long n = G->n;
  long m = G->m;
  long nw = G->num_workers;
  /* C : a three dimensional array
     C(kind,i,j) is the number of type k edges from 
     worker i to worker j.
     we may counter nodes with worker id = -1
     (executed by more than one workers);
     we use worker id = nw for such entries
  */
  long * C_ = (long *)dr_malloc(sizeof(long) * dr_dag_edge_kind_max * (nw + 1) * (nw + 1));
#define EDGE_COUNTS(k,i,j) C_[k*(nw+1)*(nw+1)+i*(nw+1)+j]
  dr_dag_edge_kind_t k;
  long i, j;
  for (k = 0; k < dr_dag_edge_kind_max; k++) {
    for (i = 0; i < nw + 1; i++) {
      for (j = 0; j < nw + 1; j++) {
	EDGE_COUNTS(k,i,j) = 0;
      }
    }
  }
  for (i = 0; i < n; i++) {
    dr_pi_dag_node * t = &G->T[i];
    if (t->info.kind >= dr_dag_node_kind_section
	&& t->subgraphs_begin_offset == t->subgraphs_end_offset) {
      for (k = 0; k < dr_dag_edge_kind_max; k++) {
	int w = t->info.worker;
	if (w == -1) {
#if 0
	  fprintf(stderr, 
		  "warning: node %ld (kind=%s) has worker = %d)\n",
		  i, dr_dag_node_kind_to_str(t->info.kind), w);
#endif
	  EDGE_COUNTS(k, nw, nw) += t->info.logical_edge_counts[k];
	} else {
	  (void)dr_check(w >= 0);
	  (void)dr_check(w < nw);
	  EDGE_COUNTS(k, w, w) += t->info.logical_edge_counts[k];
	}
      }
    }    
  }
  for (i = 0; i < m; i++) {
    dr_pi_dag_edge * e = &G->E[i];
    int uw = G->T[e->u].info.worker;
    int vw = G->T[e->v].info.worker;
    if (uw == -1) {
#if 0
      fprintf(stderr, "warning: source node (%ld) of edge %ld %ld (kind=%s) -> %ld (kind=%s) has worker = %d\n",
	      e->u,
	      i, 
	      e->u, dr_dag_node_kind_to_str(G->T[e->u].info.kind), 
	      e->v, dr_dag_node_kind_to_str(G->T[e->v].info.kind), uw);
#endif
      uw = nw;
    }
    if (vw == -1) {
#if 0
      fprintf(stderr, "warning: dest node (%ld) of edge %ld %ld (kind=%s) -> %ld (kind=%s) has worker = %d\n",
	      e->v,
	      i, 
	      e->u, dr_dag_node_kind_to_str(G->T[e->u].info.kind), 
	      e->v, dr_dag_node_kind_to_str(G->T[e->v].info.kind), vw);
#endif
      vw = nw;
    }
    (void)dr_check(uw >= 0);
    (void)dr_check(uw <= nw);
    (void)dr_check(vw >= 0);
    (void)dr_check(vw <= nw);
    EDGE_COUNTS(e->kind, uw, vw)++;
  }
#undef EDGE_COUNTS
  bs->edge_counts = C_;
}

static void 
dr_basic_stat_init(dr_basic_stat * bs, dr_pi_dag * G) {
  bs->process_event = dr_basic_stat_process_event;
  bs->G = G;
  bs->n_running = 0;
  bs->n_ready = 0;
  bs->n_workers = G->num_workers;
  bs->cum_running = 0;		/* cumulative running cpu time */
  bs->cum_delay = 0;		/* cumulative delay cpu time */
  bs->cum_no_work = 0;		/* cumulative no_work cpu time */
  bs->t = 0;			/* time of the last event */
}

/*
static void
dr_basic_stat_destroy(dr_basic_stat * bs, dr_pi_dag * G) {
  long nw = G->num_workers;
  dr_free(bs->edge_counts, 
	  sizeof(long) * dr_dag_edge_kind_max * (nw + 1) * (nw + 1));
}
*/

static void 
dr_basic_stat_process_event(chronological_traverser * ct, 
			    dr_event evt) {
  dr_basic_stat * bs = (dr_basic_stat *)ct;
  dr_clock_t dt = evt.t - bs->t;

  int n_running = bs->n_running;
  int n_delay, n_no_work;
  if (bs->n_running >= bs->n_workers) {
    /* great, all workers are running */
    n_delay = 0;
    n_no_work = 0;
    if (bs->n_running > bs->n_workers) {
      fprintf(stderr, 
	      "warning: n_running = %ld"
	      " > n_workers = %ld (clock skew?)\n",
	      bs->n_running, bs->n_workers);
    }
    n_delay = 0;
    n_no_work = 0;
  } else if (bs->n_running + bs->n_ready <= bs->n_workers) {
    /* there were enough workers to run ALL ready tasks */
    n_delay = bs->n_ready;
    n_no_work = bs->n_workers - (bs->n_running + bs->n_ready);
  } else {
    n_delay = bs->n_workers - bs->n_running;
    n_no_work = 0;
  }
  bs->cum_running += n_running * dt;
  bs->cum_delay   += n_delay * dt;
  bs->cum_no_work += n_no_work * dt;

  switch (evt.kind) {
  case dr_event_kind_ready: {
    bs->n_ready++;
    break;
  }
  case dr_event_kind_start: {
    bs->n_running++;
    break;
  }
  case dr_event_kind_last_start: {
    bs->n_ready--;
    break;
  }
  case dr_event_kind_end: {
    bs->n_running--;
    break;
  }
  default:
    assert(0);
    break;
  }

  bs->t = evt.t;
}

void
dv_statistics_graph_execution_time_breakdown() {
  char * filename;
  FILE * out;
  
  /* generate plots */
  filename = CS->SBG->fn;
  if (!filename || strlen(filename) == 0) {
    fprintf(stderr, "Error: no file name to output.");
    return ;
  }
  out = fopen(filename, "w");
  dv_check(out);
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key outside center top horizontal\n"
          "set boxwidth 0.75 relative\n"
          "set yrange [0:]\n"
          //          "set xtics rotate by -30\n"
          //          "set xlabel \"clocks\"\n"
          "set ylabel \"cumul. clocks\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram t \"work\", "
          "\"-\" u 3 w histogram t \"delay\", "
          "\"-\" u 4 w histogram t \"nowork\"\n");
  int DAGs[DV_MAX_DAG];
  int n = 0;
  int i;
  for (i = 0; i < CS->nD; i++) {
    if (!CS->SBG->checked_D[i]) continue;
    dv_dag_t * D = &CS->D[i];
    DAGs[n++] = i;

    dr_pi_dag * G = D->P->G;
    dr_basic_stat bs[1];
    dr_basic_stat_init(bs, G);
    dr_calc_inner_delay(bs, G);
    dr_calc_edges(bs, G);
    dr_pi_dag_chronological_traverse(G, (chronological_traverser *)bs);

    dr_clock_t work = bs->total_t_1;
    dr_clock_t delay = bs->cum_delay + (bs->total_elapsed - bs->total_t_1);
    dr_clock_t no_work = bs->cum_no_work;
    CS->SBG->work[i] = work;
    CS->SBG->delay[i] = delay;
    CS->SBG->nowork[i] = no_work;
  }
  int j;
  for (j = 0; j < 3; j++) {
    for (i = 0; i < n; i++) {
      fprintf(out,
              "DAG_%d  %lld %lld %lld\n",
              DAGs[i],
              CS->SBG->work[DAGs[i]],
              CS->SBG->delay[DAGs[i]],
              CS->SBG->nowork[DAGs[i]]);
    }
    fprintf(out,
            "e\n");
  }
  fprintf(out,
          "pause -1\n");
  fclose(out);
  fprintf(stdout, "generated breakdown graphs to %s\n", filename);
  
  /* call gnuplot */
  GPid pid;
  char * argv[4];
  argv[0] = "gnuplot";
  argv[1] = "-persist";
  argv[2] = filename;
  argv[3] = NULL;
  int ret = g_spawn_async_with_pipes(NULL, argv, NULL,
                                     G_SPAWN_SEARCH_PATH, //G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH,
                                     NULL, NULL, &pid,
                                     NULL, NULL, NULL, NULL);
  if (!ret) {
    fprintf(stderr, "g_spawn_async_with_pipes() failed.\n");
  }
}

void
dv_statistics_graph_critical_path_breakdown(char * filename) {
  /* generate plots */
  if (!filename || strlen(filename) == 0) {
    fprintf(stderr, "Error: no file name to output.");
    return ;
  }
  FILE * out;
  out = fopen(filename, "w");
  dv_check(out);
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key outside center top horizontal\n"
          "set boxwidth 0.75 relative\n"
          "set yrange [0:]\n"
          "set ylabel \"cumul. clocks\"\n"
          "set xtics rotate by -30\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram t \"work\", "
          "\"-\" u 3 w histogram t \"delay\"\n");
  int i;
  for (i = 0; i < CS->nD; i++) {
    if (!CS->SBG->checked_D[i]) continue;
    
    //dv_dag_t * D = dv_create_new_dag(CS->D[i].P);
    dv_dag_t * D = &CS->D[i];
    dv_dag_compute_critical_paths(D);
  }

  int ptimes, cp;
  for (ptimes = 0; ptimes < 3; ptimes++) {
    
    for (i = 0; i < CS->nD; i++) {
      if (!CS->SBG->checked_D[i]) continue;
      dv_dag_t * D = &CS->D[i];
      for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
        if (!CS->SBG->checked_cp[cp]) continue;
        fprintf(out,
                "\"DAG%d (cp%d)\"  %lf %lf",
                i,
                cp,
                D->rt->cps[cp].work,
                D->rt->cps[cp].delay);
        fprintf(out, "\n");
      }
    }
    
    fprintf(out, "e\n");
  }

  fprintf(out,
          "pause -1\n");
  fclose(out);
  fprintf(stdout, "generated critical-path breakdown graphs to %s\n", filename);
  
  /* call gnuplot */
  GPid pid;
  char * argv[4];
  argv[0] = "gnuplot";
  argv[1] = "-persist";
  argv[2] = filename;
  argv[3] = NULL;
  int ret = g_spawn_async_with_pipes(NULL, argv, NULL,
                                     G_SPAWN_SEARCH_PATH, //G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH,
                                     NULL, NULL, &pid,
                                     NULL, NULL, NULL, NULL);
  if (!ret) {
    fprintf(stderr, "g_spawn_async_with_pipes() failed.\n");
  }
}

void
dv_statistics_graph_critical_path_delay_breakdown(char * filename) {
  /* generate plots */
  if (!filename || strlen(filename) == 0) {
    fprintf(stderr, "Error: no file name to output.");
    return ;
  }
  FILE * out;
  out = fopen(filename, "w");
  dv_check(out);
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key outside center top horizontal\n"
          "set boxwidth 0.75 relative\n"
          "set yrange [0:]\n"
          "set ylabel \"cumul. clocks\"\n"
          "set xtics rotate by -30\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram t \"work\", "
          "\"-\" u 3 w histogram t \"safe delay\", "
          "\"-\" u 4 w histogram t \"problematic delay\"\n");
  int i;
  for (i = 0; i < CS->nD; i++) {
    if (!CS->SBG->checked_D[i]) continue;
    
    //dv_dag_t * D = dv_create_new_dag(CS->D[i].P);
    dv_dag_t * D = &CS->D[i];
    dv_dag_compute_critical_paths(D);
  }

  int ptimes, cp;
  for (ptimes = 0; ptimes < 3; ptimes++) {
    
    for (i = 0; i < CS->nD; i++) {
      if (!CS->SBG->checked_D[i]) continue;
      dv_dag_t * D = &CS->D[i];
      for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
        if (!CS->SBG->checked_cp[cp]) continue;
        fprintf(out,
                "\"DAG%d (cp%d)\"  %lf %lf %lf",
                i,
                cp,
                D->rt->cps[cp].work,
                D->rt->cps[cp].delay - D->rt->cps[cp].problematic_delay,
                D->rt->cps[cp].problematic_delay);
        fprintf(out, "\n");
      }
    }
    
    fprintf(out, "e\n");
  }

  fprintf(out,
          "pause -1\n");
  fclose(out);
  fprintf(stdout, "generated critical-path breakdown graphs to %s\n", filename);
  
  /* call gnuplot */
  GPid pid;
  char * argv[4];
  argv[0] = "gnuplot";
  argv[1] = "-persist";
  argv[2] = filename;
  argv[3] = NULL;
  int ret = g_spawn_async_with_pipes(NULL, argv, NULL,
                                     G_SPAWN_SEARCH_PATH, //G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH,
                                     NULL, NULL, &pid,
                                     NULL, NULL, NULL, NULL);
  if (!ret) {
    fprintf(stderr, "g_spawn_async_with_pipes() failed.\n");
  }
}

void
dv_statistics_graph_critical_path_edge_based_delay_breakdown(char * filename) {
  /* generate plots */
  if (!filename || strlen(filename) == 0) {
    fprintf(stderr, "Error: no file name to output.");
    return ;
  }
  FILE * out;
  out = fopen(filename, "w");
  dv_check(out);
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key outside center top horizontal\n"
          "set boxwidth 0.75 relative\n"
          "set yrange [0:]\n"
          "set ylabel \"cumul. clocks\"\n"
          "set xtics rotate by -30\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram t \"work\", "
          "\"-\" u 3 w histogram t \"safe delay\", "
          "\"-\" u 4 w histogram t \"end\", "
          "\"-\" u 5 w histogram t \"create\", "
          "\"-\" u 6 w histogram t \"create cont.\", "
          "\"-\" u 7 w histogram t \"wait cont.\", "
          "\"-\" u 8 w histogram t \"other cont.\"\n");
  int i;
  for (i = 0; i < CS->nD; i++) {
    if (!CS->SBG->checked_D[i]) continue;
    
    //dv_dag_t * D = dv_create_new_dag(CS->D[i].P);
    dv_dag_t * D = &CS->D[i];
    dv_dag_compute_critical_paths(D);
  }

  int ptimes, cp;
  for (ptimes = 0; ptimes < 7; ptimes++) {
    
    for (i = 0; i < CS->nD; i++) {
      if (!CS->SBG->checked_D[i]) continue;
      for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
        if (!CS->SBG->checked_cp[cp]) continue;
        dv_dag_t * D = &CS->D[i];
        fprintf(out,
                "\"DAG%d (cp%d)\"  %lf %lf",
                i,
                cp,
                D->rt->cps[cp].work,
                D->rt->cps[cp].delay - D->rt->cps[cp].problematic_delay);
        int ek;
        for (ek = 0; ek < dr_dag_edge_kind_max; ek++)
          fprintf(out, " %lf", D->rt->cps[cp].pdelays[ek]);
        fprintf(out, "\n");
      }
    }
    
    fprintf(out, "e\n");
  }

  fprintf(out,
          "pause -1\n");
  fclose(out);
  fprintf(stdout, "generated critical-path delay breakdown graphs to %s\n", filename);
  
  /* call gnuplot */
  GPid pid;
  char * argv[4];
  argv[0] = "gnuplot";
  argv[1] = "-persist";
  argv[2] = filename;
  argv[3] = NULL;
  int ret = g_spawn_async_with_pipes(NULL, argv, NULL,
                                     G_SPAWN_SEARCH_PATH, //G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH,
                                     NULL, NULL, &pid,
                                     NULL, NULL, NULL, NULL);
  if (!ret) {
    fprintf(stderr, "g_spawn_async_with_pipes() failed.\n");
  }
}

/****** end of GNUPLOT graphs ******/

