#include "dagviz-gtk.h"

/****** GNUPLOT graphs ******/

void
dv_statistics_graph_delay_distribution() {
  char * filename;
  FILE * out;
  
  /* generate plots */
  filename = DVG->SD->fn;
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
          DVG->SD->bar_width,
          DVG->SD->xrange_from,
          DVG->SD->xrange_to);
  dv_stat_distribution_entry_t * e;
  int i;
  int count_e = 0;
  for (i = 0; i < DVG->SD->ne; i++) {
    e = &DVG->SD->e[i];
    if (e->dag_id >= 0)
      count_e++;
  }    
  for (i = 0; i < DVG->SD->ne; i++) {
    e = &DVG->SD->e[i];
    if (e->dag_id >= 0) {
      fprintf(out, "\"-\" u (hist($1,width)):(1.0) smooth frequency w boxes t \"%s\"", DVG->SD->e[i].title);
      if (count_e > 1)
        fprintf(out, ", ");
      else
        fprintf(out, "\n");
      count_e--;
    }
  }
  for (i = 0; i < DVG->SD->ne; i++) {
    e = &DVG->SD->e[i];
    if (e->dag_id < 0)
      continue;
    dm_dag_t * D = DMG->D + e->dag_id;
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

void
dv_statistics_graph_execution_time_breakdown() {
  char * filename;
  FILE * out;
  
  /* generate plots */
  filename = DMG->SBG->fn;
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
  int DAGs[DM_MAX_DAG];
  int n = 0;
  int i;
  for (i = 0; i < DMG->nD; i++) {
    if (!DMG->SBG->checked_D[i]) continue;
    dm_dag_t * D = &DMG->D[i];
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
    DMG->SBG->work[i] = work;
    DMG->SBG->delay[i] = delay;
    DMG->SBG->nowork[i] = no_work;
  }
  int j;
  for (j = 0; j < 3; j++) {
    for (i = 0; i < n; i++) {
      fprintf(out,
              "DAG_%d  %lld %lld %lld\n",
              DAGs[i],
              DMG->SBG->work[DAGs[i]],
              DMG->SBG->delay[DAGs[i]],
              DMG->SBG->nowork[DAGs[i]]);
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
          "#set terminal postscript eps enhanced color size 12cm,5.5cm\n"
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.75 relative\n"
          "set yrange [0:]\n"
          "set ylabel \"cumul. clocks\"\n"
          "set xtics rotate by -20\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram t \"work\", "
          "\"-\" u 3 w histogram t \"delay\"\n");
  int i;
  for (i = 0; i < DMG->nD; i++) {
    if (!DMG->SBG->checked_D[i]) continue;
    
    //dm_dag_t * D = dv_create_new_dag(DMG->D[i].P);
    dm_dag_t * D = &DMG->D[i];
    dm_dag_compute_critical_paths(D);
  }

  int ptimes, cp;
  for (ptimes = 0; ptimes < 2; ptimes++) {
    
    for (i = 0; i < DMG->nD; i++) {
      if (!DMG->SBG->checked_D[i]) continue;
      dm_dag_t * D = &DMG->D[i];
      int num_cp = 0;
      for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++)
        if (DMG->SBG->checked_cp[cp]) num_cp++;
      for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
        if (!DMG->SBG->checked_cp[cp]) continue;
        fprintf(out, "\"%s", D->name_on_graph);
        if (num_cp > 1)
          fprintf(out, " (cp%d)", cp + 1);
        fprintf(out,
                "\"  %lf %lf",
                D->rt->cpss[cp].work,
                D->rt->cpss[cp].delay);
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
          "#set terminal postscript eps enhanced color size 12cm,5.5cm\n"
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.75 relative\n"
          "set yrange [0:]\n"
          "set ylabel \"cumul. clocks\"\n"
          "set xtics rotate by -20\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram t \"work\", "
          "\"-\" u 3 w histogram t \"busy delay\", "
          "\"-\" u 4 w histogram t \"scheduler delay\"\n");
  int i;
  for (i = 0; i < DMG->nD; i++) {
    if (!DMG->SBG->checked_D[i]) continue;
    
    //dm_dag_t * D = dv_create_new_dag(DMG->D[i].P);
    dm_dag_t * D = &DMG->D[i];
    dm_dag_compute_critical_paths(D);
  }

  int ptimes, cp;
  for (ptimes = 0; ptimes < 3; ptimes++) {
    
    for (i = 0; i < DMG->nD; i++) {
      if (!DMG->SBG->checked_D[i]) continue;
      dm_dag_t * D = &DMG->D[i];
      int num_cp = 0;
      for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++)
        if (DMG->SBG->checked_cp[cp]) num_cp++;
      for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
        if (!DMG->SBG->checked_cp[cp]) continue;
        fprintf(out, "\"%s", D->name_on_graph);
        if (num_cp > 1)
          fprintf(out, " (cp%d)", cp + 1);
        fprintf(out,
                "\"  %lf %lf %lf",
                D->rt->cpss[cp].work,
                D->rt->cpss[cp].delay - D->rt->cpss[cp].sched_delay,
                D->rt->cpss[cp].sched_delay);
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
  /* check */
  if (!filename || strlen(filename) == 0) {
    fprintf(stderr, "Error: no file name to output.");
    return ;
  }
  
  /* calculate critical paths */
  int to_print_other_cont = 0;
  int i;
  for (i = 0; i < DMG->nD; i++) {
    if (!DMG->SBG->checked_D[i]) continue;
    
    //dm_dag_t * D = dv_create_new_dag(DMG->D[i].P);
    dm_dag_t * D = &DMG->D[i];
    dm_dag_compute_critical_paths(D);
    int cp;
    for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++)
      if (D->rt->cpss[cp].sched_delays[dr_dag_edge_kind_other_cont] > 0.0)
        to_print_other_cont = 1;
  }

  /* generate plots */
  FILE * out;
  out = fopen(filename, "w");
  dv_check(out);
  fprintf(out,
          "#set terminal postscript eps enhanced color size 12cm,5.5cm\n"
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.75 relative\n"
          "set yrange [0:]\n"
          "set ylabel \"cumul. clocks\"\n"
          "set xtics rotate by -20\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram t \"work\", "
          "\"-\" u 3 w histogram t \"busy delay\", "
          "\"-\" u 4 w histogram t \"end\", "
          "\"-\" u 5 w histogram t \"create\", "
          "\"-\" u 6 w histogram t \"create cont.\", "
          "\"-\" u 7 w histogram t \"wait cont.\"");
  if (to_print_other_cont)
    fprintf(out, ", \"-\" u 8 w histogram t \"other cont.\"\n");
  else
    fprintf(out, "\n");    

  /* print data */
  int nptimes = 7;
  if (!to_print_other_cont)
    nptimes = 6;
  int ptimes, cp;
  for (ptimes = 0; ptimes < nptimes; ptimes++) {
    
    for (i = 0; i < DMG->nD; i++) {
      if (!DMG->SBG->checked_D[i]) continue;
      dm_dag_t * D = &DMG->D[i];
      int num_cp = 0;
      for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++)
        if (DMG->SBG->checked_cp[cp]) num_cp++;
      for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
        if (!DMG->SBG->checked_cp[cp]) continue;
        fprintf(out, "\"%s", D->name_on_graph);
        if (num_cp > 1)
          fprintf(out, " (cp%d)", cp + 1);
        fprintf(out,
                "\"  %lf %lf",
                D->rt->cpss[cp].work,
                D->rt->cpss[cp].delay - D->rt->cpss[cp].sched_delay);
        int ek;
        for (ek = 0; ek < dr_dag_edge_kind_max; ek++)
          if (ek != dr_dag_edge_kind_other_cont || to_print_other_cont)
            fprintf(out, " %lf", D->rt->cpss[cp].sched_delays[ek]);
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

