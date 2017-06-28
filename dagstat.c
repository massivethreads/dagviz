#include "dagstat.h"

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
ds_create_new_pidag(char * filename) {
  if (!filename) return NULL;
  dm_pidag_t * P = dm_pidag_read_new_file(filename);
  if (!P) return NULL;

  P->name = malloc( sizeof(char) * 20 );
  sprintf(P->name, "DAG file %ld", P - DMG->P);
  
  return P;
}

dm_dag_t *
ds_create_new_dag(dm_pidag_t * P) {
  if (!P) return NULL;
  dm_dag_t * D = dm_dag_create_new_with_pidag(P);
  if (!D) return NULL;
  
  return D;
}

void
ds_export_dpstat(int i) {
  dm_dag_t * D = &DMG->D[i];

  /* export target */
  char filename[1000];
  strcpy(filename, D->P->filename);
  filename[strlen(filename) - 4] = '\0';
  sprintf(filename, "%s.dpstat", filename);
  FILE * out = ds_open_file(filename);

  /* write out */
  double work = DMG->SBG->work[i];
  double delay = DMG->SBG->delay[i];
  double nowork = DMG->SBG->nowork[i];
  double nowork_sched = D->rt->cpss[DM_CRITICAL_PATH_1].sched_delay_nowork;
  double nowork_app = nowork - nowork_sched;
  double total = work + delay + nowork;
  fprintf(out,
          "total          = %.0lf (100%%)\n"
          "work           = %.0lf (%.2lf%%)\n"
          "delay          = %.0lf (%.2lf%%)\n"
          "no-work        = %.0lf (%.2lf%%)\n"
          "sched. no-work = %.0lf (%.2lf%%)\n"
          "app. no-work   = %.0lf (%.2lf%%)\n",
          total,
          work, work * 100.0 / total,
          delay, delay * 100.0 / total,
          nowork, nowork * 100.0 / total,
          nowork_sched, nowork_sched * 100.0 / total,
          nowork_app, nowork_app * 100.0 / total
          );
  ds_close_file(out);
  fprintf(stdout, "generated %s\n", filename);
}

int
main(int argc, char ** argv) {
  /* Init */
  dm_global_state_init();
  
  /* PIDAG */
  int i;
  for (i = 1; i < argc; i++) {
    glob_t globbuf;
    glob(argv[i], GLOB_TILDE | GLOB_PERIOD | GLOB_BRACE, NULL, &globbuf);
    int j;
    for (j = 0; j < (int) globbuf.gl_pathc; j++) {
      _unused_ dm_pidag_t * P = ds_create_new_pidag(globbuf.gl_pathv[j]);
    }
    if (globbuf.gl_pathc > 0)
      globfree(&globbuf);
  }
  for (i = 0; i < DMG->nP; i++) {
    dm_pidag_t * P = &DMG->P[i];
    _unused_ dm_dag_t * D = ds_create_new_dag(P);
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

  /* Export .dpstat files */
  for (i = 0; i < DMG->nD; i++) {
    ds_export_dpstat(i);
  }

  return 1;
}
