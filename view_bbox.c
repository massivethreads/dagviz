#include "dagviz.h"

/*-----------------BBox layout functions-----------*/

static double dv_layout_calculate_hgap(dv_dag_node_t *node) {
  double gap = dv_layout_calculate_gap(node);
  double hgap = gap * DV_HDIS;
  return hgap;
}

double dv_layout_calculate_hsize(dv_dag_node_t *node) {
  double gap = dv_layout_calculate_gap(node->parent);
  double hsize = gap * DV_RADIUS;
  return hsize;
}

double dv_layout_calculate_vresize(double val) {
  double ret;
  switch (S->sdt) {
  case 0:
    ret = log(val) / log(S->log_radix);
    break;
  case 1:
    ret = pow(val, S->power_radix);
    break;
  case 2:
    ret = val / S->linear_radix;
    break;
  default:
    dv_check(0);
    break;
  }
  return ret;
}

static double dv_layout_calculate_vgap(dv_dag_node_t *parent, dv_dag_node_t *node1, dv_dag_node_t *node2) {
  dr_pi_dag_node * pi1 = dv_pidag_get_node(node1);
  dr_pi_dag_node * pi2 = dv_pidag_get_node(node2);
  double gap = dv_layout_calculate_gap(parent);
  double vgap;
  if (!S->frombt) {
    // begin - end
    double time_gap = dv_layout_calculate_vresize(pi2->info.start.t - pi1->info.end.t);
    vgap = gap * time_gap;
  } else {
    // from beginning
    double time1 = dv_layout_calculate_vresize(pi1->info.end.t - G->bt);
    double time2 = dv_layout_calculate_vresize(pi2->info.start.t - G->bt);
    vgap = gap * (time2 - time1);
  }
  return vgap;
}

double dv_layout_calculate_vsize(dv_dag_node_t *node) {
  dr_pi_dag_node * pi = dv_pidag_get_node(node);
  double gap = dv_layout_calculate_gap(node->parent);
  double vsize;
  if (!S->frombt) {
    // begin - end
    double time_gap = dv_layout_calculate_vresize(pi->info.end.t - pi->info.start.t);
    vsize = gap * time_gap;
  } else {
    // from beginning
    double time1 = dv_layout_calculate_vresize(pi->info.start.t - G->bt);
    double time2 = dv_layout_calculate_vresize(pi->info.end.t - G->bt);
    vsize = gap * (time2 - time1);
  }
  return vsize;
}

static double dv_layout_calculate_vsize_pure(dv_dag_node_t *node) {
  dr_pi_dag_node * pi = dv_pidag_get_node(node);
  double vsize;
  if (!S->frombt) {
    double time = pi->info.end.t - pi->info.start.t;
    vsize = dv_layout_calculate_vresize(time);
  } else {
    double time1 = dv_layout_calculate_vresize(pi->info.start.t - G->bt);
    double time2 = dv_layout_calculate_vresize(pi->info.end.t - G->bt);
    double vsize = time2 - time1;
  }
  return vsize;
}

static dv_dag_node_t * dv_layout_node_get_last_tail(dv_dag_node_t *node) {
  dv_dag_node_t * ret = 0;
  dv_dag_node_t * tail;
  dv_llist_iterate_init(node->tails);
  while (tail = (dv_dag_node_t *) dv_llist_iterate_next(node->tails)) {
    dr_pi_dag_node * ret_pi = dv_pidag_get_node(ret);
    dr_pi_dag_node * tail_pi = dv_pidag_get_node(tail);
    if (!ret || ret_pi->info.end.t < tail_pi->info.end.t)
      ret = tail;
  }
  return ret;
}

static double dv_layout_node_get_node_xp(dv_dag_node_t *node) {
  dv_dag_node_t * u = node;
  double dis = 0.0;
  while (u) {
    dis += u->xpre;
    u = u->pre;
  }
  return dis;
}

static double dv_layout_node_get_last_tail_xp_r(dv_dag_node_t *node) {
  dv_check(node);
  double ret = 0.0;
  dv_dag_node_t * last_tail = 0;
  if (!dv_is_single(node))
    last_tail = dv_layout_node_get_last_tail(node);
  if (last_tail) {
    ret += dv_layout_node_get_node_xp(last_tail);
    ret += dv_layout_node_get_last_tail_xp_r(last_tail);
  }
  return ret;
}

static void dv_layout_bbox_node(dv_dag_node_t *node) {
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
    if (dv_is_union(node)
        && ( !dv_is_shrinked(node) || dv_is_expanding(node) )) {
      dv_check(node->head);
      is_single_node = 0;
    }
    break;
  default:
    dv_check(0);
    break;
  }
  if (is_single_node) {
    // node's inward
    node->lw = dv_layout_calculate_hsize(node);
    node->rw = dv_layout_calculate_hsize(node);
    node->dw = dv_layout_calculate_vsize(node);
  } else {
    // node's head's outward
    node->head->xpre = 0.0;
    node->head->y = node->y;
    // Recursive call
    dv_layout_bbox_node(node->head);
    // node's inward
    node->lw = node->head->link_lw;
    node->rw = node->head->link_rw;
    node->dw = node->head->link_dw;
    node->avoid_inward = 0;
    // avoid shrinking too small
    double comp = dv_layout_calculate_vsize_pure(node);
    if (node->lw < DV_RADIUS
        && node->rw < DV_RADIUS
        && node->dw < comp) {
      node->lw = DV_RADIUS;
      node->rw = DV_RADIUS;
      node->dw = comp;
      node->avoid_inward = 1;
    }
  }
    
  /* Calculate link-along */
  int n_links = dv_llist_size(node->links);
  dv_dag_node_t * u; // linked node 1
  dv_dag_node_t * v; // linked node 2
  double time_gap, gap, ugap, vgap;
  switch (n_links) {
  case 0:
    // node's link-along
    node->link_lw = node->lw;
    node->link_rw = node->rw;
    node->link_dw = node->dw;
    break;
  case 1:
    u = (dv_dag_node_t *) node->links->top->item;
    // node & u's gap
    gap = dv_layout_calculate_vgap(node->parent, node, u);
    // node's linked u's outward    
    u->xpre = dv_layout_node_get_last_tail_xp_r(node);
    u->y = node->y + node->dw + gap;
    // Recursive call
    dv_layout_bbox_node(u);
    // node's link-along
    node->link_lw = dv_max(node->lw, u->link_lw - u->xpre);
    node->link_rw = dv_max(node->rw, u->link_rw + u->xpre);
    node->link_dw = node->dw + gap + u->link_dw;
    break;
  case 2:
    u = (dv_dag_node_t *) node->links->top->item; // cont node
    v = (dv_dag_node_t *) node->links->top->next->item; // task node
    // node & u,v's gap
    ugap = dv_layout_calculate_vgap(node->parent, node, u);
    vgap = dv_layout_calculate_vgap(node->parent, node, v);
    // node's linked u,v's outward
    u->y = node->y + node->dw + ugap;
    v->y = node->y + node->dw + vgap;
    // Recursive call
    dv_layout_bbox_node(u);
    dv_layout_bbox_node(v);
    // node's linked u,v's outward
    u->xpre = dv_layout_calculate_hgap(u->parent) + u->link_lw;
    v->xpre = - dv_layout_calculate_hgap(v->parent) - v->link_rw;
    // node's link-along
    node->link_lw = dv_max(node->lw, - v->xpre + v->link_lw);
    node->link_rw = dv_max(node->rw, u->xpre + u->link_rw);
    node->link_dw = node->dw + dv_max(ugap + u->link_dw, vgap + v->link_dw);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

static void dv_layout_bbox_node_2nd(dv_dag_node_t *node) {
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
    if (dv_is_union(node)
        && ( !dv_is_shrinked(node) || dv_is_expanding(node) )) {
      dv_check(node->head);
      is_single_node = 0;
    }
    break;
  default:
    dv_check(0);
    break;
  }
  if (!is_single_node) {
    // node's head's outward
    node->head->xp = 0.0;
    node->head->x = node->x;
    // Recursive call
    dv_layout_bbox_node_2nd(node->head);
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
    // node's linked u's outward
    u->xp = u->xpre + node->xp;
    u->x = u->xp + u->parent->x;
    // Recursive call
    dv_layout_bbox_node_2nd(u);
    break;
  case 2:
    u = (dv_dag_node_t *) node->links->top->item; // cont node
    v = (dv_dag_node_t *) node->links->top->next->item; // task node
    // node's linked u,v's outward
    u->xp = u->xpre + node->xp;
    u->x = u->xp + u->parent->x;
    v->xp = v->xpre + node->xp;
    v->x = v->xp + v->parent->x;
    // Recursive call
    dv_layout_bbox_node_2nd(u);
    dv_layout_bbox_node_2nd(v);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

void dv_layout_bbox_dvdag(dv_dag_t *G) {

  // Relative coord
  G->rt->xpre = 0.0; // pre-based
  G->rt->y = 0.0;
  dv_layout_bbox_node(G->rt);

  // Absolute coord
  G->rt->xp = 0.0; // parent-based
  G->rt->x = 0.0;
  dv_layout_bbox_node_2nd(G->rt);

  // Check
  //print_layout(G);  

}

/*-----------------end of BBox layout functions-----------*/


/*-----------------DAG BBox Drawing functions-----------*/

static void draw_bbox_node_1(cairo_t *cr, dv_dag_node_t *node) {
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
  if (dv_is_union(node)) {

    if (dv_is_expanding(node) || dv_is_shrinking(node)) {

      double margin = 1.0;
      if (dv_is_expanding(node)) {
        // Fading out
        alpha = dv_get_alpha_fading_out(node);
        margin = dv_get_alpha_fading_in(node);
      } else {
        // Fading in
        alpha = dv_get_alpha_fading_in(node);
        margin = dv_get_alpha_fading_out(node);
      }
      // Large-sized box
      margin *= DV_UNION_NODE_MARGIN;
      xx = x - node->lw - margin;
      yy = y - margin;
      w = node->lw + node->rw + 2 * margin;
      h = node->dw + 2 * margin;
      
    } else {
      
      // Normal-sized box
      xx = x - node->lw;
      yy = y;
      w = node->lw + node->rw;
      h = node->dw;
      alpha = 1.0;
      if (dv_is_shrinking(node->parent)) {
        // Fading out
        alpha = dv_get_alpha_fading_out(node->parent);
      } else if (dv_is_expanding(node->parent)) {
        // Fading in
        alpha = dv_get_alpha_fading_in(node->parent);
      }
      
    }

  } else {
    
    // Normal-sized box (terminal node)
    xx = x - node->lw;
    yy = y;
    w = node->lw + node->rw;
    h = node->dw;
    alpha = 1.0;
    if (dv_is_shrinking(node->parent)) {
      // Fading out
      alpha = dv_get_alpha_fading_out(node->parent);
    } else if (dv_is_expanding(node->parent)) {
      // Fading in
      alpha = dv_get_alpha_fading_in(node->parent);
    }
    
  }
  // Draw path
  cairo_move_to(cr, xx, yy);
  cairo_line_to(cr, xx + w, yy);
  cairo_line_to(cr, xx + w, yy + h);
  cairo_line_to(cr, xx, yy + h);
  cairo_close_path(cr);

  // Draw node
  cairo_set_source_rgba(cr, c[0], c[1], c[2], c[3] * alpha);
  cairo_fill_preserve(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
  cairo_stroke(cr);
  cairo_restore(cr);
}

static void draw_bbox_node_r(cairo_t *cr, dv_dag_node_t *node) {
  /* Draw node */
  dv_check(node);
  if (!dv_is_union(node)
      || dv_is_shrinked(node) || dv_is_shrinking(node)) {
    draw_bbox_node_1(cr, node);
  }

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
    if (!dv_is_single(node)) {
      dv_check(node->head);
      is_single_node = 0;
    }
    break;
  default:
    dv_check(0);
    break;
  }
  if (!is_single_node) {
    // Recursive call
    if (!node->avoid_inward)
      draw_bbox_node_r(cr, node->head);
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
    draw_bbox_node_r(cr, u);
    break;
  case 2:
    u = (dv_dag_node_t *) node->links->top->item; // cont node
    v = (dv_dag_node_t *) node->links->top->next->item; // task node
    // Recursive call
    draw_bbox_node_r(cr, u);
    draw_bbox_node_r(cr, v);
    break;
  default:
    dv_check(0);
    break;
  }
}

static void draw_bbox_edge_1(cairo_t *cr, dv_dag_node_t *u, dv_dag_node_t *v) {
  draw_edge_1(cr, u, v);
#if 0
  if (u->y > v->y)
    return;
  double alpha = 1.0;
  if (dv_is_shrinking(u->parent))
    alpha = dv_get_alpha_fading_out(u->parent);
  else if (dv_is_expanding(u->parent))
    alpha = dv_get_alpha_fading_in(u->parent);
  cairo_save(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
  double x1, y1, x2, y2;
  x1 = u->x;
  y1 = u->y + u->dw;
  x2 = v->x;
  y2 = v->y;
  cairo_move_to(cr, x1, y1);
  cairo_line_to(cr, x1, y2);
  cairo_line_to(cr, x2, y2);
  cairo_stroke(cr);
  cairo_restore(cr);
#endif
}

static dv_dag_node_t * dv_dag_node_get_first(dv_dag_node_t *u) {
  dv_check(u);
  while (!dv_is_single(u)) {
    dv_check(u->head);
    u = u->head;
    dv_check(u);
  }
  return u;
}

static void draw_bbox_edge_last_r(cairo_t *cr, dv_dag_node_t *u, dv_dag_node_t *v) {
  if (u->avoid_inward) {
    draw_bbox_edge_1(cr, u, v);
    return;
  }
  dv_llist_iterate_init(u->tails);
  dv_dag_node_t *u_tail;
  while (u_tail = (dv_dag_node_t *) dv_llist_iterate_next(u->tails)) {
    if (dv_is_single(u_tail))
      draw_bbox_edge_1(cr, u_tail, v);
    else
      draw_bbox_edge_last_r(cr, u_tail, v);
  }  
}

static void draw_bbox_edge_r(cairo_t *cr, dv_dag_node_t *u) {
  if (!u) return;
  // Call head
  if (!dv_is_single(u)) {
    if (!u->avoid_inward)
      draw_bbox_edge_r(cr, u->head);
  }
  // Iterate links
  dv_dag_node_t * v;
  dv_llist_iterate_init(u->links);
  while (v = (dv_dag_node_t *) dv_llist_iterate_next(u->links)) {

    if (dv_is_single(u)) {
      
      if (dv_is_single(v))
        draw_bbox_edge_1(cr, u, v);
      else
        draw_bbox_edge_1(cr, u, dv_dag_node_get_first(v->head));
      
    } else {
    
      if (dv_is_single(v))
        draw_bbox_edge_last_r(cr, u, v);
      else
        draw_bbox_edge_last_r(cr, u, dv_dag_node_get_first(v->head));

    }
    draw_bbox_edge_r(cr, v);
    
  }
}


void dv_draw_bbox_dvdag(cairo_t *cr, dv_dag_t *G) {
  cairo_set_line_width(cr, DV_NODE_LINE_WIDTH);
  int i;
  // Draw nodes
  S->nd = 0;
  draw_bbox_node_r(cr, G->rt);
  // Draw edges
  draw_bbox_edge_r(cr, G->rt);
}


/*-----------------end of DAG BBox drawing functions-----------*/


