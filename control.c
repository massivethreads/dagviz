#include "dagviz.h"


static char *
dv_filename_get_short_name(char * fn) {
  char * p_from = strrchr(fn, '/');
  char * p_to = strrchr(fn, '.');
  if (!p_from)
    p_from = fn;
  else
    p_from++;
  if (!p_to) p_to = fn + strlen(fn);
  int n = p_to - p_from;
  char * ret = (char *) dv_malloc( sizeof(char) * n + 1 );
  strncpy(ret, p_from, n);
  return ret;
}

static void
dv_dag_collect_delays_r(dv_dag_t * D, dv_dag_node_t * node, FILE * out, dv_stat_distribution_entry_t * e) {
  if (!node)
    dv_check(node);

  if (dv_is_union(node) && dv_is_inner_loaded(node)) {
    dv_dag_collect_delays_r(D, node->head, out, e);
  } else {
    dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
    if (pi->info.kind == dr_dag_node_kind_create_task) {
      dr_pi_dag_node * pi_;
      if (e->type == 0)
        pi_ = dv_pidag_get_node_by_dag_node(D->P, node->spawn);
      else
        pi_ = dv_pidag_get_node_by_dag_node(D->P, node->next);
      dv_check(pi_);
      if (!e->stolen || pi_->info.worker != pi->info.worker)
        fprintf(out, "%lld\n", pi_->info.start.t - pi->info.end.t);
    }
  }

  dv_dag_node_t * next = NULL;
  while (next = dv_dag_node_traverse_nexts(node, next)) {
    dv_dag_collect_delays_r(D, next, out, e);
  }
}

