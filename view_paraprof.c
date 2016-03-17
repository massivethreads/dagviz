#include "dagviz.h"

/****** HISTOGRAM ******/

void
dv_histogram_init(dv_histogram_t * H) {
  H->head_e = NULL;
  H->tail_e = NULL;
  H->n_e = 0;
  H->D = NULL;
  H->work = H->delay = H->nowork = 0.0;
  H->min_entry_interval = DV_PARAPROF_MIN_ENTRY_INTERVAL;
  H->unit_thick = 2.0;
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
  return max_h * (H->unit_thick * H->D->radius);
}

static dv_histogram_entry_t *
dv_histogram_insert_entry(dv_histogram_t * H, double t, dv_histogram_entry_t * e_hint) {
  dv_histogram_entry_t * e = NULL;
  dv_histogram_entry_t * ee = H->head_e;
  if (H->tail_e && (t > H->tail_e->t)) {
    e = H->tail_e;
    ee = NULL;
  } else {
    if (e_hint && (t > e_hint->t)) {
      e = e_hint;
      ee = e->next;
    }
    while (ee != NULL && ee->t < t) {
      e = ee;
      ee = ee->next;
    }
  }
  if (ee && ( (ee->t == t) || (e && (ee->t - e->t) < H->min_entry_interval) ))
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

/*
static void
dv_histogram_pile_entry(dv_histogram_t * H, dv_histogram_entry_t * e, dv_histogram_layer_t layer, double thick, dv_dag_node_t * node) {
  (void)H;
  (void)node;
  e->h[layer] += thick;
}

static void
dv_histogram_pile(dv_histogram_t * H, dv_dag_node_t * node, dv_histogram_layer_t layer, double from_t, double to_t, double parallelism) {
  dv_histogram_entry_t * e_from = dv_histogram_insert_entry(H, from_t, NULL);
  dv_histogram_entry_t * e_to = dv_histogram_insert_entry(H, to_t, NULL);
  if (!e_from || !e_to)
    return;
  dv_histogram_entry_t * e = e_from;
  while (e != e_to) {
    dv_histogram_pile_entry(H, e, layer, parallelism * H->unit_thick * H->D->radius, node);
    e = e->next;
  }
}
*/

void
dv_histogram_add_node(dv_histogram_t * H, dv_dag_node_t * node, dv_histogram_entry_t ** hint_entry) {
  if (!H)
    return;
  
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(H->D->P, node);
  double from_t, to_t, dt;
  dv_histogram_entry_t * e_from;
  dv_histogram_entry_t * e_to;
  dv_histogram_entry_t * e;

  from_t = pi->info.first_ready_t;
  to_t = pi->info.last_start_t;
  dt = to_t - from_t;
  e_from = dv_histogram_insert_entry(H, from_t, (hint_entry)?(*hint_entry):NULL);
  e_to = dv_histogram_insert_entry(H, to_t, e_from);
  dv_check(e_from && e_to);
  e = e_from;
  while (e != e_to) {
    int layer;
    layer = dv_histogram_layer_ready_end;
    e->h[layer] += (pi->info.t_ready[dr_dag_edge_kind_end] / dt);
    layer = dv_histogram_layer_ready_create;
    e->h[layer] += (pi->info.t_ready[dr_dag_edge_kind_create] / dt);
    layer = dv_histogram_layer_ready_create_cont;
    e->h[layer] += (pi->info.t_ready[dr_dag_edge_kind_create_cont] / dt);
    layer = dv_histogram_layer_ready_wait_cont;
    e->h[layer] += (pi->info.t_ready[dr_dag_edge_kind_wait_cont] / dt);
    layer = dv_histogram_layer_ready_other_cont;
    e->h[layer] += (pi->info.t_ready[dr_dag_edge_kind_other_cont] / dt);
    e = e->next;
  }

  from_t = pi->info.start.t;
  to_t = pi->info.end.t;
  dt = to_t - from_t;
  e_from = dv_histogram_insert_entry(H, from_t, e_from);
  e_to = dv_histogram_insert_entry(H, to_t, e_to);
  dv_check(e_from && e_to);
  e = e_from;
  while (e != e_to) {
    e->h[dv_histogram_layer_running] += (pi->info.t_1 / dt);
    e = e->next;
  }
  if (hint_entry) *hint_entry = e_to;
}

/*
static void
dv_histogram_unpile_entry(dv_histogram_t * H, dv_histogram_entry_t * e, dv_histogram_layer_t layer, double thick, dv_dag_node_t * node) {
  (void)H;
  (void)node;
  e->h[layer] -= thick;
}

static void
dv_histogram_unpile(dv_histogram_t * H, dv_dag_node_t * node, dv_histogram_layer_t layer, double from_t, double to_t, double parallelism) {
  dv_histogram_entry_t * e_from = dv_histogram_insert_entry(H, from_t, NULL);
  dv_histogram_entry_t * e_to = dv_histogram_insert_entry(H, to_t, e_from);
  if (!e_from || !e_to)
    return;
  dv_histogram_entry_t * e = e_from;
  while (e != e_to) {
    dv_histogram_unpile_entry(H, e, layer, parallelism * H->unit_thick * H->D->radius, node);
    e = e->next;
  }
}
*/

void
dv_histogram_remove_node(dv_histogram_t * H, dv_dag_node_t * node, dv_histogram_entry_t ** hint_entry) {
  if (!H)
    return;
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(H->D->P, node);
  double from_t, to_t, dt;
  dv_histogram_entry_t * e_from;
  dv_histogram_entry_t * e_to;
  dv_histogram_entry_t * e;
  
  from_t = pi->info.first_ready_t;
  to_t = pi->info.last_start_t;
  dt = to_t - from_t;
  e_from = dv_histogram_insert_entry(H, from_t, (hint_entry)?(*hint_entry):NULL);
  e_to = dv_histogram_insert_entry(H, to_t, e_from);
  dv_check(e_from && e_to);
  e = e_from;
  while (e != e_to) {
    int layer;
    layer = dv_histogram_layer_ready_end;
    e->h[layer] -= (pi->info.t_ready[dr_dag_edge_kind_end] / dt);
    layer = dv_histogram_layer_ready_create;
    e->h[layer] -= (pi->info.t_ready[dr_dag_edge_kind_create] / dt);
    layer = dv_histogram_layer_ready_create_cont;
    e->h[layer] -= (pi->info.t_ready[dr_dag_edge_kind_create_cont] / dt);
    layer = dv_histogram_layer_ready_wait_cont;
    e->h[layer] -= (pi->info.t_ready[dr_dag_edge_kind_wait_cont] / dt);
    layer = dv_histogram_layer_ready_other_cont;
    e->h[layer] -= (pi->info.t_ready[dr_dag_edge_kind_other_cont] / dt);
    e = e->next;
  }
  
  from_t = pi->info.start.t;
  to_t = pi->info.end.t;
  dt = to_t - from_t;
  e_from = dv_histogram_insert_entry(H, from_t, e_from);
  e_to = dv_histogram_insert_entry(H, to_t, e_to);
  dv_check(e_from && e_to);
  e = e_from;
  while (e != e_to) {
    e->h[dv_histogram_layer_running] -= (pi->info.t_1 / dt);
    e = e->next;
  }
  if (hint_entry) *hint_entry = e_to;
}

void
dv_histogram_clean(dv_histogram_t * H) {
  printf("dv_histogram_clean()\n");
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
}

void
dv_histogram_fini(dv_histogram_t * H) {
  dv_histogram_clean(H);
  H->D = NULL;
}

_unused_ static int
dv_histogram_entry_is_invisible(dv_histogram_t * H, dv_view_t * V, dv_histogram_entry_t * e) {
  int ret = -1;
  if (!H || !V || !e) return ret;

  double x, y, w, h;
  x = dv_dag_scale_down_linear(H->D, e->t - H->D->bt);
  w = 0.0;
  if (e->next)
    w = dv_dag_scale_down_linear(H->D, e->next->t - H->D->bt) - x;
  h = 0.0;
  int i;
  for (i = 0; i < dv_histogram_layer_max; i++)
    h += e->h[i];
  y = -h;
  ret = dv_rectangle_is_invisible(V, x, y, w, h);
  return ret;  
}

_unused_ static int
dv_histogram_entry_is_invisible_fast(dv_histogram_t * H, dv_view_t * V, dv_histogram_entry_t * e) {
  double x, w;
  x = dv_dag_scale_down_linear(H->D, e->t - H->D->bt);
  w = 0.0;
  if (e->next)
    w = dv_dag_scale_down_linear(H->D, e->next->t - H->D->bt) - x;
  double bound_left = dv_view_clip_get_bound_left(V);
  double bound_right = dv_view_clip_get_bound_right(V);
  int ret = 0;
  if (x > bound_right)
    ret |= DV_DAG_NODE_HIDDEN_RIGHT;
  if (x + w < bound_left)
    ret |= DV_DAG_NODE_HIDDEN_LEFT;
  return ret;  
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
    if (!dv_rectangle_is_invisible(V, xx, yy, w, h)) {
      dv_timeline_trim_rectangle(V, &xx, &yy, &w, &h);
      cairo_rectangle(cr, xx, yy, w, h);
      cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
      cairo_fill(cr);
    }
  } else {
    fprintf(stderr, "Warning: There is no V for dv_histogram_draw_piece().\n");
    cairo_rectangle(cr, xx, yy, w, h);
    cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
    cairo_fill(cr);
  }
  
  cairo_restore(cr);
  return yy;
}

static void
dv_histogram_draw_entry(dv_histogram_t * H, dv_histogram_entry_t * e, cairo_t * cr, dv_view_t * V) {
  if (!e->next) {
    fprintf(stderr, "Warning: not draw entry at t=%lf due to no next\n", e->t);
    return;
  }
  double x = dv_dag_scale_down_linear(H->D, e->t - H->D->bt);
  double w = dv_dag_scale_down_linear(H->D, e->next->t - H->D->bt) - x;
  //double y = 0.0;
  double y = - H->unit_thick * H->D->radius;
  double h;
  int i;
  for (i=0; i<dv_histogram_layer_max; i++) {
    h = e->h[i] * H->unit_thick * H->D->radius;
    y = dv_histogram_draw_piece(H, V, cr, x, w, y, h, i);
  }
}

_unused_ static void
dv_histogram_cal_work_delay_nowork(dv_histogram_t * H) {
  H->work = H->delay = H->nowork = 0.0;
  double work_p, delay_p, nowork_p;
  double p_bound = H->D->P->num_workers * H->unit_thick * H->D->radius;
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
    H->work += interval * ( work_p / (H->unit_thick * H->D->radius) );
    H->delay += interval * ( delay_p / (H->unit_thick * H->D->radius) );
    H->nowork += interval * ( nowork_p / (H->unit_thick * H->D->radius) );
    e = e->next;
  }
  H->work /= H->D->P->num_workers;
  H->delay /= H->D->P->num_workers;
  H->nowork /= H->D->P->num_workers;
  //printf("  work,delay,nowork = %10lf, %10lf, %10lf\n", H->work, H->delay, H->nowork);
}

void
dv_histogram_draw(dv_histogram_t * H, cairo_t * cr, dv_view_t * V) {
  double time = dv_get_time();
  if (CS->verbose_level >= 2) {
    fprintf(stderr, "dv_histogram_draw()\n");
  }
  cairo_save(cr);
  cairo_new_path(cr);
  dv_histogram_entry_t * e = H->head_e;
  while (e != NULL && e->next) {
    int hidden = dv_histogram_entry_is_invisible_fast(H, V, e);
    if (!hidden)
      dv_histogram_draw_entry(H, e, cr, V);
    else if (hidden & DV_DAG_NODE_HIDDEN_RIGHT)
      break;      
    e = e->next;
  }
  
  /* draw full-parallelism line */
  double x = 0;
  double w = dv_dag_scale_down_linear(H->D, H->D->et);
  double y = - H->unit_thick * H->D->radius - H->D->P->num_workers * (H->unit_thick * H->D->radius);
  dv_rectangle_trim(V, &x, &y, &w, NULL);
  cairo_save(cr);
  cairo_new_path(cr);
  cairo_move_to(cr, x, y);
  cairo_line_to(cr, x + w, y);
  cairo_set_source_rgba(cr, 1.0, 0.0, 0.0, 0.8);
  cairo_set_line_width(cr, DV_NODE_LINE_WIDTH / V->S->zoom_ratio_x);
  cairo_stroke(cr);
  cairo_restore(cr);
  
  //dv_histogram_cal_work_delay_nowork(H);
  if (CS->verbose_level >= 2) {
    fprintf(stderr, "... done dv_histogram_draw(): %lf\n", dv_get_time() - time);
  }
  cairo_restore(cr);
}

static void
dv_histogram_reset_node(dv_histogram_t * H, dv_dag_node_t * node, dv_histogram_entry_t * e_hint_) {
  /* Calculate inward */
  dv_histogram_entry_t * e_hint = e_hint_;
  if (dv_is_union(node) && dv_is_inner_loaded(node)) {
    // Recursive call
    if (dv_is_expanded(node) || dv_is_expanding(node)) {
      dv_histogram_reset_node(H, node->head, e_hint);
    } else {
      dv_histogram_add_node(H, node, &e_hint);
    }
  } else {
    dv_histogram_add_node(H, node, &e_hint);
  }
    
  /* Calculate link-along */
  dv_dag_node_t * u, * v; // linked nodes
  switch ( dv_dag_node_count_nexts(node) ) {
  case 0:
    break;
  case 1:
    u = node->next;
    dv_histogram_reset_node(H, u, e_hint);
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    dv_histogram_reset_node(H, u, e_hint);
    dv_histogram_reset_node(H, v, e_hint);
    break;
  default:
    dv_check(0);
    break;
  }  
}

void
dv_histogram_reset(dv_histogram_t * H) {
  double time = dv_get_time();
  if (CS->verbose_level >= 1) {
    fprintf(stderr, "dv_histogram_reset()\n");
  }
  dv_histogram_clean(H);
  if (H->D) 
    dv_histogram_reset_node(H, H->D->rt, NULL);
  if (CS->verbose_level >= 1) {
    fprintf(stderr, "... done dv_histogram_reset(): %lf ms\n", dv_get_time() - time);
  }
}

/****** end of HISTOGRAM ******/


/****** PARAPROF Layout ******/

void
dv_view_layout_paraprof(dv_view_t * V) {
  dv_view_layout_timeline(V);
}

/****** end of PARAPROF Layout ******/


/****** PARAPROF Draw ******/

static void
dv_paraprof_draw_time_bar(dv_view_t * V, dv_histogram_t * H, cairo_t * cr) {
  double x = dv_dag_scale_down_linear(H->D, H->D->current_time);
  double y1 = - dv_histogram_get_max_height(H) - H->D->radius;
  double y2 = H->D->P->num_workers * (H->unit_thick * H->D->radius) + H->D->radius;
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

/****** end of PARAPROF Draw ******/
