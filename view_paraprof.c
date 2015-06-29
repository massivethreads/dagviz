#include "dagviz.h"

/*-----------Histogram Piece Pool----------------------*/

static void
dv_histogram_piece_pool_init(dv_histogram_piece_pool_t * ppool) {
  (void)ppool;
#if DV_HISTOGRAM_DIVIDE_TO_PIECES    
  ppool->sz = DV_HISTOGRAM_PIECE_POOL_SIZE;
  ppool->n = 0;
  int i;
  for (i=0; i<ppool->sz; i++)
    ppool->avail[i] = 1;
#endif /* DV_HISTOGRAM_DIVIDE_TO_PIECES */
}

#if DV_HISTOGRAM_DIVIDE_TO_PIECES    
static int
dv_histogram_piece_pool_is_empty(dv_histogram_piece_pool_t * ppool) {
  return (ppool->n == ppool->sz);
}

static dv_histogram_piece_t *
dv_histogram_piece_pool_pop(dv_histogram_piece_pool_t * ppool) {
  int i = 0;
  while (i < ppool->sz && !ppool->avail[i]) i++;
  if (i < ppool->sz && ppool->avail[i]) {
    ppool->avail[i] = 0;
    ppool->n++;
    return &ppool->T[i];
  } else {
    return NULL;
  }
}

static void
dv_histogram_piece_pool_push(dv_histogram_piece_pool_t * ppool, dv_histogram_piece_t * p) {
  int i = p - ppool->T;
  dv_check(i < ppool->sz && !ppool->avail[i]);
  ppool->avail[i] = 1;
  ppool->n--;
}

static long
dv_histogram_piece_pool_avail(dv_histogram_piece_pool_t * ppool) {
  return (ppool->sz - ppool->n);
}
#endif /* DV_HISTOGRAM_DIVIDE_TO_PIECES */

/*-----------end of Histogram Piece Pool----------------------*/


/*-----------Histogram Entry Pool----------------------*/

static void
dv_histogram_entry_pool_init(dv_histogram_entry_pool_t * epool) {
  epool->sz = DV_HISTOGRAM_ENTRY_POOL_SIZE;
  epool->n = 0;
  int i;
  for (i=0; i<epool->sz; i++)
    epool->avail[i]= 1;
}

_static_unused_ int
dv_histogram_entry_pool_is_empty(dv_histogram_entry_pool_t * epool) {
  return (epool->n == epool->sz);
}

static dv_histogram_entry_t *
dv_histogram_entry_pool_pop(dv_histogram_entry_pool_t * epool) {
  int i = 0;
  while (i < epool->sz && !epool->avail[i]) i++;
  if (i < epool->sz && epool->avail[i]) {
    epool->avail[i] = 0;
    epool->n++;
    return &epool->T[i];
  } else {
    return NULL;
  }
}

_static_unused_ void
dv_histogram_entry_pool_push(dv_histogram_entry_pool_t * epool, dv_histogram_entry_t * e) {
  int i = e - epool->T;
  dv_check(i < epool->sz && !epool->avail[i]);
  epool->avail[i] = 1;
  epool->n--;
}

_static_unused_ long
dv_histogram_entry_pool_avail(dv_histogram_entry_pool_t * epool) {
  return (epool->sz - epool->n);
}

/*-----------end of Histogram Entry Pool----------------------*/


/*-----------HISTOGRAM----------------------*/

void
dv_histogram_init(dv_histogram_t * H) {
  dv_histogram_entry_pool_init(H->epool);
  dv_histogram_piece_pool_init(H->ppool);
  H->head_e = NULL;
  H->tail_e = NULL;
  H->n_e = 0;
  H->D = NULL;
  H->max_e = NULL;
}

_static_unused_ void
dv_histogram_piece_init(dv_histogram_piece_t * p) {
  p->e = NULL;
  p->h = 0.0;
  p->next = NULL;
  p->node = NULL;
}

static void
dv_histogram_entry_init(dv_histogram_entry_t * e) {
  e->t = 0.0;
  int i;
  for (i=0; i<dv_histogram_layer_max; i++) {
    e->pieces[i] = NULL;
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
  if (ee && ee->t == t)
    return ee;
  dv_histogram_entry_t * new_e = dv_histogram_entry_pool_pop(H->epool);
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
#if DV_HISTOGRAM_DIVIDE_TO_PIECES    
    // Copy pieces from e to new_e
    for (i=0; i<dv_histogram_layer_max; i++) {
      dv_histogram_piece_t * pre_p = NULL;
      dv_histogram_piece_t * new_p = NULL;
      dv_histogram_piece_t * p = e->pieces[i];
      while (p) {
        new_p = dv_histogram_piece_pool_pop(H->ppool);
        if (!new_p) {
          fprintf(stderr, "Error: cannot pop a new piece structure!\n");
          return; //dv_check(0);
        }
        dv_histogram_piece_init(new_p);
        new_p->e = new_e;
        new_p->h = p->h;
        new_p->node = p->node;
        if (pre_p)
          pre_p->next = new_p;
        else
          new_e->pieces[i] = new_p;
        pre_p = new_p;
        p = p->next;
      }
    }
#endif /* DV_HISTOGRAM_DIVIDE_TO_PIECES */
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
#if DV_HISTOGRAM_DIVIDE_TO_PIECES    
  dv_histogram_piece_t * p = e->pieces[layer];
  while (p && p->next !=NULL)
    p = p->next;
  dv_histogram_piece_t * new_p = dv_histogram_piece_pool_pop(H->ppool);
  if (!new_p) {
    fprintf(stderr, "Error: cannot pop a new piece structure!\n");
    return; //dv_check(0);
  }
  dv_histogram_piece_init(new_p);
  new_p->e = e;
  new_p->h = thick;
  new_p->node = node;
  if (p)
    p->next = new_p;
  else
    e->pieces[layer] = new_p;
#endif /* DV_HISTOGRAM_DIVIDE_TO_PIECES */
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
  //fprintf(stderr, "used #pieces: %ld\n", H->ppool->n);
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
  dt = end_t - start_t;
  dv_histogram_pile(H, node,
                    dv_histogram_layer_running,
                    start_t, end_t,
                    pi->info.t_1 / dt);
}

/*
static void
dv_histogram_draw_test(dv_view_t * V, cairo_t * cr) {
  // Transform
  cairo_save(cr);
  cairo_matrix_t mat;
  cairo_matrix_init(&mat, 1.0, 0.0, 0.0, -1.0, 0.0, 0.0);
  // Prepare for text drawing
  cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);
  char s[DV_STRING_LENGTH];
  double x = 0.0;
  double y = 0.0;
  double xx, yy;
  int i;
  // Draw axes
  for (i=0; i<5; i++) {
    sprintf(s, "x = %d", i);
    xx = x;
    yy = y;
    cairo_matrix_transform_point(&mat, &xx, &yy);
    cairo_move_to(cr, xx, yy);
    cairo_show_text(cr, s);
    x += 50;
  }
  x = 0.0;
  for (i=0; i<5; i++) {
    sprintf(s, "y = %d", i);
    xx = x;
    yy = y;
    cairo_matrix_transform_point(&mat, &xx, &yy);
    cairo_move_to(cr, xx, yy);
    cairo_show_text(cr, s);
    y += 50;
  }
  // Un-transform
  cairo_restore(cr);
}
*/

_static_unused_ double
dv_histogram_draw_piece(dv_histogram_t * H, dv_histogram_piece_t * p, cairo_t * cr, double x, double w, double y, int layer) {
  cairo_save(cr);
  double h = p->h;
  // Color
  GdkRGBA color;
  gdk_rgba_parse(&color, DV_HISTOGRAM_COLORS[(layer + 6) % 6]);
  // Draw
  cairo_rectangle(cr, x, y - h, w, h);
  cairo_set_source_rgba(cr, color.red, color.green, color.blue, color.alpha);
  cairo_fill_preserve(cr);
  if (dv_llist_has(H->D->P->itl, (void *) p->node->pii)) {
    cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.6);
    cairo_fill(cr);
  } else {
    cairo_new_path(cr);
  }  
  cairo_restore(cr);
  return y - h;
}

static double
dv_histogram_draw_piece2(dv_histogram_t * H, cairo_t * cr, double x, double w, double y, double h, int layer) {
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
#if DV_HISTOGRAM_DIVIDE_TO_PIECES
    h = 0.0;
    dv_histogram_piece_t * p = e->pieces[i];
    while (p) {
      h += p->h;
      p = p->next;
    }
#else
    h = e->h[i];
#endif /* DV_HISTOGRAM_DIVIDE_TO_PIECES */
    y = dv_histogram_draw_piece2(H, cr, x, w, y, h, i);
  }
}

void
dv_histogram_draw(dv_histogram_t * H, cairo_t * cr) {
  dv_histogram_entry_t * e = H->head_e;
  while (e != NULL && e->next) {
    dv_histogram_draw_entry(H, e, cr);
    e = e->next;
  }
}

void
dv_histogram_reset(dv_histogram_t * H) {
  dv_dag_t * D = H->D;
  dv_histogram_init(H);
  H->D = D;
}

/*-----------end of HISTOGRAM----------------------*/


/*-----------PARAPROF layout functions----------------------*/

static void
dv_view_layout_paraprof_node(dv_view_t * V, dv_dag_node_t * node) {
  V->S->nl++;
  int lt = 4;
  dv_node_coordinate_t * nodeco = &node->c[lt];
  dv_dag_t * D = V->D;
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
  /* Calculate inward */
  nodeco->dw = V->D->radius * 2;
  nodeco->lw = 0.0;
  nodeco->rw = dv_view_scale_down(V, pi->info.end.t - D->bt) - dv_view_scale_down(V, pi->info.start.t - D->bt);
  // node's outward
  int worker = pi->info.worker;
  nodeco->x = dv_view_scale_down(V, pi->info.start.t - D->bt);
  nodeco->y = worker * (2 * V->D->radius);
  if (dv_is_union(node) && dv_is_inner_loaded(node)) {
    // Recursive call
    if (dv_is_expanded(node) || dv_is_expanding(node)) {
      dv_view_layout_paraprof_node(V, node->head);
    } else {
      //V->S->nd++;
      if (node->d > D->cur_d)
        D->cur_d = node->d;
      if (dv_is_union(node) && dv_is_inner_loaded(node)
          && dv_is_shrinked(node)
          && node->d < D->cur_d_ex)
        D->cur_d_ex = node->d;
      dv_histogram_add_node(V->D->H, node);
    }
  } else {
    //V->S->nd++;
    if (node->d > D->cur_d)
      D->cur_d = node->d;
    if (dv_is_union(node) && dv_is_inner_loaded(node)
        && dv_is_shrinked(node)
        && node->d < D->cur_d_ex)
      D->cur_d_ex = node->d;
    dv_histogram_add_node(V->D->H, node);
  }
    
  /* Calculate link-along */
  dv_dag_node_t * u, * v; // linked nodes
  switch ( dv_dag_node_count_nexts(node) ) {
  case 0:
    break;
  case 1:
    u = node->next;
    // Recursive call
    dv_view_layout_paraprof_node(V, u);
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    // Recursive call
    dv_view_layout_paraprof_node(V, u);
    dv_view_layout_paraprof_node(V, v);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

void
dv_view_layout_paraprof(dv_view_t * V) {
  V->S->nd = 0;
  V->S->ndh = 0;
  V->D->cur_d = 0;
  V->D->cur_d_ex = V->D->dmax;
  V->D->H->max_e = NULL;
  dv_check(V->D->H);
  count_insert_entry = 0;
  count_pile_entry = 0;
  count_pile = 0;
  count_add_node = 0;
  dv_view_layout_paraprof_node(V, V->D->rt);
  fprintf(stderr,
          "count_insert_entry = %ld\n"
          "  count_pile_entry = %ld\n"
          "        count_pile = %ld\n"
          "    count_add_node = %ld\n",
          count_insert_entry,
          count_pile_entry,
          count_pile,
          count_add_node);
}

/*-----------end of PARAPROF layout functions----------------------*/


/*-----------------PARAPROF Drawing functions-----------*/

static void
dv_view_draw_paraprof_node_1(dv_view_t * V, cairo_t * cr, dv_dag_node_t * node) {
  dv_dag_t * D = V->D;
  dv_view_status_t * S = V->S;
  int lt = 4;
  dv_node_coordinate_t * nodeco = &node->c[lt];
  // Count node drawn
  S->nd++;
  if (node->d > D->cur_d)
    D->cur_d = node->d;
  if (dv_is_union(node) && dv_is_inner_loaded(node)
      && dv_is_shrinked(node)
      && node->d < D->cur_d_ex)
    D->cur_d_ex = node->d;
  // Node color
  double x = nodeco->x;
  double y = nodeco->y;
  double c[4];
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
  dv_lookup_color(pi, S->nc, c, c+1, c+2, c+3);
  // Alpha
  double alpha = 1.0;
  // Draw path
  cairo_save(cr);
  cairo_new_path(cr);
  double xx, yy, w, h;
  // Normal-sized box (terminal node)
  xx = x;
  yy = y;
  w = nodeco->rw;
  h = nodeco->dw;
  
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
    
    // Draw path
    cairo_move_to(cr, xx, yy);
    cairo_line_to(cr, xx + w, yy);
    cairo_line_to(cr, xx + w, yy + h);
    cairo_line_to(cr, xx, yy + h);
    cairo_close_path(cr);
    
    // Draw node
    cairo_set_source_rgba(cr, c[0], c[1], c[2], c[3] * alpha);
    cairo_fill_preserve(cr);
    if (DV_TIMELINE_NODE_WITH_BORDER) {
      cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
      cairo_stroke_preserve(cr);
    }
    // Draw opaque
    if (dv_llist_has(V->D->P->itl, (void *) node->pii)) {
      cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.6);
      cairo_fill(cr);
    }
    
  }
  // Flag to draw infotag
  if (dv_llist_has(V->D->P->itl, (void *) node->pii)) {
    dv_llist_add(V->D->itl, (void *) node);
  }
  cairo_restore(cr);
}

static void
dv_view_draw_paraprof_node_r(dv_view_t * V, cairo_t * cr, dv_dag_node_t * node) {
  // Count node
  V->S->ndh++;
  if (!node || !dv_is_set(node))
    return;
  /* Call inward */
  if (dv_is_union(node)) {
    if (dv_is_inner_loaded(node) && dv_is_expanded(node))
      dv_view_draw_paraprof_node_r(V, cr, node->head);
  } else {
    dv_view_draw_paraprof_node_1(V, cr, node);
  }
  /* Call link-along */
  dv_dag_node_t * next = NULL;
  while ( (next = dv_dag_node_traverse_nexts(node, next)) ) {
    dv_view_draw_paraprof_node_r(V, cr, next);
  }
}

void
dv_view_draw_paraprof(dv_view_t * V, cairo_t * cr) {
  // Set adaptive line width
  double line_width = dv_min(DV_NODE_LINE_WIDTH, DV_NODE_LINE_WIDTH / dv_min(V->S->zoom_ratio_x, V->S->zoom_ratio_y));
  cairo_set_line_width(cr, line_width);
  // White & grey colors
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");
  GdkRGBA grey[1];
  gdk_rgba_parse(grey, "light grey");
  // Draw background
  /*
  cairo_new_path(cr);
  cairo_set_source_rgb(cr, grey->red, grey->green, grey->blue);
  cairo_paint(cr);
  cairo_set_source_rgb(cr, white->red, white->green, white->blue);
  double width = V->D->rt->c[V->S->lt].rw;
  double height = V->D->P->num_workers * (2 * V->D->radius);
  cairo_rectangle(cr, 0.0, 0.0, width, height);
  cairo_fill(cr);
  */
  // Draw nodes
  dv_llist_init(V->D->itl);
  dv_view_draw_paraprof_node_r(V, cr, V->D->rt);
  // Draw worker numbers
  cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);
  char s[DV_STRING_LENGTH];
  double xx, yy;
  xx = -75;
  yy = V->D->radius * 1.2;
  int i;
  for (i=0; i<V->D->P->num_workers; i++) {
    sprintf(s, "Worker %d", i);            
    cairo_move_to(cr, xx, yy);
    cairo_show_text(cr, s);
    yy += 2 * V->D->radius;
  }
  //cairo_move_to(cr, 20, - 64 * V->D->radius);
  //cairo_line_to(cr, V->S->vpw - 20, -64 * V->D->radius);
  //cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
  //cairo_stroke(cr);
  // Draw parallelism profile
  if (V->D->H)
    dv_histogram_draw(V->D->H, cr);
}

/*-----------------end of PARAPROF drawing functions-----------*/
