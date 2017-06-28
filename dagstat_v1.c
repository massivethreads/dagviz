#include "dagstat.h"

const char * const DM_GRAPH_COLORS[] =
  {"red", "green", "web-blue",
   "dark-spring-green", "green",
   "green", "blue", "magenta", "cyan", "yellow",
   "greenyellow", "light-blue"
  };
/* work, delay, no-work,
   busy delay, scheduler delay,
   end delay, create delay, create cont. delay, wait delay, other delay,
   delay during cp-sd, delay during cp-w
*/

static const char * DEFAULT_BASE_DIR = "ds_graphs/fig/gpl";

#define ds_open_file(X) ds_open_file_(X, __FILE__, __LINE__)

static FILE *
ds_open_file_(char * filename, char * file, int line) {
  if (!filename || strlen(filename) == 0) {
    fprintf(stderr, "Warning at %s:%d: no file name to output.\n", file, line);
    return NULL;
  }
  FILE * out = fopen(filename, "w");
  if (!out) {
    fprintf(stderr, "Warning at %s:%d: cannot open file %s.\n", file, line, filename);
    return NULL;
  }
  return out;
}

static void
ds_close_file(FILE * fp) {
  fclose(fp);
}

dm_pidag_t *
dm_create_new_pidag(char * filename) {
  if (!filename) return NULL;
  dm_pidag_t * P = dm_pidag_read_new_file(filename);
  if (!P) return NULL;

  P->name = malloc( sizeof(char) * 20 );
  sprintf(P->name, "DAG file %ld", P - DMG->P);
  
  return P;
}

dm_dag_t *
dm_create_new_dag(dm_pidag_t * P) {
  if (!P) return NULL;
  dm_dag_t * D = dm_dag_create_new_with_pidag(P);
  if (!D) return NULL;
  
  return D;
}

void
ds_stat_graph_time(char * filename_) {
  int i, j;
  FILE * out;
  char filename[1000];

  /* graph 1: time breakdown */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_breakdown.gpl", filename);
  out = ds_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dm_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [0:]\n"
          "set xtics rotate by -20\n"
          //          "set xlabel \"clocks\"\n"
          "set ylabel \"cumul. clocks\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram t \"work\" lc rgb \"%s\", "
          "\"-\" u 3 w histogram t \"delay\" lc rgb \"%s\", "
          "\"-\" u 4 w histogram t \"no-work\" lc rgb \"%s\"\n",
          DM_GRAPH_COLORS[0],
          DM_GRAPH_COLORS[1],
          DM_GRAPH_COLORS[2]
          );
  for (j = 0; j < 3; j++) {
    for (i = 0; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      fprintf(out,
              "\"%s\"  %lld %lld %lld\n",
              D->name_on_graph,
              DMG->SBG->work[i],
              DMG->SBG->delay[i],
              DMG->SBG->nowork[i]);
    }
    fprintf(out,
            "e\n");
  }
  fprintf(out,
          "pause -1\n");
  ds_close_file(out);
  fprintf(stdout, "generated a graph to %s\n", filename);

  /* Get max y */
  double max_y = 0.0;
  double max_y_percent = 0.0;
  double base = DMG->SBG->work[0] + DMG->SBG->delay[0] + DMG->SBG->nowork[0];
  for (i = 0; i < DMG->nD; i++) {
    double total_loss = (DMG->SBG->work[i] - DMG->SBG->work[0]) + (DMG->SBG->delay[i]) + (DMG->SBG->nowork[i]);
    if (total_loss > max_y) {
      max_y = total_loss;
      max_y_percent = ceil(max_y / base * 100.0);
    }
  }

  /* graph 2: time work */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_only_work.gpl", filename);
  out = ds_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dm_stat_breakdown.png\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [0:]\n"
          "set xtics rotate by -20\n"
          //          "set xlabel \"clocks\"\n"
          "set ylabel \"cumul. clocks\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w boxes t \"work\" lc rgb \"%s\", \n",
          DM_GRAPH_COLORS[0]
          );
  for (j = 0; j < 1; j++) {
    for (i = 0; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      fprintf(out,
              "\"%s\"  %lld\n",
              D->name_on_graph,
              DMG->SBG->work[i]);
    }
    fprintf(out,
            "e\n");
  }
  fprintf(out,
          "pause -1\n");
  ds_close_file(out);
  fprintf(stdout, "generated a graph to %s\n", filename);
  
  /* graph 3: performance loss breakdown */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_performance_loss_breakdown.gpl", filename);
  out = ds_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dm_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%lf<*]\n"
          "set xtics rotate by -20\n"
          "set ylabel \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram lc rgb \"%s\" t \"work stretch\", "
          "\"-\" u 3 w histogram lc rgb \"%s\" t \"delay\", "
          "\"-\" u 4 w histogram lc rgb \"%s\" t \"no-work (sched.)\", "
          "\"-\" u 5 w histogram lc rgb \"%s\" t \"no-work (algo.)\", "
          "\"-\" u 0:6:7 w labels center offset 0,1 notitle \n",
          max_y_percent,
          DM_GRAPH_COLORS[0],
          DM_GRAPH_COLORS[1],
          DM_GRAPH_COLORS[2],
          DM_GRAPH_COLORS[11]          
          );
  for (j = 0; j < 5; j++) {
    for (i = 0; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double loss_work = (DMG->SBG->work[i] - DMG->SBG->work[0]) * 100.0 / base;
      double loss_delay = DMG->SBG->delay[i] * 100.0 / base;
      double loss_nowork_a = D->rt->cpss[DM_CRITICAL_PATH_1].sched_delay_nowork * 100.0 / base;
      double loss_nowork_b = (DMG->SBG->nowork[i] - D->rt->cpss[DM_CRITICAL_PATH_1].sched_delay_nowork) * 100.0 / base;
      double loss = loss_work + loss_delay + loss_nowork_a + loss_nowork_b;
      fprintf(out,
              "\"%s\"  %lf %lf %lf %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              loss_work,
              loss_delay,
              loss_nowork_a,
              loss_nowork_b,
              loss,
              loss);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  ds_close_file(out);
  fprintf(stdout, "generated a graph to %s\n", filename);
}

void
ds_stat_graph_counters(char * filename_) {
  int i, j;
  FILE * out;
  char filename[1000];

  /* counters */
  int c;
  for (c = 0; c < 4; c++) {
    strcpy(filename, filename_);
    filename[strlen(filename) - 4] = '\0';
    sprintf(filename, "%s_%d.gpl", filename, c);
    out = ds_open_file(filename);
    if (!out) return;
    fprintf(out,
            "set style fill solid 0.8 noborder\n"
            "set key inside #off #outside center top horizontal\n"
            "set boxwidth 0.85 relative\n"
            "set yrange [0:]\n"
            "set ylabel \"counts\"\n"
            "set xtics rotate by -20\n"
            "plot "
            "\"-\" u 2:xtic(1) w boxes t \"counter %d\", "
            "\"-\" u 0:2:3 w labels center offset 0,1 notitle \n",
            c
            );
    for (j = 0; j < 1; j++) {
      for (i = 0; i < DMG->nD; i++) {
        dm_dag_t * D = &DMG->D[i];
        dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, D->rt);
        fprintf(out,
                "\"%s\"  %lld\n",
                D->name_on_graph,
                pi->info.counters_1[c]);
        //fprintf(stderr, "%s counter %d: %lld\n", D->name_on_graph);
      }
      fprintf(out, "e\n");
    }
    fprintf(out, "pause -1\n");
    ds_close_file(out);
    fprintf(stdout, "generated a graph to %s\n", filename);
  }

  /* counter ($1/$2) */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_0d1.gpl", filename);
  out = ds_open_file(filename);
  if (!out) return;
  fprintf(out,
          "set style fill solid 0.8 noborder\n"
          "set key inside #off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [0:]\n"
          "set ylabel \"counts\"\n"
          "set xtics rotate by -20\n"
          "plot "
          "\"-\" u 2:xtic(1) w boxes t \"c. 0 div. by c. 1 (ipc)\", "
          "\"-\" u 0:2:3 w labels center offset 0,1 notitle \n"
          );
  for (j = 0; j < 1; j++) {
    for (i = 0; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, D->rt);
      fprintf(out,
              "\"%s\"  %lf\n",
              D->name_on_graph,
              (pi->info.counters_1[1] != 0) ? ((double) pi->info.counters_1[0]) / ((double) pi->info.counters_1[1]) : 0.0);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  ds_close_file(out);
  fprintf(stdout, "generated a graph to %s\n", filename);
}

int
main(int argc, char ** argv) {
  /* Init */
  const char * ds_prefix = getenv("DS_PREFIX");
  if (!ds_prefix)
    ds_prefix = DEFAULT_BASE_DIR;
  printf("ds_prefix = %s\n", ds_prefix);

  dm_global_state_init(DMG);
  
  /* PIDAG */
  int i;
  for (i = 1; i < argc; i++) {
    glob_t globbuf;
    glob(argv[i], GLOB_TILDE | GLOB_PERIOD | GLOB_BRACE, NULL, &globbuf);
    int j;
    for (j = 0; j < (int) globbuf.gl_pathc; j++) {
      _unused_ dm_pidag_t * P = dm_create_new_pidag(globbuf.gl_pathv[j]);
    }
    if (globbuf.gl_pathc > 0)
      globfree(&globbuf);
  }
  for (i = 0; i < DMG->nP; i++) {
    dm_pidag_t * P = &DMG->P[i];
    _unused_ dm_dag_t * D = dm_create_new_dag(P);
  }

  /* Preparation */
  for (i = 0; i < DMG->nD; i++) {
    dm_dag_t * D = &DMG->D[i];

    dr_pi_dag * G = D->P->G;
    dr_basic_stat bs[1];
    dr_basic_stat_init(bs, G);
    dr_calc_inner_delay(bs, G);
    dr_calc_edges(bs, G);
    dr_pi_dag_chronological_traverse(G, (chronological_traverser *)bs);

    dr_clock_t work = bs->total_t_1;
    dr_clock_t delay = bs->cum_delay + (bs->total_elapsed - bs->total_t_1);
    dr_clock_t no_work = bs->cum_no_work;
    DMG->SBG->work[i] = work;
    DMG->SBG->delay[i] = delay;
    DMG->SBG->nowork[i] = no_work;
  }
  for (i = 0; i < DMG->nD; i++) {
    dm_dag_t * D = &DMG->D[i];
    dm_dag_compute_critical_paths(D);
  }

  /* Create directories */
  char command[1000];
  sprintf(command, "mkdir -p %s", ds_prefix);
  int err = system(command);
  if (err == -1) {
    printf("Error: cannot make dir of %s\n", ds_prefix);
    exit(1);
  }
  
  /* Graphs */
  char filename[1000];
  
  sprintf(filename, "%s/%s", ds_prefix, "cumulative_execution_time.gpl");
  ds_stat_graph_time(filename);

  sprintf(filename, "%s/%s", ds_prefix, "work_counter.gpl");
  ds_stat_graph_counters(filename);

  return 1;
}
