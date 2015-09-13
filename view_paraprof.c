#include "dagviz.h"

/*-----------HISTOGRAM----------------------*/

void
dv_histogram_init(dv_histogram_t * H) {
  H->head_e = NULL;
  H->tail_e = NULL;
  H->n_e = 0;
  H->D = NULL;
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
}

static long int count_insert_entry;
static long int count_pile_entry;
static long int count_pile;
static long int count_add_node;

double
dv_histogram_get_max_height(dv_histogram_t * H) {
  double max_h = 0.0;
  dv_histogram_entry_t * e = H->head_e;
  while (e != NULL) {
    double h = 0.0;
    int i;
    for (i = 0; i < dv_histogram_layer_max; i++) {
      h += e->h[i];
    }
    if (h > max_h)
      max_h = h;
    e = e->next;
  }
  return max_h;
}

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
}

static double
dv_histogram_draw_piece(_unused_ dv_histogram_t * H, dv_view_t * V, cairo_t * cr, double x, double width, double y, double height, int layer) {
  cairo_save(cr);
  
  /* Color */
  GdkRGBA color;
  gdk_rgba_parse(&color, DV_HISTOGRAM_COLORS[(layer + 6) % 6]);
  
  /* Draw */
  double xx, yy, w, h;
  xx = x;
  yy = y - height;
  w = width;
  h = height;
  if (V) {
    double bound_left = dv_view_clip_get_bound_left(V);
    double bound_right = dv_view_clip_get_bound_right(V);
    double bound_up = dv_view_clip_get_bound_up(V);
    double bound_down = dv_view_clip_get_bound_down(V);
    if (xx < bound_right && xx + w > bound_left &&
        yy < bound_down && yy + h > bound_up) {
      if (xx < bound_left) {
        w -= (bound_left - xx);
        xx = bound_left;
      }
      if (xx + w > bound_right)
        w = bound_right - xx;
      if (yy < bound_up) {
        h -= (bound_up - yy);
        yy = bound_up;
      }
      if (yy + h > bound_down)
        h = bound_down - yy;
      cairo_rectangle(cr, xx, yy, w, h);
      cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
      cairo_fill(cr);
    }
  } else {
    cairo_rectangle(cr, xx, yy, w, h);
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    cairo_fill(cr);    
  }
  
  cairo_restore(cr);
  return y - h;
}

static void
dv_histogram_draw_entry(dv_histogram_t * H, dv_histogram_entry_t * e, cairo_t * cr, dv_view_t * V) {
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
    y = dv_histogram_draw_piece(H, V, cr, x, w, y, h, i);
  }
}

static void
dv_histogram_cal_work_delay_nowork(dv_histogram_t * H) {
  H->work = H->delay = H->nowork = 0.0;
  double work_p, delay_p, nowork_p;
  double p_bound = H->D->P->num_workers * 2 * H->D->radius;
  dv_histogram_entry_t * e = H->head_e;
  while (e != NULL && e->next) {
    int layer = dv_histogram_layer_running;
    work_p = e->h[layer];
    double p = 0.0;
    while (layer < dv_histogram_layer_max) {
      p += e->h[layer++];
    }
    if (p < p_bound) {
      nowork_p = p_bound - p;
      delay_p = p - work_p;
    } else {
      nowork_p = 0.0;
      delay_p = p_bound - work_p;
    }
    double interval = (e->next->t - e->t) / H->D->linear_radix;
    H->work += interval * ( work_p / (2 * H->D->radius) );
    H->delay += interval * ( delay_p / (2 * H->D->radius) );
    H->nowork += interval * ( nowork_p / (2 * H->D->radius) );
    e = e->next;
  }
  H->work /= H->D->P->num_workers;
  H->delay /= H->D->P->num_workers;
  H->nowork /= H->D->P->num_workers;
  //printf("  work,delay,nowork = %10lf, %10lf, %10lf\n", H->work, H->delay, H->nowork);
}

void
dv_histogram_draw(dv_histogram_t * H, cairo_t * cr, dv_view_t * V) {
  dv_histogram_entry_t * e = H->head_e;
  while (e != NULL && e->next) {
    dv_histogram_draw_entry(H, e, cr, V);
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
  if (H->head_e)
    dv_histogram_fini(H);
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

static void
dv_paraprof_draw_time_bar(dv_view_t * V, dv_histogram_t * H, cairo_t * cr) {
  double x = dv_dag_scale_down(H->D, H->D->current_time);
  double y1 = - dv_histogram_get_max_height(H) - H->D->radius;
  double y2 = H->D->P->num_workers * (2 * H->D->radius) + H->D->radius;
  cairo_save(cr);
  cairo_move_to(cr, x, y1);
  cairo_line_to(cr, x, y2);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 0.8);
  cairo_set_line_width(cr, (DV_NODE_LINE_WIDTH * 2) / V->S->zoom_ratio_x);
  cairo_stroke(cr);
  cairo_restore(cr);
}

void
dv_view_draw_paraprof(dv_view_t * V, cairo_t * cr) {
  dv_view_draw_timeline(V, cr);
  if (V->D->H)
    dv_histogram_draw(V->D->H, cr, V);
  if (V->D->draw_with_current_time)
    dv_paraprof_draw_time_bar(V, V->D->H, cr);
}

/*-----------------end of PARAPROF drawing functions-----------*/
