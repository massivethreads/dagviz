#include "dagstat.h"

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

void
dp_stat_graph_performance_loss_factor_percentages_with_labels(char * filename_) {
  int i, j;
  FILE * out;
  char filename[1000];

  /* Get max y */
  double max_y = 0.0;
  double max_y_percent = 0.0;
  double base = CS->SBG->work[0] + CS->SBG->delay[0] + CS->SBG->nowork[0];
  for (i = 0; i < CS->nD; i++) {
    double total_loss = (CS->SBG->work[i] - CS->SBG->work[0]) + (CS->SBG->delay[i]) + (CS->SBG->nowork[i]);
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
    for (i = 1; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      double factor = (CS->SBG->work[i] - CS->SBG->work[0]);
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
    for (i = 1; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      double factor = CS->SBG->delay[i];
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
    for (i = 1; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      double factor = CS->SBG->delay[i] - D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_delay;
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
    for (i = 1; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      double factor = CS->SBG->nowork[i];
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
    for (i = 1; i < CS->nD; i++) {
      dv_dag_t * D = &CS->D[i];
      double factor = CS->SBG->nowork[i] - D->rt->cpss[DV_CRITICAL_PATH_1].sched_delay_nowork;
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

dv_pidag_t *
dv_create_new_pidag(char * filename) {
  if (!filename) return NULL;
  dv_pidag_t * P = dv_pidag_read_new_file(filename);
  if (!P) return NULL;

  P->name = malloc( sizeof(char) * 20 );
  sprintf(P->name, "DAG file %ld", P - CS->P);
  
  return P;
}

dv_dag_t *
dv_create_new_dag(dv_pidag_t * P) {
  if (!P) return NULL;
  dv_dag_t * D = dv_dag_create_new_with_pidag(P);
  if (!D) return NULL;
  
  return D;
}

int
main(int argc, char ** argv) {
  /* Init */
  const char * dp_prefix = getenv("DP_PREFIX");
  if (!dp_prefix)
    dp_prefix = DEFAULT_BASE_DIR;
  printf("dp_prefix = %s\n", dp_prefix);

  dv_global_state_init(CS);
  
  /* PIDAG */
  int i;
  for (i = 1; i < argc; i++) {
    glob_t globbuf;
    glob(argv[i], GLOB_TILDE | GLOB_PERIOD | GLOB_BRACE, NULL, &globbuf);
    int j;
    for (j = 0; j < (int) globbuf.gl_pathc; j++) {
      _unused_ dv_pidag_t * P = dv_create_new_pidag(globbuf.gl_pathv[j]);
    }
    if (globbuf.gl_pathc > 0)
      globfree(&globbuf);
  }
  for (i = 0; i < CS->nP; i++) {
    dv_pidag_t * P = &CS->P[i];
    _unused_ dv_dag_t * D = dv_create_new_dag(P);
  }

  /* Preparation */
  for (i = 0; i < CS->nD; i++) {
    dv_dag_t * D = &CS->D[i];

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
  for (i = 0; i < CS->nD; i++) {
    dv_dag_t * D = &CS->D[i];
    dv_dag_compute_critical_paths(D);
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
  
  sprintf(filename, "%s/%s", dp_prefix, "performance_loss_factor_percentages.gpl");
  dp_stat_graph_performance_loss_factor_percentages_with_labels(filename);

  return 1;
}
