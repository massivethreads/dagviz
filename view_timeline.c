#include "dagviz.h"

/*-----------Timeline layout functions----------------------*/

static void dv_layout_timeline_node(dv_dag_node_t *node) {
  /* Calculate inward */
  int is_single_node = 1;
  dr_pi_dag_node * pi = dv_pidag_get_node(node);
  switch (pi->info.kind) {
  case dr_dag_node_kind_wait_tasks:
  case dr_dag_node_kind_end_task:
  case dr_dag_node_kind_create_task:
    break;
  case dr_dag_node_kind_section:
  case dr_dag_node_kind_task:
    if (dv_is_union(node))
      is_single_node = 0;
    break;
  default:
    dv_check(0);
    break;
  }
  // node's inward
  node->lw = dv_layout_calculate_hsize(node);
  node->rw = dv_layout_calculate_hsize(node);
  node->dw = dv_layout_calculate_vresize(pi->info.end.t - G->bt) - dv_layout_calculate_vresize(pi->info.start.t - G->bt);
  // node's outward
  int worker = pi->info.worker;
  node->x = DV_RADIUS + worker * (2 * DV_RADIUS + DV_HDIS);
  node->y = dv_layout_calculate_vresize(pi->info.start.t - G->bt);
  if (!is_single_node) {
    // Recursive call
    if (!dv_is_shrinked(node))
      dv_layout_timeline_node(node->head);
  }
    
  /* Calculate link-along */
  int n_links = dv_llist_size(node->links);
  dv_dag_node_t * u; // linked node 1
  dv_dag_node_t * v; // linked node 2
  double time_gap, gap, ugap, vgap;
  switch (n_links) {
  case 0:
    break;
  case 1:
    u = (dv_dag_node_t *) node->links->top->item;
    // Recursive call
    dv_layout_timeline_node(u);
    break;
  case 2:
    u = (dv_dag_node_t *) node->links->top->item; // cont node
    v = (dv_dag_node_t *) node->links->top->next->item; // task node
    // Recursive call
    dv_layout_timeline_node(u);
    dv_layout_timeline_node(v);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

void dv_layout_timeline_dvdag(dv_dag_t *G) {

  // Absolute coord
  dv_layout_timeline_node(G->rt);

  // Check
  //print_layout(G);  

}

/*-----------end of Timeline layout functions----------------------*/


/*-----------------TIMELINE Drawing functions-----------*/

static void draw_timeline_node_1(cairo_t *cr, dv_dag_node_t *node) {
  // Count node drawn
  S->nd++;
  if (node->d > G->cur_d)
    G->cur_d = node->d;
  if (dv_is_union(node) && dv_is_shrinked(node)
      && node->d < G->cur_d_ex)
    G->cur_d_ex = node->d;
  // Node color
  double x = node->x;
  double y = node->y;
  double c[4];
  dv_lookup_color(node, c, c+1, c+2, c+3);
  // Alpha
  double alpha = 1.0;
  // Draw path
  cairo_save(cr);
  cairo_new_path(cr);
  double xx, yy, w, h;
  // Normal-sized box (terminal node)
  xx = x - node->lw;
  yy = y;
  w = node->lw + node->rw;
  h = node->dw;
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
}

static void draw_timeline_node_r(cairo_t *cr, dv_dag_node_t *node) {
  dv_check(node);
  /* Calculate inward */
  int is_single_node = 1;
  dr_pi_dag_node * pi = dv_pidag_get_node(node);
  switch (pi->info.kind) {
  case dr_dag_node_kind_wait_tasks:
  case dr_dag_node_kind_end_task:
  case dr_dag_node_kind_create_task:
    break;
  case dr_dag_node_kind_section:
  case dr_dag_node_kind_task:
    if (dv_is_union(node))
      is_single_node = 0;
    break;
  default:
    dv_check(0);
    break;
  }
  if (is_single_node) {
    draw_timeline_node_1(cr, node);
  } else {
    // Recursive call
    if (!dv_is_shrinked(node))
      draw_timeline_node_r(cr, node->head);
  }
    
  /* Calculate link-along */
  int n_links = dv_llist_size(node->links);
  dv_dag_node_t * u; // linked node 1
  dv_dag_node_t * v; // linked node 2
  switch (n_links) {
  case 0:
    break;
  case 1:
    u = (dv_dag_node_t *) node->links->top->item;
    // Recursive call
    draw_timeline_node_r(cr, u);
    break;
  case 2:
    u = (dv_dag_node_t *) node->links->top->item; // cont node
    v = (dv_dag_node_t *) node->links->top->next->item; // task node
    // Recursive call
    draw_timeline_node_r(cr, u);
    draw_timeline_node_r(cr, v);
    break;
  default:
    dv_check(0);
    break;
  }
}

void dv_draw_timeline_dvdag(cairo_t *cr, dv_dag_t *G) {
  cairo_set_line_width(cr, DV_NODE_LINE_WIDTH);
  int i;
  // Draw nodes
  S->nd = 0;
  draw_timeline_node_r(cr, G->rt);
  // Draw worker numbers
  cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);
  char *s = (char *) dv_malloc( DV_STRING_LENGTH * sizeof(char) );
  double xx, yy;
  xx = DV_RADIUS;
  yy = -4;
  for (i=0; i<G->nw; i++) {
    sprintf(s, "Worker %d", i);            
    cairo_move_to(cr, xx - 30, yy);
    cairo_show_text(cr, s);
    xx += 2 * DV_RADIUS + DV_HDIS;
  }
}

/*-----------------end of TIMELINE drawing functions-----------*/


