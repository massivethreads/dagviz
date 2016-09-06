#include "dagviz.h"

/****** HISTOGRAM ******/

void
dv_histogram_init(dv_histogram_t * H) {
  memset(H, 0, sizeof(dv_histogram_t));
  H->min_entry_interval = DV_PARAPROF_MIN_ENTRY_INTERVAL;
  H->unit_thick = 2.0;
#if 0  
  H->head_e = NULL;
  H->tail_e = NULL;
  H->n_e = 0;
  H->D = NULL;
  H->work = H->delay = H->nowork = 0.0;
  H->min_entry_interval = DV_PARAPROF_MIN_ENTRY_INTERVAL;
  H->unit_thick = 2.0;
#endif  
}

static void
dv_histogram_entry_init(dv_histogram_entry_t * e) {
  e->t = 0.0;
  int i;
  for (i=0; i<dv_histogram_layer_max; i++) {
    e->h[i] = 0.0;
  }
  e->next = NULL;
  e->value_1 = 0.0;
  e->value_2 = 0.0;
  e->value_3 = 0.0;
  e->cumul_value_1 = 0.0;
  e->cumul_value_2 = 0.0;
  e->cumul_value_3 = 0.0;
}

static void
dv_histogram_compute_tallest_entry(dv_histogram_t * H) {
  H->max_h = 0.0;
  dv_histogram_entry_t * e = H->head_e;
  while (e != NULL) {
    double h = 0.0;
    int i;
    for (i = 0; i < dv_histogram_layer_max; i++) {
      h += e->h[i];
    }
    if (h > H->max_h) {
      H->max_h = h;
      H->tallest_e = e;
    }
    e = e->next;
  }
}

double
dv_histogram_get_max_height(dv_histogram_t * H) {
  if (!H->tallest_e || H->max_h == 0.0) {
    dv_histogram_compute_tallest_entry(H);
  }    
  return H->max_h * (H->unit_thick * H->D->radius);
}

dv_histogram_entry_t *
dv_histogram_insert_entry(dv_histogram_t * H, double t, dv_histogram_entry_t * e_hint) {
  dv_histogram_entry_t * e = NULL;
  dv_histogram_entry_t * ee = H->head_e;
  if (H->tail_e && (t >= H->tail_e->t)) {
    e = H->tail_e;
    ee = NULL;
  } else {
    if (e_hint && (t >= e_hint->t)) {
      e = e_hint;
      ee = e->next;
    }
    while (ee != NULL && ee->t <= t) {
      e = ee;
      ee = ee->next;
    }
  }
  if (e && (e->t == t))
    return e;
  if (e && ee && (ee->t - e->t) < H->min_entry_interval)
    return e;
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
  
  /* invalidate current max height */
  H->tallest_e = 0;
  H->max_h = 0.0;
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
  
  /* invalidate current max height */
  H->tallest_e = 0;
  H->max_h = 0.0;
}

void
dv_histogram_clean(dv_histogram_t * H) {
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
  double p_bound = H->D->P->num_workers;
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
    H->work += interval * work_p;
    H->delay += interval * delay_p;
    H->nowork += interval * nowork_p;
    e = e->next;
  }
  //H->work /= H->D->P->num_workers;
  //H->delay /= H->D->P->num_workers;
  //H->nowork /= H->D->P->num_workers;
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
  cairo_set_line_width(cr, CS->opts.nlw / V->S->zoom_ratio_x);
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

static void
dv_histogram_build_all_r(dv_histogram_t * H, dv_dag_node_t * node, dv_histogram_entry_t * e_hint_) {
  /* Calculate inward */
  dv_histogram_entry_t * e_hint = e_hint_;
  if (dv_is_union(node) && dv_is_inner_loaded(node)) {
    dv_histogram_build_all_r(H, node->head, e_hint);
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
    dv_histogram_build_all_r(H, u, e_hint);
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    dv_histogram_build_all_r(H, u, e_hint);
    dv_histogram_build_all_r(H, v, e_hint);
    break;
  default:
    dv_check(0);
    break;
  }  
}

void
dv_histogram_build_all(dv_histogram_t * H) {
  double time = dv_get_time();
  if (CS->verbose_level >= 1) {
    fprintf(stderr, "dv_histogram_build_all()\n");
  }
  dv_histogram_clean(H);
  if (H->D) 
    dv_histogram_build_all_r(H, H->D->rt, NULL);
  if (CS->verbose_level >= 1) {
    fprintf(stderr, "... done dv_histogram_build_all(): %lf ms\n", dv_get_time() - time);
  }
}

void
dv_histogram_compute_significant_intervals(dv_histogram_t * H) {
  double time = dv_get_time();
  if (CS->verbose_level >= 1) {
    fprintf(stderr, "dv_histogram_compute_significant_intervals()\n");
  }
  int num_workers = H->D->P->num_workers;
  double num_running_workers = 0.0;
  double cumul_1 = 0.0;
  double cumul_2 = 0.0;
  double cumul_3 = 0.0;
  dv_histogram_entry_t * e;
  dv_histogram_entry_t * ee;
  e = H->head_e;
  while (e != H->tail_e) {
    ee = e->next;
    e->cumul_value_1 = cumul_1;
    e->cumul_value_2 = cumul_2;
    e->cumul_value_3 = cumul_3;
    num_running_workers = e->h[dv_histogram_layer_running];
    /* sched. delay or not */
    if (num_running_workers >= (1.0 * num_workers))
      e->value_1 = 0.0;
    else
      e->value_1 = ee->t - e->t;
    cumul_1 += e->value_1;
    /* delay & nowork associated with sched. delay */
    double num_ready_tasks = 0.0;
    int layer = dv_histogram_layer_running;
    while (++layer < dv_histogram_layer_max) {
      num_ready_tasks += e->h[layer];
    }    
    if (num_running_workers + num_ready_tasks <= num_workers) {
      e->value_2 = (ee->t - e->t) * (num_ready_tasks);
      e->value_3 = (ee->t - e->t) * (num_workers - num_running_workers - num_ready_tasks);
    } else {
      e->value_2 = (ee->t - e->t) * (num_workers - num_running_workers);
      e->value_3 = 0.0;
    }
    cumul_2 += e->value_2;
    cumul_3 += e->value_3;
    e = ee;
  }
  H->tail_e->cumul_value_1 = cumul_1;
  H->tail_e->cumul_value_2 = cumul_2;
  H->tail_e->cumul_value_3 = cumul_3;
  if (CS->verbose_level >= 1) {
    fprintf(stderr, "... done dv_histogram_compute_significant_intervals(): %lf ms\n", dv_get_time() - time);
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
  cairo_set_line_width(cr, (CS->opts.nlw * 2) / V->S->zoom_ratio_x);
  cairo_stroke(cr);
  cairo_restore(cr);
}

/* graph x -> (normalized) time point */
/* normalized means H->D->bt is 0 */
_static_unused_ double
dv_paraprof_convert_graph_x_to_paraprof_x(dv_histogram_t * H, double x) {
  return x * H->D->linear_radix;
}

/* (normalized) time point -> graph x */
_static_unused_ double
dv_paraprof_convert_paraprof_x_to_graph_x(dv_histogram_t * H, double x) {
  return dv_dag_scale_down_linear(H->D, x);
}

/* graph y -> parallelism/worker */
_static_unused_ double
dv_paraprof_convert_graph_y_to_paraprof_y(dv_histogram_t * H, double y) {
  if (y < 0) {
    /* to parallelism */
    y = fabs(y) - H->unit_thick * H->D->radius;
    if (y > 0) {
      return y / (H->unit_thick * H->D->radius);
    } else {
      return 0.0;
    }
  } else {
    /* to worker number */
    return floor(y / (H->D->radius * 2));
  }
}

/* parallelism -> graph y */
_static_unused_ double
dv_paraprof_convert_parallelism_to_graph_y(dv_histogram_t * H, double p) {
  double y = (-1) * p * H->unit_thick * H->D->radius - H->unit_thick * H->D->radius;
  return y;
}

/* worker number -> graph y */
_static_unused_ double
dv_paraprof_convert_worker_number_to_graph_y(dv_histogram_t * H, double w) {
  return w * (H->D->radius * 2);
}

_static_unused_ double
dv_paraprof_convert_viewport_x_to_paraprof_x(dv_histogram_t * H, dv_view_t * V, double x) {
  return dv_paraprof_convert_graph_x_to_paraprof_x(H, dv_view_convert_viewport_x_to_graph_x(V, x));
}
_static_unused_ double
dv_paraprof_convert_paraprof_x_to_viewport_x(dv_histogram_t * H, dv_view_t * V, double x) {
  return dv_view_convert_graph_x_to_viewport_x(V, dv_paraprof_convert_paraprof_x_to_graph_x(H, x));
}
_static_unused_ double
dv_paraprof_convert_viewport_y_to_paraprof_y(dv_histogram_t * H, dv_view_t * V, double y) {
  return dv_paraprof_convert_graph_y_to_paraprof_y(H, dv_view_convert_viewport_y_to_graph_y(V, y));
}
_static_unused_ double
dv_paraprof_convert_parallelism_to_viewport_y(dv_histogram_t * H, dv_view_t * V, double p) {
  return dv_view_convert_graph_y_to_viewport_y(V, dv_paraprof_convert_parallelism_to_graph_y(H, p));
}
_static_unused_ double
dv_paraprof_convert_worker_number_to_viewport_y(dv_histogram_t * H, dv_view_t * V, double w) {
  return dv_view_convert_graph_y_to_viewport_y(V, dv_paraprof_convert_worker_number_to_graph_y(H, w));
}

static void
dv_paraprof_draw_ruler_tick(cairo_t * cr, double x0, double y0, double x1, double y1) {
  cairo_move_to(cr, x0, y0);
  cairo_line_to(cr, x1, y1);
}

void
dv_paraprof_draw_rulers(dv_viewport_t * VP, dv_view_t * V, cairo_t * cr) {
  if (!V || !V->D->H){
    fprintf(stderr, "Error: there is no view (V) or no historgram structure (no V->D->H).\n");
    return;
  }
  dv_histogram_t * H = V->D->H;
  cairo_save(cr);
  cairo_new_path(cr);

  /* background */
  double ruler_width = CS->opts.ruler_width;
  double x = 0.0;
  double y = VP->vph - ruler_width;
  dv_draw_path_rectangle(cr, x, y, VP->vpw, ruler_width);
  x = VP->vpw - ruler_width;
  y = 0.0;
  dv_draw_path_rectangle(cr, x, y, ruler_width, VP->vph);
  GdkRGBA c;
  gdk_rgba_parse(&c, "#F0F0F0");
  cairo_set_source_rgba(cr, c.red, c.green, c.blue, c.alpha);
  cairo_fill(cr);

  /* prepare */
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 8.3);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  char s[DV_STRING_LENGTH];
  cairo_text_extents_t ext;
  double bound_left, bound_right, bound_top, bound_bottom;

  /* horizontal ruler */
  {
    double tick_length = ruler_width;
    double tick_length_2 = tick_length / 2.0;
    double tick_length_3 = tick_length / 4.0;

    double tick_interval_threshold = 200;

    const double A[3] = {2.0, 2.0, 2.5};
    const int An = 3;
    int Ai = 0;
    double tick_interval = 1.0;
    double tick_interval_next = tick_interval * A[Ai++ % An];
    double zr = V->S->zoom_ratio_x;
    while (tick_interval_next / V->D->linear_radix * zr < tick_interval_threshold) {
      tick_interval = tick_interval_next;
      tick_interval_next *= A[Ai++ % An];
    }
    double tick_interval_2 = tick_interval / 5.0;
    double tick_interval_3 = tick_interval / 10.0;
    double x_left = dv_max(0.0, dv_paraprof_convert_viewport_x_to_paraprof_x(H, V, ruler_width));
    double x_right = dv_min(dv_paraprof_convert_viewport_x_to_paraprof_x(H, V, VP->vpw), V->D->et - V->D->bt);
    bound_left = dv_paraprof_convert_paraprof_x_to_viewport_x(H, V, x_left);
    bound_right = dv_paraprof_convert_paraprof_x_to_viewport_x(H, V, x_right);
    double x_0 = floor(x_left / tick_interval) * tick_interval;
    double x_n = ceil(x_right / tick_interval) * tick_interval;
    double y0 = VP->vph - ruler_width;
#if 0    
    fprintf(stderr, "tick_interval = %.2lf\n", tick_interval);
    fprintf(stderr, "x_left = %.2lf, x_right = %.2lf\n", x_left, x_right);
    fprintf(stderr, "x_0 = %.2lf, x_n = %.2lf\n", x_0, x_n);
    fprintf(stderr, "y0 = %.2lf\n", y0);
#endif    

    /* 1st level ticks */
    {
      double x_ = x_0;
      while (x_ <= x_n) {
        if (x_left <= x_ && x_ <= x_right) {
          double x = dv_paraprof_convert_paraprof_x_to_viewport_x(H, V, x_);
          dv_paraprof_draw_ruler_tick(cr, x, y0, x, y0 + tick_length);
          dv_convert_tick_value_to_simplified_string(x_, s);
          cairo_text_extents(cr, s, &ext);
          cairo_move_to(cr, x + 2 - ext.x_bearing, y0 + tick_length - 1 - (ext.y_bearing + ext.height));
          cairo_show_text(cr, s);
        }
        x_ += tick_interval;
      }
    }
    /* 2nd level ticks */
    {
      double x_ = x_0;
      while (x_ <= x_n) {
        if (x_left <= x_ && x_ <= x_right) {
          double x = dv_paraprof_convert_paraprof_x_to_viewport_x(H, V, x_);
          dv_paraprof_draw_ruler_tick(cr, x, y0, x, y0 + tick_length_2);
        }
        x_ += tick_interval_2;
      }
    }
    /* 3nd level ticks */
    if (tick_interval_3 / V->D->linear_radix * zr >= 5.0) {
      double x_ = x_0;
      while (x_ <= x_n) {
        if (x_left <= x_ && x_ <= x_right) {
          double x = dv_paraprof_convert_paraprof_x_to_viewport_x(H, V, x_);
          dv_paraprof_draw_ruler_tick(cr, x, y0, x, y0 + tick_length_3);
        }
        x_ += tick_interval_3;
      }
    }
  } /* horizontal ruler */

  /* vertical ruler: upper part */
  {
    double tick_length = ruler_width;
    double tick_length_2 = tick_length / 2.0;
    double tick_length_3 = tick_length / 4.0;

    double tick_interval_threshold = 120;

    const double A[3] = {2.0, 2.0, 2.5};
    const int An = 3;
    int Ai = 0;
    double tick_interval = 1.0;
    double tick_interval_next = tick_interval * A[Ai++ % An];
    double zr = V->S->zoom_ratio_y;
    while (tick_interval_next * H->unit_thick * H->D->radius * zr < tick_interval_threshold) {
      tick_interval = tick_interval_next;
      tick_interval_next *= A[Ai++ % An];
    }
    double tick_interval_2 = tick_interval / 5.0;
    double tick_interval_3 = tick_interval / 10.0;
    double y_top = dv_min(dv_histogram_get_max_height(H) / (H->unit_thick * H->D->radius), dv_paraprof_convert_viewport_y_to_paraprof_y(H, V, ruler_width));
    double y_bottom = 0.0;
    if (dv_view_convert_viewport_y_to_graph_y(V, VP->vph - ruler_width) < H->unit_thick * H->D->radius) {
      y_bottom = dv_paraprof_convert_viewport_y_to_paraprof_y(H, V, VP->vph - ruler_width);
    }
    bound_top = dv_paraprof_convert_parallelism_to_viewport_y(H, V, y_top);
    bound_bottom = dv_paraprof_convert_parallelism_to_viewport_y(H, V, y_bottom);
    double y_0 = floor(y_bottom / tick_interval) * tick_interval;
    double y_n = ceil(y_top / tick_interval) * tick_interval;
    double x0 = VP->vpw - ruler_width;
#if 0    
    fprintf(stderr, "tick_interval = %.2lf\n", tick_interval);
    fprintf(stderr, "y_top = %.2lf, y_bottom = %.2lf\n", y_top, y_bottom);
    fprintf(stderr, "y_0 = %.2lf, y_n = %.2lf\n", y_0, y_n);
    fprintf(stderr, "x0 = %.2lf\n", x0);
#endif    

    /* 1st level ticks */
    {
      double y_ = y_0;
      while (y_ <= y_n) {
        if (y_bottom <= y_ && y_ <= y_top) {
          double y = dv_paraprof_convert_parallelism_to_viewport_y(H, V, y_);
          dv_paraprof_draw_ruler_tick(cr, x0, y, x0 + tick_length, y);
          dv_convert_tick_value_to_simplified_string(y_, s);
          char ss[5];
          size_t i;
          for (i = 0; i < strlen(s); i++) {
            sprintf(ss, "%c", s[i]);
            cairo_text_extents(cr, ss, &ext);
            cairo_move_to(cr, x0 + tick_length - 1 - (ext.x_bearing + ext.width), y + 3 - ext.y_bearing);
            y += ext.height + 2;
            cairo_show_text(cr, ss);
          }
        }
        y_ += tick_interval;
      }
    }
    /* 2nd level ticks */
    {
      double y_ = y_0;
      while (y_ <= y_n) {
        if (y_bottom <= y_ && y_ <= y_top) {
          double y = dv_paraprof_convert_parallelism_to_viewport_y(H, V, y_);
          dv_paraprof_draw_ruler_tick(cr, x0, y, x0 + tick_length_2, y);
        }
        y_ += tick_interval_2;
      }
    }
    /* 3nd level ticks */
    if (tick_interval_3 * H->unit_thick * H->D->radius * zr >= 5.0) {
      double y_ = y_0;
      while (y_ <= y_n) {
        if (y_bottom <= y_ && y_ <= y_top) {
          double y = dv_paraprof_convert_parallelism_to_viewport_y(H, V, y_);
          dv_paraprof_draw_ruler_tick(cr, x0, y, x0 + tick_length_3, y);
        }
        y_ += tick_interval_3;
      }
    }
  } /* vertical ruler: upper part */

  /* cairo stroke */
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_set_line_width(cr, 0.8);
  cairo_stroke(cr);

  /* horizontal slider */
  if (bound_left <= bound_right) {
    double x = (VP->x < bound_left) ? bound_left : (VP->x > bound_right) ? bound_right : VP->x;
    double y = VP->vph - ruler_width;
    const double base = 4.0;
    cairo_move_to(cr, x, y);
    cairo_rel_line_to(cr, base, base);
    cairo_rel_line_to(cr, -2 * base, 0.0);
    cairo_close_path(cr);
  }

  /* vertical slider */
  if (bound_top <= bound_bottom) {
    double x = VP->vpw - ruler_width;
    double y = (VP->y < bound_top) ? bound_top : (VP->y > bound_bottom) ? bound_bottom : VP->y;
    const double base = 4.0;
    cairo_move_to(cr, x, y);
    cairo_rel_line_to(cr, base, -base);
    cairo_rel_line_to(cr, 0.0, 2 * base);
    cairo_close_path(cr);
  }

  /* cairo fill */
  cairo_fill(cr);
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





/****** CRITICAL PATH ******/


static dv_dag_node_t *
dv_critical_path_find_clicked_node_r(dv_view_t * V, double cx, double cy, dv_dag_node_t * node) {
  dv_dag_node_t * ret = NULL;
  dv_node_coordinate_t * co = &node->c[V->S->coord];
  /* Call inward */
  double x, y, w, h;
  x = co->x;
  y = co->y;
  w = co->rw;
  h = co->dw;
  if ( (x <= cx && cx <= x + w && y <= cy && cy <= y + h)
       || (x <= cx && cx <= x + w && (y + 6 * (2 * V->D->radius)) <= cy && cy <= (y + 6 * (2 * V->D->radius) + h)) ) {
    if (dv_is_single(node))
      ret = node;
    else
      ret = dv_critical_path_find_clicked_node_r(V, cx, cy, node->head);
  } else if ( cx > x + w ) {
    /* Call link-along */
    dv_dag_node_t * next = NULL;
    while ( (next = dv_dag_node_traverse_nexts(node, next)) ) {
      ret = dv_critical_path_find_clicked_node_r(V, cx, cy, next);
      if (ret)
        break;
    }
  }
  return ret;
}

dv_dag_node_t *
dv_critical_path_find_clicked_node(dv_view_t * V, double x, double y) {
  dv_dag_node_t * ret = dv_critical_path_find_clicked_node_r(V, x, y, V->D->rt);
  return ret;
}

static void
dv_view_layout_critical_path_node(dv_view_t * V, dv_dag_node_t * node, int * parent_on_global_cp) {
  /* Counting statistics */
  V->S->nl++;
  if (node->d > V->D->collapsing_d)
    V->D->collapsing_d = node->d;
  
  /* Check if node is still on the global critical paths */
  int me_on_global_cp[DV_NUM_CRITICAL_PATHS];
  int me_on_at_least_one_cp = 0;
  int cp;
  for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
    if (parent_on_global_cp[cp] && dv_node_flag_check(node->f, CS->oncp_flags[cp])) {
      me_on_at_least_one_cp = 1;
      me_on_global_cp[cp] = 1;
    } else {
      me_on_global_cp[cp] = 0;
    }
  }
  if (!me_on_at_least_one_cp) return;
  
  dv_node_coordinate_t * nodeco = &node->c[V->S->coord];
  dv_dag_t * D = V->D;
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
  
  /* Calculate inward */
  nodeco->x = dv_dag_scale_down_linear(D, pi->info.start.t - D->bt);
  nodeco->lw = 0.0;
  nodeco->rw = dv_dag_scale_down_linear(D, pi->info.end.t - D->bt) - dv_dag_scale_down_linear(D, pi->info.start.t - D->bt);
  nodeco->y = 0.0;
  nodeco->dw = 5 * (2 * V->D->radius);
  if (dv_is_union(node)) {
    if (dv_is_inner_loaded(node) && dv_is_expanded(node)) 
      dv_view_layout_critical_path_node(V, node->head, me_on_global_cp);
  }
    
  /* Calculate link-along */
  dv_dag_node_t * next = NULL;
  while ( (next = (dv_dag_node_t *) dv_dag_node_traverse_nexts(node, next)) ) {
    dv_view_layout_critical_path_node(V, next, parent_on_global_cp);
  }
}

void
dv_view_layout_critical_path(dv_view_t * V) {
  V->D->collapsing_d = 0;  
  int on_global_cp[DV_NUM_CRITICAL_PATHS];
  int cp;
  for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
    if (V->D->show_critical_paths[cp])
      on_global_cp[cp] = 1;
    else
      on_global_cp[cp] = 0;
  }
  dv_view_layout_critical_path_node(V, V->D->rt, on_global_cp);
}


static void
dv_view_draw_critical_path_node_1(dv_view_t * V, cairo_t * cr, dv_dag_node_t * node, int * on_global_cp) {
  cairo_save(cr);
  /* Get inputs */
  dv_dag_t * D = V->D;
  dv_view_status_t * S = V->S;
  dv_node_coordinate_t * nodeco = &node->c[S->coord];
  
  /* Count drawn node */
  S->nd++;
  if (node->d > D->cur_d)
    D->cur_d = node->d;
  if (dv_is_union(node) && dv_is_inner_loaded(node)
      && dv_is_shrinked(node)
      && node->d < D->cur_d_ex)
    D->cur_d_ex = node->d;
  
  /* Node color */
  double x = nodeco->x;
  double y = nodeco->y;
  double c[4];
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
  dv_lookup_color(pi, S->nc, c, c+1, c+2, c+3);
  /* Alpha */
  double alpha = 1.0;
  
  /* Coordinates */
  double xx, yy, w, h;

  /* Draw */
  int cp;
  for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
    xx = x;
    yy = y + cp * (6 * (2 * V->D->radius));
    w = nodeco->rw;
    h = nodeco->dw;

    //if ( (D->show_critical_paths[cp] && dv_node_flag_check(node->f, CS->oncp_flags[cp])) ) {
    if (on_global_cp[cp]) {
  
      /* Draw path */
      cairo_new_path(cr);
      if (!dv_rectangle_is_invisible(V, xx, yy, w, h)) {
        dv_rectangle_trim(V, &xx, &yy, &w, &h);
        
        cairo_rectangle(cr, xx, yy, w, h);
        
        /* Draw node */
        if (dv_is_union(node)) {
      
          double c[4] = { 0.15, 0.15, 0.15, 0.2 };
          cairo_set_source_rgba(cr, c[0], c[1], c[2], c[3]);
          cairo_fill(cr);
          
        } else {
          
          cairo_set_source_rgba(cr, c[0], c[1], c[2], c[3] * alpha);
          cairo_fill_preserve(cr);
          if (DV_TIMELINE_NODE_WITH_BORDER) {
            cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
            cairo_stroke_preserve(cr);
          }
          /* Highlight */
          if ( node->highlight ) {
            cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.5);
            cairo_fill_preserve(cr);
          }
          /* Draw node's infotag's mark */
          if (dv_llist_has(V->D->P->itl, (void *) node->pii)) {
            cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.6);
            cairo_fill_preserve(cr);
          }
          
          /* Highlight critical paths */
          if (1) {
            cairo_new_path(cr);
            double margin, line_width;
            GdkRGBA color[1];
            
            //line_width = 2 * CS->opts.nlw;
            line_width = 2 * CS->opts.nlw / V->S->zoom_ratio_x;
            if (line_width > 10 * CS->opts.nlw)
              line_width = 10 * CS->opts.nlw;
            margin = - 0.5 * line_width;
            
            gdk_rgba_parse(color, CS->cp_colors[cp]);
            cairo_set_source_rgba(cr, color->red, color->green, color->blue, color->alpha);
            cairo_set_line_width(cr, line_width);
            cairo_rectangle(cr, xx - margin, yy - margin, w + 2 * margin, h + 2 * margin);
            cairo_stroke(cr);
          }
          
        }  
      }
    }
  }

  /* Flag to draw infotag */
  if (dv_llist_has(V->D->P->itl, (void *) node->pii)) {
    dv_llist_add(V->D->itl, (void *) node);
  }
  
  cairo_restore(cr);
}

static void
dv_view_draw_critical_path_node_r(dv_view_t * V, cairo_t * cr, dv_dag_node_t * node, int * parent_on_global_cp) {
  /* Counting statistics */
  V->S->ndh++;
  if (!node || !dv_is_set(node))
    return;
  
  /* Check if node is still on the global critical paths */
  int me_on_global_cp[DV_NUM_CRITICAL_PATHS];
  int me_on_at_least_one_cp = 0;
  int cp;
  for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
    if (parent_on_global_cp[cp] && dv_node_flag_check(node->f, CS->oncp_flags[cp])) {
      me_on_at_least_one_cp = 1;
      me_on_global_cp[cp] = 1;
    } else {
      me_on_global_cp[cp] = 0;
    }
  }
  if (!me_on_at_least_one_cp) return;
  
  /* Call inward */
  dv_node_coordinate_t * nodeco = &node->c[V->S->coord];
  double xx, yy, w, h;
  int hidden;
  xx = nodeco->x;
  yy = nodeco->y;
  w = nodeco->rw;
  h = nodeco->dw;
  hidden = dv_rectangle_is_invisible(V, xx, yy, w, h);
  yy = nodeco->y + 6 * (2 * V->D->radius);
  hidden &= dv_rectangle_is_invisible(V, xx, yy, w, h);
  if (!hidden) {
    if (dv_is_union(node) && dv_is_inner_loaded(node) && dv_is_expanded(node)) {
      dv_view_draw_critical_path_node_r(V, cr, node->head, me_on_global_cp);
    } else {
      dv_view_draw_critical_path_node_1(V, cr, node, me_on_global_cp);
    }
  }
  
  /* Call link-along */
  if (1) { //(!(hidden & DV_DAG_NODE_HIDDEN_RIGHT)) {
    dv_dag_node_t * next = NULL;
    while ( (next = (dv_dag_node_t *) dv_dag_node_traverse_nexts(node, next)) ) {
      dv_view_draw_critical_path_node_r(V, cr, next, parent_on_global_cp);
    }
  }
}

void
dv_view_draw_critical_path(dv_view_t * V, cairo_t * cr) {
  /* Draw critical path */
  {
    dv_llist_init(V->D->itl);
    V->S->nd = 0;
    V->S->ndh = 0;
    V->D->cur_d = 0;
    V->D->cur_d_ex = V->D->dmax;
    int on_global_cp[DV_NUM_CRITICAL_PATHS];
    int cp;
    for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
      if (V->D->show_critical_paths[cp])
        on_global_cp[cp] = 1;
      else
        on_global_cp[cp] = 0;
    }
    dv_view_draw_critical_path_node_r(V, cr, V->D->rt, on_global_cp);
  }
  
  /* Draw histogram */
  if (V->D->H)
    dv_histogram_draw(V->D->H, cr, V);
  if (V->D->draw_with_current_time)
    dv_paraprof_draw_time_bar(V, V->D->H, cr);
}


/****** end of CRITICAL PATH ******/

