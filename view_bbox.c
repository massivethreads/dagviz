#include "dagviz.h"

/*-----------------BBox layout functions-----------*/

static double dv_view_calculate_hgap(dv_view_t *V, dv_dag_node_t *node) {
  double gap = dv_view_calculate_gap(V, node);
  double hgap = gap * DV_HDIS;
  return hgap;
}

double dv_view_calculate_hsize(dv_view_t *V, dv_dag_node_t *node) {
  double gap = dv_view_calculate_gap(V, node->parent);
  double hsize = gap * DV_RADIUS;
  return hsize;
}

double dv_view_calculate_vresize(dv_view_t *V, double val) {
  dv_view_status_t *S = V->S;
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

static double dv_view_calculate_vgap(dv_view_t *V, dv_dag_node_t *parent, dv_dag_node_t *node1, dv_dag_node_t *node2) {
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  dr_pi_dag_node * pi1 = dv_pidag_get_node(D->P, node1);
  dr_pi_dag_node * pi2 = dv_pidag_get_node(D->P, node2);
  double gap = dv_view_calculate_gap(V, parent);
  double vgap;
  if (!S->frombt) {
    // begin - end
    double time_gap = dv_view_calculate_vresize(V, pi2->info.start.t - pi1->info.end.t);
    vgap = gap * time_gap;
  } else {
    // from beginning
    double time1 = dv_view_calculate_vresize(V, pi1->info.end.t - D->bt);
    double time2 = dv_view_calculate_vresize(V, pi2->info.start.t - D->bt);
    vgap = gap * (time2 - time1);
  }
  return vgap;
}

double dv_view_calculate_vsize(dv_view_t *V, dv_dag_node_t *node) {
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  dr_pi_dag_node * pi = dv_pidag_get_node(D->P, node);
  double gap = 1.0;//dv_view_calculate_gap(V, node->parent);
  double vsize;
  if (!S->frombt) {
    // begin - end
    double time_gap = dv_view_calculate_vresize(V, pi->info.end.t - pi->info.start.t);
    vsize = gap * time_gap;
  } else {
    // from beginning
    double time1 = dv_view_calculate_vresize(V, pi->info.start.t - D->bt);
    double time2 = dv_view_calculate_vresize(V, pi->info.end.t - D->bt);
    vsize = gap * (time2 - time1);
  }
  return vsize;
}

/*
static double dv_view_calculate_vsize_pure(dv_view_t *V, dv_dag_node_t *node) {
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  dr_pi_dag_node * pi = dv_pidag_get_node(D->P, node);
  double vsize;
  if (!S->frombt) {
    double time = pi->info.end.t - pi->info.start.t;
    vsize = dv_view_calculate_vresize(V, time);
  } else {
    double time1 = dv_view_calculate_vresize(V, pi->info.start.t - D->bt);
    double time2 = dv_view_calculate_vresize(V, pi->info.end.t - D->bt);
    double vsize = time2 - time1;
  }
  return vsize;
}
*/

static dv_dag_node_t * dv_layout_node_get_last_tail(dv_view_t *V, dv_dag_node_t *node) {
  dv_dag_node_t * ret = 0;
  dv_dag_node_t * tail = NULL;
  while (tail = (dv_dag_node_t *) dv_llist_iterate_next(node->tails, tail)) {
    dr_pi_dag_node * ret_pi = dv_pidag_get_node(V->D->P, ret);
    dr_pi_dag_node * tail_pi = dv_pidag_get_node(V->D->P, tail);
    if (!ret || ret_pi->info.end.t < tail_pi->info.end.t)
      ret = tail;
  }
  return ret;
}

static double dv_layout_node_get_node_xp(dv_dag_node_t *node) {
  int lt = 1;
  dv_dag_node_t * u = node;
  double dis = 0.0;
  while (u) {
    dis += u->c[lt].xpre;
    u = u->pre;
  }
  return dis;
}

static double dv_layout_node_get_last_tail_xp_r(dv_view_t *V, dv_dag_node_t *node) {
  dv_check(node);
  double ret = 0.0;
  dv_dag_node_t * last_tail = 0;
  if (!dv_is_single(node)) {
    last_tail = dv_layout_node_get_last_tail(V, node);
    if (last_tail) {
      ret += dv_layout_node_get_node_xp(last_tail);
      ret += dv_layout_node_get_last_tail_xp_r(V, last_tail);
    }
  }
  return ret;
}

static void dv_view_layout_bbox_node(dv_view_t *V, dv_dag_node_t *node) {
  int lt = 1;
  dv_node_coordinate_t *nodeco = &node->c[lt];
  /* Calculate inward */
  if (dv_is_inward_callable(node)) {
    dv_check(node->head);
    // node's head's outward
    dv_node_coordinate_t *headco = &node->head->c[lt];
    headco->xpre = 0.0;
    headco->y = nodeco->y;
    // Recursive call
    dv_view_layout_bbox_node(V, node->head);
    // node's inward
    nodeco->lw = headco->link_lw;
    nodeco->rw = headco->link_rw;
    nodeco->dw = headco->link_dw;
    // for enhancing expand/collapse animation
    if (nodeco->lw < DV_RADIUS)
      nodeco->lw = DV_RADIUS;
    if (nodeco->rw < DV_RADIUS)
      nodeco->rw = DV_RADIUS;
    double vsize = dv_view_calculate_vsize(V, node);
    if (nodeco->dw < vsize)
      nodeco->dw = vsize;
  } else {
    // node's inward
    nodeco->lw = DV_RADIUS;//dv_view_calculate_hsize(V, node);
    nodeco->rw = DV_RADIUS;//dv_view_calculate_hsize(V, node);
    nodeco->dw = dv_view_calculate_vsize(V, node);
  }
    
  /* Calculate link-along */
  int n_links = dv_llist_size(node->links);
  dv_dag_node_t *u, *v; // linked nodes
  dv_node_coordinate_t *uco, *vco;
  double g, gap, ugap, vgap, hgap;
  switch (n_links) {
  case 0:
    // node's link-along
    nodeco->link_lw = nodeco->lw;
    nodeco->link_rw = nodeco->rw;
    nodeco->link_dw = nodeco->dw;
    break;
  case 1:
    u = (dv_dag_node_t *) node->links->top->item;
    uco = &u->c[lt];
    // node & u's gap
    g = dv_view_calculate_gap(V, node->parent);
    ugap = dv_view_calculate_vgap(V, node->parent, node, u);
    // node's linked u's outward    
    uco->xpre = dv_layout_node_get_last_tail_xp_r(V, node);
    uco->y = nodeco->y + nodeco->dw * g + ugap;
    // Recursive call
    dv_view_layout_bbox_node(V, u);
    // node's link-along
    nodeco->link_lw = dv_max(nodeco->lw, uco->link_lw - uco->xpre);
    nodeco->link_rw = dv_max(nodeco->rw, uco->link_rw + uco->xpre);
    nodeco->link_dw = nodeco->dw * g + ugap + uco->link_dw;
    break;
  case 2:
    u = (dv_dag_node_t *) node->links->top->item; // cont node
    v = (dv_dag_node_t *) node->links->top->next->item; // task node
    uco = &u->c[lt];
    vco = &v->c[lt];
    // node & u,v's gap
    g = dv_view_calculate_gap(V, node->parent);
    ugap = dv_view_calculate_vgap(V, node->parent, node, u);
    vgap = dv_view_calculate_vgap(V, node->parent, node, v);
    // node's linked u,v's outward
    uco->y = nodeco->y + nodeco->dw * g + ugap;
    vco->y = nodeco->y + nodeco->dw * g + vgap;
    // Recursive call
    dv_view_layout_bbox_node(V, u);
    dv_view_layout_bbox_node(V, v);
    
    // node's linked u,v's outward
    hgap = g * DV_HDIS;
    // u
    uco->xpre = (uco->link_lw - DV_RADIUS) + hgap;
    if (dv_llist_size(u->links) == 2)
      uco->xpre = - ((dv_dag_node_t *) dv_llist_get(u->links, 1))->c[lt].xpre;
    // v
    vco->xpre = (vco->link_rw - DV_RADIUS) + hgap;
    if (dv_llist_size(u->links) == 2)
      vco->xpre += (uco->link_lw - DV_RADIUS) - uco->xpre;
    vco->xpre = - vco->xpre;
    
    // node's link-along
    nodeco->link_lw = - vco->xpre + vco->link_lw;
    nodeco->link_rw = uco->xpre + uco->link_rw;
    nodeco->link_dw = nodeco->dw * g + dv_max(ugap + uco->link_dw, vgap + vco->link_dw);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

static void dv_view_layout_bbox_node_2nd(dv_dag_node_t *node) {
  int lt = 1;
  dv_node_coordinate_t *nodeco = &node->c[lt];
  /* Calculate inward */
  if (dv_is_inward_callable(node)) {
    dv_check(node->head);
    // node's head's outward
    dv_node_coordinate_t *headco = &node->head->c[lt];
    headco->xp = 0.0;
    headco->x = nodeco->x;
    // Recursive call
    dv_view_layout_bbox_node_2nd(node->head);
  }
    
  /* Calculate link-along */
  int n_links = dv_llist_size(node->links);
  dv_dag_node_t *u, *v; // linked nodes
  dv_node_coordinate_t *uco, *vco;
  switch (n_links) {
  case 0:
    break;
  case 1:
    u = (dv_dag_node_t *) node->links->top->item;
    uco = &u->c[lt];
    // node's linked u's outward
    uco->xp = uco->xpre + nodeco->xp;
    uco->x = uco->xp + u->parent->c[lt].x;
    // Recursive call
    dv_view_layout_bbox_node_2nd(u);
    break;
  case 2:
    u = (dv_dag_node_t *) node->links->top->item; // cont node
    v = (dv_dag_node_t *) node->links->top->next->item; // task node
    uco = &u->c[lt];
    vco = &v->c[lt];
    // node's linked u,v's outward
    uco->xp = uco->xpre + nodeco->xp;
    uco->x = uco->xp + u->parent->c[lt].x;
    vco->xp = vco->xpre + nodeco->xp;
    vco->x = vco->xp + v->parent->c[lt].x;
    // Recursive call
    dv_view_layout_bbox_node_2nd(u);
    dv_view_layout_bbox_node_2nd(v);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

void dv_view_layout_bbox(dv_view_t *V) {
  dv_dag_t *D = V->D;
  int lt = 1;
  dv_node_coordinate_t *rtco = &D->rt->c[lt];

  // Relative coord
  rtco->xpre = 0.0; // pre-based
  rtco->y = 0.0;
  dv_view_layout_bbox_node(V, D->rt);

  // Absolute coord
  rtco->xp = 0.0; // parent-based
  rtco->x = 0.0;
  dv_view_layout_bbox_node_2nd(D->rt);

  // Check
  //print_layout(D);

}

/*-----------------end of BBox layout functions-----------*/


/*-----------------DAG BBox Drawing functions-----------*/

static void dv_view_draw_bbox_node_1(dv_view_t *V, cairo_t *cr, dv_dag_node_t *node) {
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  int lt = 1;
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
  if (dv_is_union(node) && dv_is_inner_loaded(node)) {

    if (dv_is_expanding(node) || dv_is_shrinking(node)) {

      double margin = 1.0;
      if (dv_is_expanding(node)) {
        // Fading out
        alpha = dv_view_get_alpha_fading_out(V, node);
        margin = dv_view_get_alpha_fading_in(V, node);
      } else {
        // Fading in
        alpha = dv_view_get_alpha_fading_in(V, node);
        margin = dv_view_get_alpha_fading_out(V, node);
      }
      // Large-sized box
      margin *= DV_UNION_NODE_MARGIN;
      xx = x - nodeco->lw - margin;
      yy = y - margin;
      w = nodeco->lw + nodeco->rw + 2 * margin;
      h = nodeco->dw + 2 * margin;
      
    } else {
      
      // Normal-sized box
      xx = x - nodeco->lw;
      yy = y;
      w = nodeco->lw + nodeco->rw;
      h = nodeco->dw;
      alpha = 1.0;
      if (dv_is_shrinking(node->parent)) {
        // Fading out
        alpha = dv_view_get_alpha_fading_out(V, node->parent);
      } else if (dv_is_expanding(node->parent)) {
        // Fading in
        alpha = dv_view_get_alpha_fading_in(V, node->parent);
      }
      
    }

  } else {
    
    // Normal-sized box (terminal node)
    xx = x - nodeco->lw;
    yy = y;
    w = nodeco->lw + nodeco->rw;
    h = nodeco->dw;
    alpha = 1.0;
    if (dv_is_shrinking(node->parent)) {
      // Fading out
      alpha = dv_view_get_alpha_fading_out(V, node->parent);
    } else if (dv_is_expanding(node->parent)) {
      // Fading in
      alpha = dv_view_get_alpha_fading_in(V, node->parent);
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

static void dv_view_draw_bbox_node_r(dv_view_t *V, cairo_t *cr, dv_dag_node_t *node) {
  // Count node
  V->S->ndh++;
  if (!node || !dv_is_set(node))
    return;
  /* Draw node */
  if (!dv_is_union(node) || !dv_is_inner_loaded(node)
      || dv_is_shrinked(node) || dv_is_shrinking(node)) {
    dv_view_draw_bbox_node_1(V, cr, node);
  }
  /* Call inward */
  if (!dv_is_single(node)) {
    dv_view_draw_bbox_node_r(V, cr, node->head);
  }
  /* Call link-along */
  dv_dag_node_t * u = NULL;
  while (u = (dv_dag_node_t *) dv_llist_iterate_next(node->links, u)) {
    dv_view_draw_bbox_node_r(V, cr, u);
  }
}

static void dv_view_draw_bbox_edge_1(dv_view_t *V, cairo_t *cr, dv_dag_node_t *u, dv_dag_node_t *v) {
  dv_view_draw_edge_1(V, cr, u, v);
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

static void dv_view_draw_bbox_edge_last_r(dv_view_t *V, cairo_t *cr, dv_dag_node_t *u, dv_dag_node_t *v) {
  dv_dag_node_t *u_tail = NULL;
  while (u_tail = (dv_dag_node_t *) dv_llist_iterate_next(u->tails, u_tail)) {
    if (dv_is_single(u_tail))
      dv_view_draw_bbox_edge_1(V, cr, u_tail, v);
    else
      dv_view_draw_bbox_edge_last_r(V, cr, u_tail, v);
  }  
}

static void dv_view_draw_bbox_edge_r(dv_view_t *V, cairo_t *cr, dv_dag_node_t *u) {
  if (!u || !dv_is_set(u))
    return;
  // Call head
  if (!dv_is_single(u)) {
    dv_view_draw_bbox_edge_r(V, cr, u->head);
  }
  // Iterate links
  dv_dag_node_t * v = NULL;
  while (v = (dv_dag_node_t *) dv_llist_iterate_next(u->links, v)) {

    if (dv_is_single(u)) {
      
      if (dv_is_single(v))
        dv_view_draw_bbox_edge_1(V, cr, u, v);
      else
        dv_view_draw_bbox_edge_1(V, cr, u, dv_dag_node_get_first(v->head));
      
    } else {
    
      if (dv_is_single(v))
        dv_view_draw_bbox_edge_last_r(V, cr, u, v);
      else
        dv_view_draw_bbox_edge_last_r(V, cr, u, dv_dag_node_get_first(v->head));

    }
    dv_view_draw_bbox_edge_r(V, cr, v);
    
  }
}

void dv_view_draw_bbox(dv_view_t *V, cairo_t *cr) {
  cairo_set_line_width(cr, DV_NODE_LINE_WIDTH);
  int i;
  // Draw nodes
  dv_view_draw_bbox_node_r(V, cr, V->D->rt);
  // Draw edges
  dv_view_draw_bbox_edge_r(V, cr, V->D->rt);
}


/*-----------------end of DAG BBox drawing functions-----------*/


