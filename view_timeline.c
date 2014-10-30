#include "dagviz.h"

/*-----------Timeline layout functions----------------------*/

static void dv_view_layout_timeline_node(dv_view_t *V, dv_dag_node_t *node) {
  int lt = 2;
  dv_node_coordinate_t *nodeco = &node->c[lt];
  dv_dag_t *D = V->D;
  dr_pi_dag_node * pi = dv_pidag_get_node(D->P, node);
  /* Calculate inward */
  nodeco->lw = DV_RADIUS;//dv_view_calculate_hsize(V, node);
  nodeco->rw = DV_RADIUS;//dv_view_calculate_hsize(V, node);
  nodeco->dw = dv_view_calculate_vresize(V, pi->info.end.t - D->bt) - dv_view_calculate_vresize(V, pi->info.start.t - D->bt);
  // node's outward
  int worker = pi->info.worker;
  nodeco->x = DV_RADIUS + worker * (2 * DV_RADIUS + DV_HDIS);
  nodeco->y = dv_view_calculate_vresize(V, pi->info.start.t - D->bt);
  if (dv_is_union(node) && dv_is_inner_loaded(node)) {
    // Recursive call
    if (dv_is_expanded(node))
      dv_view_layout_timeline_node(V, node->head);
  }
    
  /* Calculate link-along */
  int n_links = dv_llist_size(node->links);
  dv_dag_node_t *u, *v; // linked nodes
  switch (n_links) {
  case 0:
    break;
  case 1:
    u = (dv_dag_node_t *) node->links->top->item;
    // Recursive call
    dv_view_layout_timeline_node(V, u);
    break;
  case 2:
    u = (dv_dag_node_t *) node->links->top->item; // cont node
    v = (dv_dag_node_t *) node->links->top->next->item; // task node
    // Recursive call
    dv_view_layout_timeline_node(V, u);
    dv_view_layout_timeline_node(V, v);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

void dv_view_layout_timeline(dv_view_t *V) {

  // Absolute coord
  dv_view_layout_timeline_node(V, V->D->rt);

  // Check
  //print_layout(G);  

}

/*-----------end of Timeline layout functions----------------------*/


/*-----------------TIMELINE Drawing functions-----------*/

static void dv_view_draw_timeline_node_1(dv_view_t *V, cairo_t *cr, dv_dag_node_t *node) {
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  int lt = 2;
  dv_node_coordinate_t *nodeco = &node->c[lt];
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
  dr_pi_dag_node *pi = dv_pidag_get_node(D->P, node);
  dv_lookup_color(pi, S->nc, c, c+1, c+2, c+3);
  // Alpha
  double alpha = 1.0;
  // Draw path
  cairo_save(cr);
  cairo_new_path(cr);
  double xx, yy, w, h;
  // Normal-sized box (terminal node)
  xx = x - nodeco->lw;
  yy = y;
  w = nodeco->lw + nodeco->rw;
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
    cairo_stroke(cr);
  }
  cairo_restore(cr);
  // Draw infotag
  if (dv_llist_has(V->D->P->itl, (void *) node->pii))
    dv_view_draw_infotag_1(V, cr, node);
}

static void dv_view_draw_timeline_node_r(dv_view_t *V, cairo_t *cr, dv_dag_node_t *node) {
  // Count node
  V->S->ndh++;
  if (!node || !dv_is_set(node))
    return;
  /* Call inward */
  if (dv_is_union(node)) {
    if (dv_is_inner_loaded(node) && !dv_is_shrinked(node))
      dv_view_draw_timeline_node_r(V, cr, node->head);
  } else {
    dv_view_draw_timeline_node_1(V, cr, node);
  }
  /* Call link-along */
  dv_dag_node_t * u = NULL;
  while (u = (dv_dag_node_t *) dv_llist_iterate_next(node->links, u)) {
    dv_view_draw_timeline_node_r(V, cr, u);
  }
}

void dv_view_draw_timeline(dv_view_t *V, cairo_t *cr) {
  cairo_set_line_width(cr, DV_NODE_LINE_WIDTH);
  int i;
  // Draw nodes
  dv_view_draw_timeline_node_r(V, cr, V->D->rt);
  // Draw worker numbers
  cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);
  char *s = (char *) dv_malloc( DV_STRING_LENGTH * sizeof(char) );
  double xx, yy;
  xx = DV_RADIUS;
  yy = -4;
  for (i=0; i<V->D->P->num_workers; i++) {
    sprintf(s, "Worker %d", i);            
    cairo_move_to(cr, xx - 30, yy);
    cairo_show_text(cr, s);
    xx += 2 * DV_RADIUS + DV_HDIS;
  }
}

/*-----------------end of TIMELINE drawing functions-----------*/

