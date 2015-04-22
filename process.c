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

static void
dv_dag_collect_sync_delays_r(dv_dag_t * D, dv_dag_node_t * node, FILE * out, dv_stat_distribution_entry_t * e) {
  if (!node)
    dv_check(node);

  if (dv_is_union(node) && dv_is_inner_loaded(node)) {
    dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
    if (pi->info.kind == dr_dag_node_kind_section) {
      dr_pi_dag_node * pi_ = dv_pidag_get_node_by_dag_node(D->P, node->next);
      if (pi_) {
        dr_clock_t last_t = 0;
        dv_dag_node_t * x = NULL;
        while (x = dv_dag_node_traverse_tails(node, x)) {
          dr_pi_dag_node * x_pi = dv_pidag_get_node_by_dag_node(D->P, x);
          if (x_pi->info.end.t > last_t)
            last_t = x_pi->info.end.t;
        }
        dv_check(last_t);
        fprintf(out, "%lld\n", pi_->info.start.t - last_t);
      }
    }
    dv_dag_collect_sync_delays_r(D, node->head, out, e);
  }

  dv_dag_node_t * next = NULL;
  while (next = dv_dag_node_traverse_nexts(node, next)) {
    dv_dag_collect_sync_delays_r(D, next, out, e);
  }
}

static void
dv_dag_collect_intervals_r(dv_dag_t * D, dv_dag_node_t * node, FILE * out, dv_stat_distribution_entry_t * e) {
  if (!node)
    dv_check(node);

  if (dv_is_union(node) && dv_is_inner_loaded(node)) {
    dv_dag_collect_intervals_r(D, node->head, out, e);
  } else if (!dv_is_union(node) || !dv_is_inner_loaded(node)) {
    dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
    fprintf(out, "%lld\n", pi->info.end.t - pi->info.start.t);
  }

  dv_dag_node_t * next = NULL;
  while (next = dv_dag_node_traverse_nexts(node, next)) {
    dv_dag_collect_intervals_r(D, next, out, e);
  }
}

static void
dv_dag_expand_implicitly_r(dv_dag_t * D, dv_dag_node_t * node) {
  if (!dv_is_set(node))
    dv_dag_node_set(D, node);
  
  if (dv_is_union(node)) {

    /* Build inner */
    if ( !dv_is_inner_loaded(node) ) {
      if (dv_dag_build_node_inner(D, node) != DV_OK) {
        fprintf(stderr, "error in dv_dag_build_node_inner\n");
        return;
      }
    }
    /* Call inward */
    dv_check(node->head);
    dv_dag_expand_implicitly_r(D, node->head);
    
  }
  
  /* Call link-along */
  dv_dag_node_t * next = NULL;
  while (next = dv_dag_node_traverse_nexts(node, next)) {
    dv_dag_expand_implicitly_r(D, next);
  }
}

static void
dv_dag_expand_implicitly(dv_dag_t * D) {
  dv_dag_expand_implicitly_r(D, D->rt);
}

static void
dv_dag_set_status_label(dv_dag_t * D, GtkWidget * label) {
  char str[100];
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, D->rt);
  sprintf(str, " nodes: %ld/%ld (%ldMB), elasped: %llu clocks", D->n, D->P->n, D->P->sz / (1 << 20), pi->info.end.t - pi->info.start.t);
  gtk_label_set_text(GTK_LABEL(label), str);
}


static void
dv_dag_node_pool_set_status_label(dv_dag_node_pool_t * pool, GtkWidget * label) {
  char str[100];
  sprintf(str, "Node Pool: %ld / %ld (%ldMB)", pool->n, pool->N, pool->sz / (1 << 20) );
  gtk_label_set_text(GTK_LABEL(label), str);
}
