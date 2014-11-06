#include "dagviz.h"

/*-----------Timeline2 layout functions----------------------*/

static void
dv_view_layout_timeline2_node(dv_view_t * V, dv_dag_node_t * node) {
  int lt = 3;
  dv_node_coordinate_t * nodeco = &node->c[lt];
  dv_dag_t * D = V->D;
  dr_pi_dag_node * pi = dv_pidag_get_node(D->P, node);
  /* Calculate inward */
  nodeco->dw = DV_RADIUS * 2;
  nodeco->lw = 0.0;
  nodeco->rw = dv_view_calculate_vresize(V, pi->info.end.t - D->bt) - dv_view_calculate_vresize(V, pi->info.start.t - D->bt);
  // node's outward
  int worker = pi->info.worker;
  nodeco->x = dv_view_calculate_vresize(V, pi->info.start.t - D->bt);
  nodeco->y = worker * (2 * DV_RADIUS);
  if (dv_is_union(node) && dv_is_inner_loaded(node)) {
    // Recursive call
    if (dv_is_expanded(node))
      dv_view_layout_timeline2_node(V, node->head);
  }
    
  /* Calculate link-along */
  int n_links = dv_llist_size(node->links);
  dv_dag_node_t * u, * v; // linked nodes
  switch (n_links) {
  case 0:
    break;
  case 1:
    u = (dv_dag_node_t *) node->links->top->item;
    // Recursive call
    dv_view_layout_timeline2_node(V, u);
    break;
  case 2:
    u = (dv_dag_node_t *) node->links->top->item; // cont node
    v = (dv_dag_node_t *) node->links->top->next->item; // task node
    // Recursive call
    dv_view_layout_timeline2_node(V, u);
    dv_view_layout_timeline2_node(V, v);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

void
dv_view_layout_timeline2(dv_view_t * V) {

  // Absolute coord
  dv_view_layout_timeline2_node(V, V->D->rt);

  // Check
  //print_layout(G);  

}

/*-----------end of Timeline2 layout functions----------------------*/


/*-----------------TIMELINE2 Drawing functions-----------*/

static void
dv_view_draw_timeline2_node_1(dv_view_t * V, cairo_t * cr, dv_dag_node_t * node) {
  dv_dag_t * D = V->D;
  dv_view_status_t * S = V->S;
  int lt = 3;
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
  dr_pi_dag_node * pi = dv_pidag_get_node(D->P, node);
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
  // Draw infotag
  if (dv_llist_has(V->D->P->itl, (void *) node->pii)) {
    cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.6);
    cairo_fill(cr);
    dv_llist_add(V->D->itl, (void *) node);
  }
  cairo_restore(cr);
}

static void
dv_view_draw_timeline2_node_r(dv_view_t * V, cairo_t * cr, dv_dag_node_t * node) {
  // Count node
  V->S->ndh++;
  if (!node || !dv_is_set(node))
    return;
  /* Call inward */
  if (dv_is_union(node)) {
    if (dv_is_inner_loaded(node) && !dv_is_shrinked(node))
      dv_view_draw_timeline2_node_r(V, cr, node->head);
  } else {
    dv_view_draw_timeline2_node_1(V, cr, node);
  }
  /* Call link-along */
  dv_dag_node_t * u = NULL;
  while (u = (dv_dag_node_t *) dv_llist_iterate_next(node->links, u)) {
    dv_view_draw_timeline2_node_r(V, cr, u);
  }
}

static void
dv_view_draw_paraprof(dv_view_t * V, cairo_t * cr) {
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

void
dv_view_draw_timeline2(dv_view_t * V, cairo_t * cr) {
  // Set adaptive line width
  double line_width = dv_min(DV_NODE_LINE_WIDTH, DV_NODE_LINE_WIDTH / V->S->zoom_ratio);
  cairo_set_line_width(cr, line_width);
  fprintf(stderr, "line width = %lf, zoom_ratio = %lf\n", line_width, V->S->zoom_ratio);
  // White & grey colors
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");
  GdkRGBA grey[1];
  gdk_rgba_parse(grey, "light grey");
  // Draw background
  cairo_new_path(cr);
  cairo_set_source_rgb(cr, grey->red, grey->green, grey->blue);
  cairo_paint(cr);
  cairo_set_source_rgb(cr, white->red, white->green, white->blue);
  double width = V->D->rt->c[V->S->lt].rw;
  double height = V->D->P->num_workers * (2 * DV_RADIUS);
  cairo_rectangle(cr, 0.0, 0.0, width, height);
  cairo_fill(cr);
  // Draw nodes
  dv_llist_init(V->D->itl);
  dv_view_draw_timeline2_node_r(V, cr, V->D->rt);
  // Draw infotags
  dv_dag_node_t * node = NULL;
  while (node = (dv_dag_node_t *) dv_llist_pop(V->D->itl)) {
    dv_view_draw_infotag_1(V, cr, node);
  }            
  // Draw worker numbers
  cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);
  char s[DV_STRING_LENGTH];
  double xx, yy;
  xx = -75;
  yy = DV_RADIUS * 1.2;
  int i;
  for (i=0; i<V->D->P->num_workers; i++) {
    sprintf(s, "Worker %d", i);            
    cairo_move_to(cr, xx, yy);
    cairo_show_text(cr, s);
    yy += 2 * DV_RADIUS;
  }
  // Draw parallelism profile
  dv_view_draw_paraprof(V, cr);
}

/*-----------------end of TIMELINE2 drawing functions-----------*/
