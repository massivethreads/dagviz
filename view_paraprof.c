#include "dagviz.h"

/*-----------HISTOGRAM----------------------*/

void
dv_histogram_init(dv_histogram_t * H) {
  H->head_e = NULL;
  H->tail_e = NULL;
  H->n_e = 0;
  H->D = NULL;
  H->max_e = NULL;
  H->work = H->delay = H->nowork = 0.0;
}

static void
dv_histogram_entry_init(dv_histogram_entry_t * e) {
  e->t = 0.0;
  int i;
  for (i=0; i<dv_histogram_layer_max; i++) {
    e->h[i] = 0.0;
  }
  e->next = NULL;
  e->sum_h = 0.0;
}

static long int count_insert_entry;
static long int count_pile_entry;
static long int count_pile;
static long int count_add_node;

static dv_histogram_entry_t *
dv_histogram_insert_entry(dv_histogram_t * H, double t) {
  count_insert_entry++;
  dv_histogram_entry_t * e = NULL;
  dv_histogram_entry_t * ee = H->head_e;
  while (ee != NULL && ee->t < t) {
    e = ee;
    ee = ee->next;
  }
  if (ee && ( (ee->t == t) || (e && (ee->t - e->t) < DV_PARAPROF_MIN_ENTRY_INTERVAL) ))
    return ee;
  dv_histogram_entry_t * new_e = dv_histogram_entry_pool_pop(CS->epool);
  if (!new_e) {
    fprintf(stderr, "Error: cannot pop a new entry structure!\n");
    dv_check(0);
  }
  dv_histogram_entry_init(new_e);
  new_e->t = t;
  new_e->next = ee;
  if (e) {
    e->next = new_e;
    // Copy h
    int i;
    for (i=0; i<dv_histogram_layer_max; i++) {
      new_e->h[i] = e->h[i];
    }
  } else {
    H->head_e = new_e;
  }
  if (ee) {
    new_e->next = ee;
  } else {
    H->tail_e = new_e;
  }
  H->n_e++;
  return new_e;
}

static void
dv_histogram_pile_entry(dv_histogram_t * H, dv_histogram_entry_t * e, dv_histogram_layer_t layer, double thick, dv_dag_node_t * node) {
  (void)H;
  (void)node;
  count_pile_entry++;
  e->h[layer] += thick;
  e->sum_h += thick;
}

static void
dv_histogram_pile(dv_histogram_t * H, dv_dag_node_t * node, dv_histogram_layer_t layer, double from_t, double to_t, double parallelism) {
  count_pile++;
  dv_histogram_entry_t * e_from = dv_histogram_insert_entry(H, from_t);
  dv_histogram_entry_t * e_to = dv_histogram_insert_entry(H, to_t);
  if (!e_from || !e_to)
    return;
  dv_histogram_entry_t * e = e_from;
  while (e != e_to) {
    dv_histogram_pile_entry(H, e, layer, parallelism * 2 * H->D->radius, node);
    if (!H->max_e || e->sum_h > H->max_e->sum_h)
      H->max_e = e;
    e = e->next;
  }
}

void
dv_histogram_add_node(dv_histogram_t * H, dv_dag_node_t * node) {
  count_add_node++;
  if (!H)
    return;
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(H->D->P, node);
  double first_ready_t = pi->info.first_ready_t;
  double start_t = pi->info.start.t;
  double last_start_t = pi->info.last_start_t;
  double end_t = pi->info.end.t;
  double dt = last_start_t - first_ready_t;
  dv_histogram_pile(H, node,
                    dv_histogram_layer_ready_end,
                    first_ready_t, last_start_t,
                    pi->info.t_ready[dr_dag_edge_kind_end] / dt);
  dv_histogram_pile(H, node,
                    dv_histogram_layer_ready_create,
                    first_ready_t, last_start_t,
                    pi->info.t_ready[dr_dag_edge_kind_create] / dt);
  dv_histogram_pile(H, node,
                    dv_histogram_layer_ready_create_cont,
                    first_ready_t, last_start_t,
                    pi->info.t_ready[dr_dag_edge_kind_create_cont] / dt);
  dv_histogram_pile(H, node,
                    dv_histogram_layer_ready_wait_cont,
                    first_ready_t, last_start_t,
                    pi->info.t_ready[dr_dag_edge_kind_wait_cont] / dt);
  dv_histogram_pile(H, node,
                    dv_histogram_layer_ready_other_cont,
                    first_ready_t, last_start_t,
                    pi->info.t_ready[dr_dag_edge_kind_other_cont] / dt);
  dv_histogram_pile(H, node,
                    dv_histogram_layer_running,
                    start_t, end_t,
                    pi->info.t_1 / (end_t - start_t) );
}

static void
dv_histogram_unpile_entry(dv_histogram_t * H, dv_histogram_entry_t * e, dv_histogram_layer_t layer, double thick, dv_dag_node_t * node) {
  (void)H;
  (void)node;
  e->h[layer] -= thick;
  e->sum_h -= thick;
}

static void
dv_histogram_unpile(dv_histogram_t * H, dv_dag_node_t * node, dv_histogram_layer_t layer, double from_t, double to_t, double parallelism) {
  count_pile++;
  dv_histogram_entry_t * e_from = dv_histogram_insert_entry(H, from_t);
  dv_histogram_entry_t * e_to = dv_histogram_insert_entry(H, to_t);
  if (!e_from || !e_to)
    return;
  dv_histogram_entry_t * e = e_from;
  while (e != e_to) {
    dv_histogram_unpile_entry(H, e, layer, parallelism * 2 * H->D->radius, node);
    if (!H->max_e || e->sum_h > H->max_e->sum_h)
      H->max_e = e;
    e = e->next;
  }
}

void
dv_histogram_remove_node(dv_histogram_t * H, dv_dag_node_t * node) {
  if (!H)
    return;
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(H->D->P, node);
  double first_ready_t = pi->info.first_ready_t;
  double start_t = pi->info.start.t;
  double last_start_t = pi->info.last_start_t;
  double end_t = pi->info.end.t;
  double dt = last_start_t - first_ready_t;
  dv_histogram_unpile(H, node,
                      dv_histogram_layer_ready_end,
                      first_ready_t, last_start_t,
                      pi->info.t_ready[dr_dag_edge_kind_end] / dt);
  dv_histogram_unpile(H, node,
                      dv_histogram_layer_ready_create,
                      first_ready_t, last_start_t,
                      pi->info.t_ready[dr_dag_edge_kind_create] / dt);
  dv_histogram_unpile(H, node,
                      dv_histogram_layer_ready_create_cont,
                      first_ready_t, last_start_t,
                      pi->info.t_ready[dr_dag_edge_kind_create_cont] / dt);
  dv_histogram_unpile(H, node,
                      dv_histogram_layer_ready_wait_cont,
                      first_ready_t, last_start_t,
                      pi->info.t_ready[dr_dag_edge_kind_wait_cont] / dt);
  dv_histogram_unpile(H, node,
                      dv_histogram_layer_ready_other_cont,
                      first_ready_t, last_start_t,
                      pi->info.t_ready[dr_dag_edge_kind_other_cont] / dt);
  dv_histogram_unpile(H, node,
                      dv_histogram_layer_running,
                      start_t, end_t,
                      pi->info.t_1 / (end_t - start_t) );
}

void
dv_histogram_fini(dv_histogram_t * H) {
  dv_histogram_entry_t * e = H->head_e;
  dv_histogram_entry_t * ee;
  long n = 0;
  while (e != NULL) {
    ee = e->next;
    dv_histogram_entry_pool_push(CS->epool, e);
    n++;
    e = ee;
  }
  dv_check(n == H->n_e);
  H->head_e = NULL;
  H->tail_e = NULL;
  H->n_e = 0;
  H->D = NULL;
  H->max_e = NULL;
}

static double
dv_histogram_draw_piece(dv_histogram_t * H, cairo_t * cr, double x, double w, double y, double h, int layer) {
  (void)H;
  cairo_save(cr);
  // Color
  GdkRGBA color;
  gdk_rgba_parse(&color, DV_HISTOGRAM_COLORS[(layer + 6) % 6]);
  // Draw
  cairo_rectangle(cr, x, y - h, w, h);
  cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
  cairo_fill(cr);
  cairo_restore(cr);
  return y - h;
}

static void
dv_histogram_draw_entry(dv_histogram_t * H, dv_histogram_entry_t * e, cairo_t * cr) {
  if (!e->next) {
    fprintf(stderr, "Warning: not draw entry at t=%lf due to no next\n", e->t);
    return;
  }
  double x = dv_dag_scale_down(H->D, e->t - H->D->bt);
  double w = dv_dag_scale_down(H->D, e->next->t - H->D->bt) - x;
  double y = 0.0;
  double h;
  int i;
  for (i=0; i<dv_histogram_layer_max; i++) {
    h = e->h[i];
    y = dv_histogram_draw_piece(H, cr, x, w, y, h, i);
  }
}

static void
dv_histogram_cal_work_delay_nowork(dv_histogram_t * H) {
  double work = 0.0;
  double delay = 0.0;
  double nowork = 0.0;
  dv_histogram_entry_t * e = H->head_e;
  while (e != NULL && e->next) {
    work += e->h[dv_histogram_layer_running];
    int layer = dv_histogram_layer_running;
    double p = 0.0;
    while (layer < dv_histogram_layer_max) {
      p += e->h[layer++];
    }
    double p_bound = H->D->P->num_workers * 2 * H->D->radius;
    if (p < p_bound) {
      nowork += p_bound - p;
      delay += p - e->h[dv_histogram_layer_running];
    } else {
      delay += p_bound - e->h[dv_histogram_layer_running];
    }
    double interval = e->next->t - e->t;
    H->work = dv_dag_scale_down(H->D, interval * ( work / (2 * H->D->radius) )) / H->D->P->num_workers;
    H->delay = dv_dag_scale_down(H->D, interval * ( delay / (2 * H->D->radius) )) / H->D->P->num_workers;
    H->nowork = dv_dag_scale_down(H->D, interval * ( nowork / (2 * H->D->radius) )) / H->D->P->num_workers;
    e = e->next;
  }
  printf("  work = %lf\n delay = %lf\nnowork = %lf\n", H->work, H->delay, H->nowork);
}

void
dv_histogram_draw(dv_histogram_t * H, cairo_t * cr) {
  dv_histogram_entry_t * e = H->head_e;
  while (e != NULL && e->next) {
    dv_histogram_draw_entry(H, e, cr);
    e = e->next;
  }
  dv_histogram_cal_work_delay_nowork(H);
}

static void
dv_histogram_reset_node(dv_histogram_t * H, dv_dag_node_t * node) {
  /* Calculate inward */
  if (dv_is_union(node) && dv_is_inner_loaded(node)) {
    // Recursive call
    if (dv_is_expanded(node) || dv_is_expanding(node)) {
      dv_histogram_reset_node(H, node->head);
    } else {
      dv_histogram_add_node(H, node);
    }
  } else {
    dv_histogram_add_node(H, node);
  }
    
  /* Calculate link-along */
  dv_dag_node_t * u, * v; // linked nodes
  switch ( dv_dag_node_count_nexts(node) ) {
  case 0:
    break;
  case 1:
    u = node->next;
    // Recursive call
    dv_histogram_reset_node(H, u);
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    // Recursive call
    dv_histogram_reset_node(H, u);
    dv_histogram_reset_node(H, v);
    break;
  default:
    dv_check(0);
    break;
  }  
}

void
dv_histogram_reset(dv_histogram_t * H) {
  dv_dag_t * D = H->D;
  dv_histogram_init(H);
  H->D = D;
  dv_histogram_reset_node(H, D->rt);
}

/*-----------end of HISTOGRAM----------------------*/


/*-----------PARAPROF layout functions----------------------*/

void
dv_view_layout_paraprof(dv_view_t * V) {
  dv_view_layout_timeline(V);
}

/*-----------end of PARAPROF layout functions----------------------*/


/*-----------------PARAPROF Drawing functions-----------*/

void
dv_view_draw_paraprof(dv_view_t * V, cairo_t * cr) {
  dv_view_draw_timeline(V, cr);
  if (V->D->H)
    dv_histogram_draw(V->D->H, cr);
}

/*-----------------end of PARAPROF drawing functions-----------*/
