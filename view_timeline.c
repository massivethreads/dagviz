#include "dagviz.h"

/*-----------Timeline2 layout functions----------------------*/

static void
dv_view_layout_timeline2_node(dv_view_t * V, dv_dag_node_t * node) {
  V->S->nl++;
  int lt = 3;
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
    if (dv_is_expanded(node))
      dv_view_layout_timeline2_node(V, node->head);
  }
    
  /* Calculate link-along */
  dv_dag_node_t * u, * v; // linked nodes
  switch ( dv_dag_node_count_nexts(node) ) {
  case 0:
    break;
  case 1:
    u = node->next;
    // Recursive call
    dv_view_layout_timeline2_node(V, u);
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
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

static double min_x;
static double max_x;
static double min_w;
static double max_w;

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
  
  if (xx < min_x) min_x = xx;
  if (xx + w > max_x) max_x = xx + w;
  if (w < min_w) min_w = w;
  if (w > max_w) max_w = w;

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
  dv_dag_node_t * next = NULL;
  while ( (next = dv_dag_node_traverse_nexts(node, next)) ) {
    dv_view_draw_timeline2_node_r(V, cr, next);
  }
}

void
dv_view_draw_timeline2(dv_view_t * V, cairo_t * cr) {
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
  min_x = 1000;
  max_x = 0.0;
  min_w = 1000;
  max_w = 0.0;
  dv_view_draw_timeline2_node_r(V, cr, V->D->rt);
  //fprintf(stderr, "min_x = %lf, max_x = %lf, min_w = %lf, max_w = %lf, max_x*zrx = %lf, max_w*zrx = %lf\n", min_x, max_x, min_w, max_w, max_x * V->S->zoom_ratio_x, max_w * V->S->zoom_ratio_x);
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
}

/*-----------------end of TIMELINE2 drawing functions-----------*/
