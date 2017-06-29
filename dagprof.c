#include "interface.c"

const char * const DV_GRAPH_COLORS[] =
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

static const char * DEFAULT_BASE_DIR = "dp_graphs/fig/gpl/";

#define dp_open_file(X) dp_open_file_(X, __FILE__, __LINE__)

static FILE *
dp_open_file_(char * filename, char * file, int line) {
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
dp_close_file(FILE * fp) {
  fclose(fp);
}

static void
dp_global_state_init_nogtk(dv_global_state_t * CS) {
  memset(CS, 0, sizeof(dv_global_state_t));
  DMG->nP = 0;
  DMG->nD = 0;
  DVG->nV = 0;
  DVG->nVP = 0;
  DVG->activeV = NULL;
  DVG->activeVP = NULL;
  DVG->error = DV_OK;
  int i;
  for (i = 0; i < DV_NUM_COLOR_POOLS; i++)
    DVG->CP_sizes[i] = 0;
  //dv_btsample_viewer_init(DVG->btviewer);
  dm_dag_node_pool_init(DMG->pool);
  dm_histogram_entry_pool_init(DMG->epool);
  DVG->SD->ne = 0;
  for (i = 0; i < DV_MAX_DISTRIBUTION; i++) {
    DVG->SD->e[i].dag_id = -1; /* none */
    DVG->SD->e[i].type = 0;
    DVG->SD->e[i].stolen = 0;
    DVG->SD->e[i].title = NULL;
    DVG->SD->e[i].title_entry = NULL;
  }
  DVG->SD->xrange_from = 0;
  DVG->SD->xrange_to = 10000;
  DVG->SD->node_pool_label = NULL;
  DVG->SD->entry_pool_label = NULL;
  DVG->SD->fn = DV_STAT_DISTRIBUTION_OUTPUT_DEFAULT_NAME;
  DVG->SD->bar_width = 20;
  for (i = 0; i < DM_MAX_DAG; i++) {
    DMG->SBG->checked_D[i] = 1;
    DMG->SBG->work[i] = 0.0;
    DMG->SBG->delay[i] = 0.0;
    DMG->SBG->nowork[i] = 0.0;
  }
  DMG->SBG->fn = DV_STAT_BREAKDOWN_OUTPUT_DEFAULT_NAME;
  DMG->SBG->fn_2 = DV_STAT_BREAKDOWN_OUTPUT_DEFAULT_NAME_2;
  int cp;
  for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
    DMG->SBG->checked_cp[cp] = 0;
    if (cp == DV_CRITICAL_PATH_1)
      DMG->SBG->checked_cp[cp] = 1;
  }
  
  DVG->context_view = NULL;
  DVG->context_node = NULL;

  DVG->verbose_level = DV_VERBOSE_LEVEL_DEFAULT;

  DMG->oncp_flags[DV_CRITICAL_PATH_0] = DV_NODE_FLAG_CRITICAL_PATH_0;
  DMG->oncp_flags[DV_CRITICAL_PATH_1] = DV_NODE_FLAG_CRITICAL_PATH_1;
  DMG->oncp_flags[DV_CRITICAL_PATH_2] = DV_NODE_FLAG_CRITICAL_PATH_2;
  DVG->cp_colors[DV_CRITICAL_PATH_0] = DV_CRITICAL_PATH_0_COLOR;
  DVG->cp_colors[DV_CRITICAL_PATH_1] = DV_CRITICAL_PATH_1_COLOR;
  DVG->cp_colors[DV_CRITICAL_PATH_2] = DV_CRITICAL_PATH_2_COLOR;
}

_static_unused_ void
dp_call_gnuplot(char * filename) {
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
dp_stat_graph_execution_time_breakdown(char * filename) {
  /* graph 1: execution time breakdown (including serial) */
  FILE * out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [0:]\n"
          //          "set xtics rotate by -30\n"
          //          "set xlabel \"clocks\"\n"
          "set ylabel \"cumul. clocks\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram t \"work\" lc rgb \"%s\", "
          "\"-\" u 3 w histogram t \"delay\" lc rgb \"%s\", "
          "\"-\" u 4 w histogram t \"no-work\" lc rgb \"%s\"\n",
          DV_GRAPH_COLORS[0],
          DV_GRAPH_COLORS[1],
          DV_GRAPH_COLORS[2]
          );
  int i, j;
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
  dp_close_file(out);
  fprintf(stdout, "generated breakdown graphs to %s\n", filename);

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

  /* graph 2: performance loss (without percentage) */
  char filename_[1000];
  strcpy(filename_, filename);
  filename_[strlen(filename) - 4] = '\0';
  sprintf(filename_, "%s_perf_loss.gpl", filename_);
  out = dp_open_file(filename_);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [0:]\n"
          //          "set xtics rotate by -30\n"
          //          "set xlabel \"clocks\"\n"
          "set ylabel \"cumul. clocks\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram t \"work stretch\" lc rgb \"%s\", "
          "\"-\" u 3 w histogram t \"delay\" lc rgb \"%s\", "
          "\"-\" u 4 w histogram t \"no-work\" lc rgb \"%s\"\n",
          DV_GRAPH_COLORS[0],
          DV_GRAPH_COLORS[1],
          DV_GRAPH_COLORS[2]          
          );
  for (j = 0; j < 3; j++) {
    for (i = 0; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      fprintf(out,
              "\"%s\"  %lld %lld %lld\n",
              D->name_on_graph,
              DMG->SBG->work[i] - DMG->SBG->work[0],
              DMG->SBG->delay[i],
              DMG->SBG->nowork[i]);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated breakdown graphs to %s\n", filename_);

  /* graph 3: performance loss with percentage line */
  strcpy(filename_, filename);
  filename_[strlen(filename) - 4] = '\0';
  sprintf(filename_, "%s_perf_loss_with_percentage_line.gpl", filename_);
  out = dp_open_file(filename_);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:]\n"
          "set y2range [*<0:100<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "#set y2tics\n"
          "#set ytics nomirror\n"
          "#set y2label \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram lc rgb \"%s\" t \"work stretch\" axes x1y1, "
          "\"-\" u 3 w histogram lc rgb \"%s\" t \"delay\" axes x1y1, "
          "\"-\" u 4 w histogram lc rgb \"%s\" t \"no-work\" axes x1y1, "
          "\"-\" u 5 w linespoints lt 2 lc rgb \"orange\" t \"percentage\" axes x1y2, "
          "\"-\" u 0:5:6 w labels center offset 0,1.0 axes x1y2 notitle \n",
          DV_GRAPH_COLORS[0],
          DV_GRAPH_COLORS[1],
          DV_GRAPH_COLORS[2]          
          );
  for (j = 0; j < 5; j++) {
    for (i = 0; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      dm_clock_t loss = (DMG->SBG->work[i] - DMG->SBG->work[0])+ (DMG->SBG->delay[i]) + (DMG->SBG->nowork[i]);
      double percentage = loss * 100.0 / base;
      fprintf(out,
              "\"%s\"  %lld %lld %lld %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              DMG->SBG->work[i] - DMG->SBG->work[0],
              DMG->SBG->delay[i],
              DMG->SBG->nowork[i],
              percentage,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated breakdown graphs to %s\n", filename_);

  /* graph 4: performance loss with percentage labels */
  strcpy(filename_, filename);
  filename_[strlen(filename) - 4] = '\0';
  sprintf(filename_, "%s_perf_loss_with_percentage_labels.gpl", filename_);
  out = dp_open_file(filename_);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram lc rgb \"%s\" t \"work stretch\", "
          "\"-\" u 3 w histogram lc rgb \"%s\" t \"delay\", "
          "\"-\" u 4 w histogram lc rgb \"%s\" t \"no-work\", "
          "\"-\" u 0:5:6 w labels center offset 0,1.0 notitle \n",
          DV_GRAPH_COLORS[0],
          DV_GRAPH_COLORS[1],
          DV_GRAPH_COLORS[2]          
          );
  for (j = 0; j < 4; j++) {
    for (i = 0; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      dm_clock_t loss = (DMG->SBG->work[i] - DMG->SBG->work[0]) + (DMG->SBG->delay[i]) + (DMG->SBG->nowork[i]);
      double percentage = loss * 100.0 / base;
      fprintf(out,
              "\"%s\"  %lld %lld %lld %lld \"%.1lf%%\"\n",
              D->name_on_graph,
              DMG->SBG->work[i] - DMG->SBG->work[0],
              DMG->SBG->delay[i],
              DMG->SBG->nowork[i],
              loss,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated breakdown graphs to %s\n", filename_);

  /* graph 5: performance loss percentages */
  strcpy(filename_, filename);
  filename_[strlen(filename) - 4] = '\0';
  sprintf(filename_, "%s_perf_loss_percentages.gpl", filename_);
  out = dp_open_file(filename_);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%lf<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram lc rgb \"%s\" t \"work stretch\", "
          "\"-\" u 3 w histogram lc rgb \"%s\" t \"delay\", "
          "\"-\" u 4 w histogram lc rgb \"%s\" t \"no-work\", "
          "\"-\" u 0:5:6 w labels center offset 0,1 notitle \n",
          max_y_percent,
          DV_GRAPH_COLORS[0],
          DV_GRAPH_COLORS[1],
          DV_GRAPH_COLORS[2]          
          );
  for (j = 0; j < 4; j++) {
    for (i = 0; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double loss_work = (DMG->SBG->work[i] - DMG->SBG->work[0]) * 100.0 / base;
      double loss_delay = (DMG->SBG->delay[i]) * 100.0 / base;
      double loss_nowork = (DMG->SBG->nowork[i]) * 100.0 / base;
      double loss = loss_work + loss_delay + loss_nowork;
      fprintf(out,
              "\"%s\"  %lf %lf %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              loss_work,
              loss_delay,
              loss_nowork,
              loss,
              loss);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated breakdown graphs to %s\n", filename_);

  /* graph 6: performance loss percentages with fixed yrange [0:100] */
  strcpy(filename_, filename);
  filename_[strlen(filename) - 4] = '\0';
  sprintf(filename_, "%s_perf_loss_percentages_fixed.gpl", filename_);
  out = dp_open_file(filename_);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:100<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram lc rgb \"%s\" t \"work stretch\", "
          "\"-\" u 3 w histogram lc rgb \"%s\" t \"delay\", "
          "\"-\" u 4 w histogram lc rgb \"%s\" t \"no-work\", "
          "\"-\" u 0:5:6 w labels center offset 0,1 notitle \n",
          DV_GRAPH_COLORS[0],
          DV_GRAPH_COLORS[1],
          DV_GRAPH_COLORS[2]          
          );
  for (j = 0; j < 4; j++) {
    for (i = 0; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double loss_work = (DMG->SBG->work[i] - DMG->SBG->work[0]) * 100.0 / base;
      double loss_delay = (DMG->SBG->delay[i]) * 100.0 / base;
      double loss_nowork = (DMG->SBG->nowork[i]) * 100.0 / base;
      double loss = loss_work + loss_delay + loss_nowork;
      fprintf(out,
              "\"%s\"  %lf %lf %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              loss_work,
              loss_delay,
              loss_nowork,
              loss,
              loss);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated breakdown graphs to %s\n", filename_);

  /* graph 7: performance loss with percentage labels 2 (including delay & no-work breakdowns) */
  strcpy(filename_, filename);
  filename_[strlen(filename) - 4] = '\0';
  sprintf(filename_, "%s_perf_loss_with_percentage_labels_2.gpl", filename_);
  out = dp_open_file(filename_);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram lc rgb \"%s\" t \"work stretch\", "
          "\"-\" u 3 w histogram lc rgb \"%s\" t \"delay w\", "
          "\"-\" u 4 w histogram lc rgb \"%s\" t \"delay sd\", "
          "\"-\" u 5 w histogram lc rgb \"%s\" t \"no-work w\", "
          "\"-\" u 6 w histogram lc rgb \"%s\" t \"no-work sd\", "
          "\"-\" u 0:7:8 w labels center offset 0,1.0 notitle \n",
          DV_GRAPH_COLORS[0],
          DV_GRAPH_COLORS[1],
          DV_GRAPH_COLORS[10],
          DV_GRAPH_COLORS[2],
          DV_GRAPH_COLORS[11]          
          );
  for (j = 0; j < 6; j++) {
    for (i = 0; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      dm_clock_t loss = (DMG->SBG->work[i] - DMG->SBG->work[0]) + (DMG->SBG->delay[i]) + (DMG->SBG->nowork[i]);
      double percentage = loss * 100.0 / base;
      fprintf(out,
              "\"%s\"  %lld %lf %lf %lf %lf %lld \"%.1lf%%\"\n",
              D->name_on_graph,
              DMG->SBG->work[i] - DMG->SBG->work[0],
              (i != 0) ? (DMG->SBG->delay[i] - D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_delay) : 0,
              (i != 0) ? (D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_delay) : (DMG->SBG->delay[i]),
              DMG->SBG->nowork[i] - D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_nowork,
              D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_delay,
              loss,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated breakdown graphs to %s\n", filename_);

  /* graph 8: performance loss percentages 2 (including delay & no-work breakdowns) */
  strcpy(filename_, filename);
  filename_[strlen(filename) - 4] = '\0';
  sprintf(filename_, "%s_perf_loss_percentages_2.gpl", filename_);
  out = dp_open_file(filename_);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%lf<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram lc rgb \"%s\" t \"work stretch\", "
          "\"-\" u 3 w histogram lc rgb \"%s\" t \"delay sd\", "
          "\"-\" u 4 w histogram lc rgb \"%s\" t \"delay w\", "
          "\"-\" u 5 w histogram lc rgb \"%s\" t \"no-work sd\", "
          "\"-\" u 6 w histogram lc rgb \"%s\" t \"no-work w\", "
          "\"-\" u 0:7:8 w labels center offset 0,1 notitle \n",
          max_y_percent,
          DV_GRAPH_COLORS[0],
          DV_GRAPH_COLORS[1],
          DV_GRAPH_COLORS[10],
          DV_GRAPH_COLORS[2],
          DV_GRAPH_COLORS[11]          
          );
  for (j = 0; j < 6; j++) {
    for (i = 0; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double loss_work = (DMG->SBG->work[i] - DMG->SBG->work[0]) * 100.0 / base;
      double loss_delay_w = (i != 0) ? ((DMG->SBG->delay[i] - D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_delay) * 100.0 / base) : 0.0;
      double loss_delay_sd = (i != 0) ? (D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_delay * 100.0 / base) : (DMG->SBG->delay[i] * 100.0 / base);
      double loss_nowork_w = (DMG->SBG->nowork[i] - D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_nowork) * 100.0 / base;
      double loss_nowork_sd = D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_nowork * 100.0 / base;
      double loss = loss_work + loss_delay_w + loss_delay_sd + loss_nowork_w + loss_nowork_sd;
      fprintf(out,
              "\"%s\"  %lf %lf %lf %lf %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              loss_work,
              loss_delay_sd,
              loss_delay_w,
              loss_nowork_sd,
              loss_nowork_w,
              loss,
              loss);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated breakdown graphs to %s\n", filename_);

  /* graph 9: performance loss percentages 2 (including delay & no-work breakdowns) */
  strcpy(filename_, filename);
  filename_[strlen(filename) - 4] = '\0';
  sprintf(filename_, "%s_perf_loss_percentages_3.gpl", filename_);
  out = dp_open_file(filename_);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%lf<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram lc rgb \"%s\" t \"work stretch\", "
          "\"-\" u 3 w histogram lc rgb \"%s\" t \"delay\", "
          "\"-\" u 4 w histogram lc rgb \"%s\" t \"no-work (s)\", "
          "\"-\" u 5 w histogram lc rgb \"%s\" t \"no-work (p)\", "
          "\"-\" u 0:6:7 w labels center offset 0,1 notitle \n",
          max_y_percent,
          DV_GRAPH_COLORS[0],
          DV_GRAPH_COLORS[1],
          DV_GRAPH_COLORS[2],
          DV_GRAPH_COLORS[11]          
          );
  for (j = 0; j < 5; j++) {
    for (i = 0; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double loss_work = (DMG->SBG->work[i] - DMG->SBG->work[0]) * 100.0 / base;
      double loss_delay = DMG->SBG->delay[i] * 100.0 / base;
      double loss_nowork_a = D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_nowork * 100.0 / base;
      double loss_nowork_b = (DMG->SBG->nowork[i] - D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_nowork) * 100.0 / base;
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
  dp_close_file(out);
  fprintf(stdout, "generated breakdown graphs to %s\n", filename_);

}

void
dp_stat_graph_critical_path_breakdown(char * filename) {
  /* generate plots */
  FILE * out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal postscript eps enhanced color size 12cm,5.5cm\n"
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [0:]\n"
          "set ylabel \"cumul. clocks\"\n"
          "#set xtics rotate by -20\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram lc rgb \"%s\" t \"work\", "
          "\"-\" u 3 w histogram lc rgb \"%s\" t \"delay\"\n",
          DV_GRAPH_COLORS[0],
          DV_GRAPH_COLORS[3]
          );
  int ptimes, cp;
  for (ptimes = 0; ptimes < 2; ptimes++) {
    
    int i;
    for (i = 0; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      /* exclude serial DAGs */
      if (strstr(D->name_on_graph, "serial")) continue;
      cp = DV_CRITICAL_PATH_1;
      fprintf(out, "\"%s", D->name_on_graph);
      fprintf(out,
              "\"  %lf %lf",
              D->rt->cpss[cp].work,
              D->rt->cpss[cp].delay);
      fprintf(out, "\n");
    }
    
    fprintf(out, "e\n");
  }

  fprintf(out,
          "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated critical path's work-delay breakdown graphs to %s\n", filename);
}

void
dp_stat_graph_critical_path_delay_breakdown(char * filename) {
  /* generate plots */
  FILE * out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal postscript eps enhanced color size 12cm,5.5cm\n"
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [0:]\n"
          "set ylabel \"cumul. clocks\"\n"
          "#set xtics rotate by -20\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram lc rgb \"%s\" t \"work\", "
          "\"-\" u 3 w histogram lc rgb \"%s\" t \"busy delay\", "
          "\"-\" u 4 w histogram lc rgb \"%s\" t \"scheduler delay\"\n",
          DV_GRAPH_COLORS[0],
          DV_GRAPH_COLORS[3],
          DV_GRAPH_COLORS[4]          
          );

  int ptimes, cp;
  for (ptimes = 0; ptimes < 3; ptimes++) {

    int i;
    for (i = 0; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      /* exclude serial DAGs */
      if (strstr(D->name_on_graph, "serial")) continue;
      cp = DV_CRITICAL_PATH_1;
      fprintf(out, "\"%s", D->name_on_graph);
      fprintf(out,
              "\"  %lf %lf %lf",
              D->rt->cpss[cp].work,
              D->rt->cpss[cp].delay - D->rt->cpss[cp].sched_delay,
              D->rt->cpss[cp].sched_delay);
      fprintf(out, "\n");
    }
    
    fprintf(out, "e\n");
  }

  fprintf(out,
          "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated critical-path breakdown graphs to %s\n", filename);
}

void
dp_stat_graph_critical_path_edge_based_delay_breakdown(char * filename) {
  /* calculate critical paths */
  int to_print_other_cont = 0;
  int i;
  for (i = 0; i < DMG->nD; i++) {
    dm_dag_t * D = &DMG->D[i];
    int cp;
    for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++)
      if (D->rt->cpss[cp].sched_delays[dr_dag_edge_kind_other_cont] > 0.0)
        to_print_other_cont = 1;
  }

  /* generate plots */
  FILE * out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal postscript eps enhanced color size 12cm,5.5cm\n"
          "#set terminal png font arial 14 size 640,350\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [0:]\n"
          "set ylabel \"cumul. clocks\"\n"
          "#set xtics rotate by -20\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram lc rgb \"%s\" t \"work\", "
          "\"-\" u 3 w histogram lc rgb \"%s\" t \"busy delay\", "
          "\"-\" u 4 w histogram lc rgb \"%s\" t \"end\", "
          "\"-\" u 5 w histogram lc rgb \"%s\" t \"create\", "
          "\"-\" u 6 w histogram lc rgb \"%s\" t \"create cont.\", "
          "\"-\" u 7 w histogram lc rgb \"%s\" t \"wait cont.\"",
          DV_GRAPH_COLORS[0],
          DV_GRAPH_COLORS[3],
          DV_GRAPH_COLORS[5],
          DV_GRAPH_COLORS[6],
          DV_GRAPH_COLORS[7],
          DV_GRAPH_COLORS[8]
          );
  if (to_print_other_cont)
    fprintf(out, ", \"-\" u 8 w histogram lc rgb \"%s\" t \"other cont.\"\n",
            DV_GRAPH_COLORS[9]);
  else
    fprintf(out, "\n");    

  /* print data */
  int nptimes = 7;
  if (!to_print_other_cont)
    nptimes = 6;
  int ptimes, cp;
  for (ptimes = 0; ptimes < nptimes; ptimes++) {
    
    for (i = 0; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      /* exclude serial DAGs */
      if (strstr(D->name_on_graph, "serial")) continue;
      cp = DV_CRITICAL_PATH_1;
      fprintf(out, "\"%s", D->name_on_graph);
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
    
    fprintf(out, "e\n");
  }

  fprintf(out,
          "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated critical path's scheduler delay breakdown graphs to %s\n", filename);
}

void
dp_stat_graph_performance_loss_factors_with_percentage_lines(char * filename_) {
  /* Get max y */
  int i;
  double max_y = 0.0;
  for (i = 0; i < DMG->nD; i++) {
    double total_loss = (DMG->SBG->work[i] - DMG->SBG->work[0]) + (DMG->SBG->delay[i]) + (DMG->SBG->nowork[i]);
    if (total_loss > max_y)
      max_y = total_loss;
  }

  FILE * out;
  char filename[1000];

  /* work stretch */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_work_stretch.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%.0lf<*]\n"
          "set y2range [*<0:100<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "#set y2tics\n"
          "#set ytics nomirror\n"
          "#set y2label \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w boxes lc rgb \"%s\" t \"work stretch\" axes x1y1, "
          "\"-\" u 3 w linespoints lt 2 lc rgb \"orange\" t \"percentage\" axes x1y2, "
          "\"-\" u 0:3:4 w labels center offset 0,1 axes x1y2 notitle \n",
          max_y,
          DV_GRAPH_COLORS[0]
          );
  int j;
  for (j = 0; j < 3; j++) {
    for (i = 1; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double total_loss = (DMG->SBG->work[i] - DMG->SBG->work[0]) + (DMG->SBG->delay[i]) + (DMG->SBG->nowork[i]);
      double factor = (DMG->SBG->work[i] - DMG->SBG->work[0]);
      double percentage = (factor / total_loss) * 100.0;
      if (total_loss < 0) percentage = 0.0;
      fprintf(out,
              "\"%s\"  %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              factor,
              percentage,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph of work stretch to %s\n", filename);

  /* delay */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_delay.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%.0lf<*]\n"
          "set y2range [*<0:100<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "#set y2tics\n"
          "#set ytics nomirror\n"
          "#set y2label \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w boxes lc rgb \"%s\" t \"delay\" axes x1y1, "
          "\"-\" u 3 w linespoints lt 2 lc rgb \"orange\" t \"percentage\" axes x1y2, "
          "\"-\" u 0:3:4 w labels center offset 0,1 axes x1y2 notitle \n",
          max_y,
          DV_GRAPH_COLORS[1]
          );
  for (j = 0; j < 3; j++) {
    for (i = 1; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double total_loss = (DMG->SBG->work[i] - DMG->SBG->work[0]) + (DMG->SBG->delay[i]) + (DMG->SBG->nowork[i]);
      double factor = DMG->SBG->delay[i];
      double percentage = (factor / total_loss) * 100.0;
      if (total_loss < 0) percentage = 0.0;
      fprintf(out,
              "\"%s\"  %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              factor,
              percentage,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph of total delay to %s\n", filename);

  /* delay breakdown */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_delay_breakdown.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%.0lf<*]\n"
          "set y2range [*<0:100<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "#set y2tics\n"
          "#set ytics nomirror\n"
          "#set y2label \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram lc rgb \"%s\" t \"delay sd\" axes x1y1, "
          "\"-\" u 3 w histogram lc rgb \"%s\" t \"delay w\" axes x1y1, "
          "\"-\" u 4 w linespoints lt 2 lc rgb \"orange\" notitle axes x1y2, "
          "\"-\" u 0:4:5 w labels center offset 0,1 axes x1y2 notitle \n",
          max_y,
          DV_GRAPH_COLORS[1],
          DV_GRAPH_COLORS[10]
          );
  for (j = 0; j < 4; j++) {
    for (i = 1; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double total_loss = (DMG->SBG->work[i] - DMG->SBG->work[0]) + (DMG->SBG->delay[i]) + (DMG->SBG->nowork[i]);
      double factor = DMG->SBG->delay[i];
      double percentage = (factor / total_loss) * 100.0;
      if (total_loss < 0) percentage = 0.0;
      fprintf(out,
              "\"%s\"  %lf %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_delay,
              factor - D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_delay,
              percentage,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph to %s\n", filename);
  
  /* nowork */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_nowork.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%.0lf<*]\n"
          "set y2range [*<0:100<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "#set y2tics\n"
          "#set ytics nomirror\n"
          "#set y2label \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w boxes lc rgb \"%s\" t \"no-work\" axes x1y1, "
          "\"-\" u 3 w linespoints lt 2 lc rgb \"orange\" t \"percentage\" axes x1y2, "
          "\"-\" u 0:3:4 w labels center offset 0,1 axes x1y2 notitle \n",
          max_y,
          DV_GRAPH_COLORS[2]
          );
            
  for (j = 0; j < 3; j++) {
    for (i = 1; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double total_loss = (DMG->SBG->work[i] - DMG->SBG->work[0]) + (DMG->SBG->delay[i]) + (DMG->SBG->nowork[i]);
      double factor = DMG->SBG->nowork[i];
      double percentage = (factor / total_loss) * 100.0;
      if (total_loss < 0) percentage = 0.0;
      fprintf(out,
              "\"%s\"  %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              factor,
              percentage,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph of nowork by scheduler delay to %s\n", filename);
  
  /* nowork breakdown */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_nowork_breakdown.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key off #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%.0lf<*]\n"
          "set y2range [*<0:100<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "#set y2tics\n"
          "#set ytics nomirror\n"
          "#set y2label \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram lc rgb \"%s\" t \"no-work sd\" axes x1y1, "
          "\"-\" u 3 w histogram lc rgb \"%s\" t \"no-work w\" axes x1y1, "
          "\"-\" u 4 w linespoints lt 2 lc rgb \"orange\" notitle axes x1y2, "
          "\"-\" u 0:4:5 w labels center offset 0,1 axes x1y2 notitle \n",
          max_y,
          DV_GRAPH_COLORS[2],
          DV_GRAPH_COLORS[11]
          );
  for (j = 0; j < 4; j++) {
    for (i = 1; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double total_loss = (DMG->SBG->work[i] - DMG->SBG->work[0]) + (DMG->SBG->delay[i]) + (DMG->SBG->nowork[i]);
      double factor = DMG->SBG->nowork[i];
      double percentage = (factor / total_loss) * 100.0;
      if (total_loss < 0) percentage = 0.0;
      fprintf(out,
              "\"%s\"  %lf %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_nowork,
              factor - D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_nowork,
              percentage,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph of nowork by scheduler delay to %s\n", filename);
  
}

void
dp_stat_graph_performance_loss_factors_with_percentage_labels_to_base(char * filename_) {
  /* Get max y */
  int i;
  double max_y = 0.0;
  for (i = 0; i < DMG->nD; i++) {
    double total_loss = (DMG->SBG->work[i] - DMG->SBG->work[0]) + (DMG->SBG->delay[i]) + (DMG->SBG->nowork[i]);
    if (total_loss > max_y)
      max_y = total_loss;
  }

  FILE * out;
  char filename[1000];
  double base = DMG->SBG->work[0] + DMG->SBG->delay[0] + DMG->SBG->nowork[0];

  /* work stretch */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_work_stretch.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style fill solid 0.8 noborder\n"
          "set key inside #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%.0lf<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w boxes lc rgb \"%s\" t \"work stretch\", "
          "\"-\" u 0:2:3 w labels center offset 0,1 notitle \n",
          max_y,
          DV_GRAPH_COLORS[0]
          );
  int j;
  for (j = 0; j < 2; j++) {
    for (i = 1; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double factor = (DMG->SBG->work[i] - DMG->SBG->work[0]);
      double percentage = (factor / base) * 100.0;
      fprintf(out,
              "\"%s\"  %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              factor,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph of work stretch to %s\n", filename);

  /* delay */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_delay.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style fill solid 0.8 noborder\n"
          "set key inside #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%.0lf<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w boxes lc rgb \"%s\" t \"delay\", "
          "\"-\" u 0:2:3 w labels center offset 0,1 notitle \n",
          max_y,
          DV_GRAPH_COLORS[1]
          );
  for (j = 0; j < 2; j++) {
    for (i = 1; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double factor = DMG->SBG->delay[i];
      double percentage = (factor / base) * 100.0;
      fprintf(out,
              "\"%s\"  %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              factor,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph of total delay to %s\n", filename);

  /* delay breakdown */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_delay_breakdown.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key inside #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%.0lf<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram lc rgb \"%s\" t \"delay sd\", "
          "\"-\" u 3 w histogram lc rgb \"%s\" t \"delay w\", "
          "\"-\" u 0:($2+$3):4 w labels center offset 0,1 notitle \n",
          max_y,
          DV_GRAPH_COLORS[1],
          DV_GRAPH_COLORS[10]
          );
  for (j = 0; j < 3; j++) {
    for (i = 1; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double factor = DMG->SBG->delay[i];
      double percentage = (factor / base) * 100.0;
      fprintf(out,
              "\"%s\"  %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_delay,
              factor - D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_delay,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph to %s\n", filename);

  /* nowork */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_nowork.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style fill solid 0.8 noborder\n"
          "set key inside #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%.0lf<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w boxes lc rgb \"%s\" t \"no-work\", "
          "\"-\" u 0:2:3 w labels center offset 0,1 notitle \n",
          max_y,
          DV_GRAPH_COLORS[2]
          );
  for (j = 0; j < 2; j++) {
    for (i = 1; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double factor = DMG->SBG->nowork[i];
      double percentage = (factor / base) * 100.0;
      fprintf(out,
              "\"%s\"  %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              factor,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph of nowork by scheduler delay to %s\n", filename);
  
  /* nowork breakdown */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_nowork_breakdown.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key inside #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%.0lf<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"cumul. clocks\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram lc rgb \"%s\" t \"no-work sd\", "
          "\"-\" u 3 w histogram lc rgb \"%s\" t \"no-work w\", "
          "\"-\" u 0:($2+$3):4 w labels center offset 0,1 notitle \n",
          max_y,
          DV_GRAPH_COLORS[2],
          DV_GRAPH_COLORS[11]
          );
  for (j = 0; j < 3; j++) {
    for (i = 1; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double factor = DMG->SBG->nowork[i];
      double percentage = (factor / base) * 100.0;
      fprintf(out,
              "\"%s\"  %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_nowork,
              factor - D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_nowork,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph of nowork by scheduler delay to %s\n", filename);
  
}

void
dp_stat_graph_performance_loss_factor_percentages_with_labels(char * filename_) {
  int i, j;
  FILE * out;
  char filename[1000];

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

  /* work stretch */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_work_stretch.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style fill solid 0.8 noborder\n"
          "set key inside #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%lf<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w boxes lc rgb \"%s\" t \"work stretch\", "
          "\"-\" u 0:2:3 w labels center offset 0,1 notitle \n",
          max_y_percent,
          DV_GRAPH_COLORS[0]
          );
  for (j = 0; j < 2; j++) {
    for (i = 1; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double factor = (DMG->SBG->work[i] - DMG->SBG->work[0]);
      double percentage = (factor / base) * 100.0;
      fprintf(out,
              "\"%s\"  %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              percentage,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph to %s\n", filename);

  /* delay */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_delay.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style fill solid 0.8 noborder\n"
          "set key inside #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%lf<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w boxes lc rgb \"%s\" t \"delay\", "
          "\"-\" u 0:2:3 w labels center offset 0,1 notitle \n",
          max_y_percent,
          DV_GRAPH_COLORS[1]
          );
  for (j = 0; j < 2; j++) {
    for (i = 1; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double factor = DMG->SBG->delay[i];
      double percentage = (factor / base) * 100.0;
      fprintf(out,
              "\"%s\"  %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              percentage,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph to %s\n", filename);

  /* delay breakdown */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_delay_breakdown.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key inside #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%lf<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram lc rgb \"%s\" t \"delay sd\", "
          "\"-\" u 3 w histogram lc rgb \"%s\" t \"delay w\", "
          "\"-\" u 0:($2+$3):4 w labels center offset 0,1 notitle \n",
          max_y_percent,
          DV_GRAPH_COLORS[1],
          DV_GRAPH_COLORS[10]
          );
  for (j = 0; j < 3; j++) {
    for (i = 1; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double factor = DMG->SBG->delay[i] - D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_delay;
      double percentage = (factor / base) * 100.0;
      double percentage_2 = (D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_delay / base) * 100.0;
      fprintf(out,
              "\"%s\"  %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              percentage_2,
              percentage,
              percentage + percentage_2);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph to %s\n", filename);

  /* nowork */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_nowork.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style fill solid 0.8 noborder\n"
          "set key inside #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%lf<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w boxes lc rgb \"%s\" t \"no-work\", "
          "\"-\" u 0:2:3 w labels center offset 0,1 notitle \n",
          max_y_percent,
          DV_GRAPH_COLORS[2]
          );
  for (j = 0; j < 2; j++) {
    for (i = 1; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double factor = DMG->SBG->nowork[i];
      double percentage = (factor / base) * 100.0;
      fprintf(out,
              "\"%s\"  %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              percentage,
              percentage);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph to %s\n", filename);

  /* nowork breakdown */
  strcpy(filename, filename_);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s_nowork_breakdown.gpl", filename);
  out = dp_open_file(filename);
  if (!out) return;
  fprintf(out,
          "#set terminal png font arial 14 size 640,350\n"
          "#set terminal postscript eps enhanced color size 10cm,5cm\n"
          "#set output ~/Desktop/00dv_stat_breakdown.png\n"
          "set style data histograms\n"
          "set style histogram rowstacked\n"
          "set style fill solid 0.8 noborder\n"
          "set key inside #outside center top horizontal\n"
          "set boxwidth 0.85 relative\n"
          "set yrange [*<0:%lf<*]\n"
          "#set xtics rotate by -20\n"
          "set ylabel \"percent\"\n"
          "plot "
          "\"-\" u 2:xtic(1) w histogram lc rgb \"%s\" t \"no-work sd\", "
          "\"-\" u 3 w histogram lc rgb \"%s\" t \"no-work w\", "
          "\"-\" u 0:($2+$3):4 w labels center offset 0,1 notitle \n",
          max_y_percent,
          DV_GRAPH_COLORS[2],
          DV_GRAPH_COLORS[11]
          );
  for (j = 0; j < 3; j++) {
    for (i = 1; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      double factor = DMG->SBG->nowork[i] - D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_nowork;
      double percentage = (factor / base) * 100.0;
      double percentage_2 = (D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_nowork / base) * 100.0;
      fprintf(out,
              "\"%s\"  %lf %lf \"%.1lf%%\"\n",
              D->name_on_graph,
              percentage_2,
              percentage,
              percentage + percentage_2);
    }
    fprintf(out, "e\n");
  }
  fprintf(out, "pause -1\n");
  dp_close_file(out);
  fprintf(stdout, "generated graph to %s\n", filename);

}

int
main(int argc, char ** argv) {
  /* Initialization */
  const char * dp_prefix = getenv("DP_PREFIX");
  if (!dp_prefix)
    dp_prefix = DEFAULT_BASE_DIR;
  printf("dp_prefix = %s\n", dp_prefix);

  /* GTK */
  gtk_init(&argc, &argv);

  /* CS, GUI */
  dp_global_state_init_nogtk(DVG);
  dv_gui_init(GUI);

  /* PIDAG */
  int i;
  for (i = 1; i < argc; i++) {
    glob_t globbuf;
    glob(argv[i], GLOB_TILDE | GLOB_PERIOD | GLOB_BRACE, NULL, &globbuf);
    int j;
    for (j = 0; j < (int) globbuf.gl_pathc; j++) {
      _unused_ dm_pidag_t * P = dv_create_new_pidag(globbuf.gl_pathv[j]);
    }
    if (globbuf.gl_pathc > 0)
      globfree(&globbuf);
  }
  for (i = 0; i < DMG->nP; i++) {
    dm_pidag_t * P = &DMG->P[i];
    _unused_ dm_dag_t * D = dv_create_new_dag(P);
    //_unused_ dv_view_t * V = dv_create_new_view(D);
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
  sprintf(command, "mkdir -p %s", dp_prefix);
  int err = system(command);
  if (err == -1) {
    printf("Error: cannot make dir of %s\n", dp_prefix);
    exit(1);
  }
  

  /* Graphs */
  char filename[1000];
  
  sprintf(filename, "%s/%s", dp_prefix, "execution_time_breakdown.gpl");
  dp_stat_graph_execution_time_breakdown(filename);
  //dp_call_gnuplot(filename);

  
  sprintf(filename, "%s/%s", dp_prefix, "critical_path_breakdown_0.gpl");
  dp_stat_graph_critical_path_breakdown(filename);
  //dp_call_gnuplot(filename);
  
  sprintf(filename, "%s/%s", dp_prefix, "critical_path_breakdown_1.gpl");
  dp_stat_graph_critical_path_delay_breakdown(filename);
  //dp_call_gnuplot(filename);
  
  sprintf(filename, "%s/%s", dp_prefix, "critical_path_breakdown_2.gpl");
  dp_stat_graph_critical_path_edge_based_delay_breakdown(filename);
  //dp_call_gnuplot(filename);

  
  sprintf(filename, "%s/%s", dp_prefix, "performance_loss_factor_with_lines.gpl");
  dp_stat_graph_performance_loss_factors_with_percentage_lines(filename);

  sprintf(filename, "%s/%s", dp_prefix, "performance_loss_factor_with_labels_to_base.gpl");
  dp_stat_graph_performance_loss_factors_with_percentage_labels_to_base(filename);

  sprintf(filename, "%s/%s", dp_prefix, "performance_loss_factor_percentages.gpl");
  dp_stat_graph_performance_loss_factor_percentages_with_labels(filename);


  /* Finalization */  
  return 1;
}
