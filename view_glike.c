#include "dagviz.h"


/*-----------------Grid-like layout functions-----------*/

static void dv_view_layout_glike_node(dv_view_t *V, dv_dag_node_t *node) {
  /* Calculate inward */
  if (dv_is_inward_callable(node)) {
    dv_check(node->head);
    // node's head's outward
    node->head->xpre = 0.0;
    node->head->y = node->y;
    // Recursive call
    dv_view_layout_glike_node(V, node->head);
    // node's inward
    node->lw = node->head->link_lw;
    node->rw = node->head->link_rw;
    node->dw = node->head->link_dw;
  } else {
    // node's inward
    node->lw = DV_RADIUS;
    node->rw = DV_RADIUS;
    node->dw = 2 * DV_RADIUS;
  }
    
  /* Calculate link-along */
  int n_links = dv_llist_size(node->links);
  dv_dag_node_t *u, *v; // linked nodes
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
    gap = dv_view_calculate_gap(V, node->parent);
    // node's linked u's outward
    u->xpre = 0.0;
    u->y = node->y + (node->dw + DV_VDIS) * gap;
    // Recursive call
    dv_view_layout_glike_node(V, u);
    // node's link-along
    node->link_lw = dv_max(node->lw, u->link_lw);
    node->link_rw = dv_max(node->rw, u->link_rw);
    node->link_dw = (node->dw + DV_VDIS) * gap + u->link_dw;
    break;
  case 2:
    u = (dv_dag_node_t *) node->links->top->item; // cont node
    v = (dv_dag_node_t *) node->links->top->next->item; // task node
    // node & u,v's gap
    gap = dv_view_calculate_gap(V, node->parent);
    // node's linked u,v's outward
    u->y = node->y + (node->dw + DV_VDIS) * gap;
    v->y = node->y + (node->dw + DV_VDIS) * gap;
    // Recursive call
    dv_view_layout_glike_node(V, u);
    dv_view_layout_glike_node(V, v);
    
    // node's linked u,v's outward
    double hgap = gap * DV_HDIS;
    // u
    u->xpre = (u->link_lw - DV_RADIUS) + hgap;
    if (dv_llist_size(u->links) == 2)
      u->xpre = - ((dv_dag_node_t *) dv_llist_get(u->links, 1))->xpre;
    // v
    v->xpre = (v->link_rw - DV_RADIUS) + hgap;
    if (dv_llist_size(u->links) == 2)
      v->xpre += (u->link_lw - DV_RADIUS) - u->xpre;
    v->xpre = - v->xpre;
    
    // node's link-along
    node->link_lw = - v->xpre + v->link_lw;
    node->link_rw = u->xpre + u->link_rw;
    node->link_dw = (node->dw + DV_VDIS) * gap + dv_max(u->link_dw, v->link_dw);
    break;
  default:
    dv_check(0);
    break;
  }  
}

static void dv_view_layout_glike_node_2nd(dv_dag_node_t *node) {
  /* Calculate inward */
  if (dv_is_inward_callable(node)) {
    dv_check(node->head);
    // node's head's outward
    node->head->xp = 0.0;
    node->head->x = node->x;
    // Recursive call
    dv_view_layout_glike_node_2nd(node->head);
  }
    
  /* Calculate link-along */
  int n_links = dv_llist_size(node->links);
  dv_dag_node_t *u, *v; // linked nodes
  switch (n_links) {
  case 0:
    break;
  case 1:
    u = (dv_dag_node_t *) node->links->top->item;
    // node's linked u's outward
    u->xp = u->xpre + node->xp;
    u->x = u->xp + u->parent->x;
    // Recursive call
    dv_view_layout_glike_node_2nd(u);
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
    dv_view_layout_glike_node_2nd(u);
    dv_view_layout_glike_node_2nd(v);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

void dv_view_layout_glike(dv_view_t *V) {
  dv_dag_t *D = V->D;

  // Relative coord
  D->rt->xpre = 0.0; // pre-based
  D->rt->y = 0.0;
  dv_view_layout_glike_node(V, D->rt);

  // Absolute coord
  D->rt->xp = 0.0; // parent-based
  D->rt->x = 0.0;
  dv_view_layout_glike_node_2nd(D->rt);
  
}

/*-----------------end of Grid-like layout functions-----------*/



/*-----------------Grid-like Drawing functions-----------*/

static void dv_view_draw_glike_node_1(dv_view_t *V, cairo_t *cr, dv_dag_node_t *node) {
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  // Count node drawn
  S->nd++;
  if (node->d > D->cur_d)
    D->cur_d = node->d;
  if (dv_is_union(node) && dv_is_inner_loaded(node)
      && dv_is_shrinked(node)
      && node->d < D->cur_d_ex)
    D->cur_d_ex = node->d;
  // Node color
  double x = node->x;
  double y = node->y;
  double c[4];
  dr_pi_dag_node *pi = dv_pidag_get_node(D->P, node);
  dv_lookup_color(pi, S->nc, c, c+1, c+2, c+3);
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
        alpha = dv_view_get_alpha_fading_out(V, node);
        margin = dv_view_get_alpha_fading_in(V, node);
      } else {
        // Fading in
        alpha = dv_view_get_alpha_fading_in(V, node);
        margin = dv_view_get_alpha_fading_out(V, node);
      }
      // Large-sized box
      margin *= DV_UNION_NODE_MARGIN;
      xx = x - node->lw - margin;
      yy = y - margin;
      w = node->lw + node->rw + 2 * margin;
      h = node->dw + 2 * margin;
      
    } else {
      
      // Normal-sized box
      xx = x - node->lw;//DV_RADIUS;
      yy = y;
      w = node->lw + node->rw;//2 * DV_RADIUS;
      h = node->dw;//2 * DV_RADIUS;
      alpha = 1.0;
      if (dv_is_shrinking(node->parent)) {
        // Fading out
        alpha = dv_view_get_alpha_fading_out(V, node->parent);
      } else if (dv_is_expanding(node->parent)) {
        // Fading in
        alpha = dv_view_get_alpha_fading_in(V, node->parent);
      }
      
    }

    // Box
    cairo_move_to(cr, xx, yy);
    cairo_line_to(cr, xx + w, yy);
    cairo_line_to(cr, xx + w, yy + h);
    cairo_line_to(cr, xx, yy + h);
    cairo_close_path(cr);
    
  } else {
    
    // Normal-sized circle
    cairo_arc(cr, x, y + DV_RADIUS, DV_RADIUS, 0.0, 2*M_PI);
    alpha = 1.0;
    if (dv_is_shrinking(node->parent)) {
      // Fading out
      alpha = dv_view_get_alpha_fading_out(V, node->parent);
    } else if (dv_is_expanding(node->parent)) {
      // Fading in
      alpha = dv_view_get_alpha_fading_in(V, node->parent);
    }
    
  }
  
  // Draw node
  cairo_set_source_rgba(cr, c[0], c[1], c[2], c[3] * alpha);
  cairo_fill_preserve(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
  cairo_stroke(cr);
  cairo_restore(cr);
}

static void dv_view_draw_glike_node_r(dv_view_t *V, cairo_t *cr, dv_dag_node_t *u) {
  // Count node
  V->S->ndh++;
  if (!u || !dv_is_set(u))
    return;
  /* Draw node */
  if (!dv_is_union(u) || !dv_is_inner_loaded(u)
      || dv_is_shrinked(u) || dv_is_shrinking(u)) {
    dv_view_draw_glike_node_1(V, cr, u);
  }
  /* Call inward */
  if (!dv_is_single(u)) {
		dv_view_draw_glike_node_r(V, cr, u->head);
  }
  /* Call link-along */
  dv_dag_node_t * v = NULL;
  while (v = (dv_dag_node_t *) dv_llist_iterate_next(u->links, v)) {
    dv_view_draw_glike_node_r(V, cr, v);    
  }
}

static void dv_view_draw_glike_edge_1(dv_view_t *V, cairo_t *cr, dv_dag_node_t *u, dv_dag_node_t *v) {
  dv_view_draw_edge_1(V, cr, u, v);
}

static dv_dag_node_t * dv_dag_node_get_first(dv_dag_node_t *u) {
  dv_check(u);
  while (!dv_is_single(u)) {
    dv_check(u->head);
    u = u->head;
  }
  return u;
}

static dv_dag_node_t * dv_dag_node_get_last(dv_dag_node_t *u) {
  dv_check(u);
  while (!dv_is_single(u)) {
    dv_check(u->tails->top);
    u = (dv_dag_node_t *) u->tails->top->item;
    dv_check(u);
  }
  return u;
}

static void dv_view_draw_glike_edge_r(dv_view_t *V, cairo_t *cr, dv_dag_node_t *u) {
  if (!u || !dv_is_set(u))
    return;
  /* Call inward */
  if (!dv_is_single(u)) {
		dv_view_draw_glike_edge_r(V, cr, u->head);
  }
  /* Call link-along */
  dv_dag_node_t * v = NULL;
  while (v = (dv_dag_node_t *) dv_llist_iterate_next(u->links, v)) {

    dv_dag_node_t *u_tail, *v_head;
    if (dv_is_single(u)) {
      
      if (dv_is_single(v)) {
        dv_view_draw_glike_edge_1(V, cr, u, v);        
      } else {
        v_head = v->head;
				dv_dag_node_t * v_first = dv_dag_node_get_first(v_head);
				dv_view_draw_glike_edge_1(V, cr, u, v_first);
        
      }
      
    } else {
      
      u_tail = NULL;
      while (u_tail = (dv_dag_node_t *) dv_llist_iterate_next(u->tails, u_tail)) {
        dv_dag_node_t * u_last = dv_dag_node_get_last(u_tail);
        
        if (dv_is_single(v)) {
          dv_view_draw_glike_edge_1(V, cr, u_last, v);
        } else {
          
          v_head = v->head;
					dv_dag_node_t * v_first = dv_dag_node_get_first(v_head);
					dv_view_draw_glike_edge_1(V, cr, u_last, v_first);
          
        }
        
      }
      
    }
    dv_view_draw_glike_edge_r(V, cr, v);
    
  }
}

void dv_view_draw_glike(dv_view_t *V, cairo_t *cr) {
  cairo_set_line_width(cr, 2.0);
  int i;
  // Draw nodes
  dv_view_draw_glike_node_r(V, cr, V->D->rt);
  // Draw edges
  dv_view_draw_glike_edge_r(V, cr, V->D->rt);
}

/*-----------------end of Grid-like Drawing functions-----------*/
