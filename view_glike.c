#include "dagviz.h"

/*-----------------Grid-like Layout functions-----------*/


/*-----------------Grid-related functions-----------*/

static void dv_grid_line_init(dv_grid_line_t * l) {
  l->l = 0;
  l->r = 0;
  dv_llist_init(l->L);
  l->c = 0;
}

static dv_grid_line_t * dv_grid_line_create() {
  dv_grid_line_t * l = (dv_grid_line_t *) dv_malloc( sizeof(dv_grid_line_t) );
  dv_grid_line_init(l);
  return l;
}

/*-----------------end of Grid-related functions-----------*/

static void bind_node_1(dv_dag_node_t *u, dv_grid_line_t *l) {
  u->vl = l;
  dv_llist_add(l->L, u);
}

static void dv_layout_bind_node(dv_dag_node_t * u, dv_grid_line_t * l) {
  // Bind itself
  if (l != NULL)
    bind_node_1(u, l);
  // Bind child nodes
  if (dv_is_union(u) && dv_is_inner_loaded(u)) {
    dv_check(u->head);
    if (!u->vl_in)
      u->vl_in = dv_grid_line_create();
    dv_layout_bind_node(u->head, u->vl_in);
  }

  // Call following links
  if (l == NULL)
    return;
  int count = dv_llist_size(u->links);
  dv_dag_node_t * v;
  dv_dag_node_t * vv;
  switch (count) {
  case 0:
    break;
  case 1:
    v = (dv_dag_node_t *) u->links->top->item;
    dv_layout_bind_node(v, l);
    break;
  case 2:
    v = (dv_dag_node_t *) u->links->top->item;
    vv = (dv_dag_node_t *) u->links->top->next->item;
    if (!l->r) {
      l->r = dv_grid_line_create();
      l->r->l = l;
    }
    if (!l->l) {
      l->l = dv_grid_line_create();
      l->l->r = l;
    }
    dv_layout_bind_node(v, l->r);
    dv_layout_bind_node(vv, l->l);
    break;
  default:
    dv_check(0);
    break;
  }

}

static double dv_layout_count_line_left(dv_view_t *V, dv_grid_line_t *);

static double dv_layout_count_line_right(dv_view_t *V, dv_grid_line_t *l) {
  dv_llist_iterate_init(l->L);
  dv_dag_node_t * node;
  double max = 0.0;
  while ( node = (dv_dag_node_t *) dv_llist_iterate_next(l->L) ) {
    if (dv_is_inward_callable(node)) {
      if (!node->vl_in || node->head->vl == NULL)
        dv_layout_bind_node(node, NULL);
      dv_grid_line_t * ll = node->vl_in;
      double num = 0.0;
      double gap = dv_view_calculate_gap(V, node);
      while (ll->r) {
        ll->rc = dv_layout_count_line_right(V, ll);
        ll->r->lc = dv_layout_count_line_left(V, ll->r);
        num += ll->rc + gap + ll->r->lc;
        ll = ll->r;
      }
      ll->rc = dv_layout_count_line_right(V, ll);
      num += ll->rc;
      node->rc = num;
      if (num > max) {
        max = num;
      }
    }
  }
  return max;
}

static double dv_layout_count_line_left(dv_view_t *V, dv_grid_line_t *l) {
  dv_llist_iterate_init(l->L);
  dv_dag_node_t * node;
  double max = 0.0;
  while ( node = (dv_dag_node_t *) dv_llist_iterate_next(l->L) ) {
    if (dv_is_inward_callable(node)) {
      if (!node->vl_in)
        dv_layout_bind_node(node, NULL);
      dv_grid_line_t * ll = node->vl_in;
      double num = 0.0;
      double gap = dv_view_calculate_gap(V, node);
      while (ll->l) {
        ll->lc = dv_layout_count_line_left(V, ll);
        ll->l->rc = dv_layout_count_line_right(V, ll->l);
        num += ll->lc + gap + ll->l->rc;
        ll = ll->l;
      }
      ll->lc = dv_layout_count_line_left(V, ll);
      num += ll->lc;
      node->lc = num;
      if (num > max)
        max = num;
    }
  }
  return max;
}

static double dv_layout_count_line_down(dv_view_t *V, dv_dag_node_t *node) {
  double c = 0.0;
  if (dv_is_inward_callable(node)) {
    
    // Call recursive head
    dv_check(node->head);
    dv_dag_node_t * hd = node->head;
    hd->dc = dv_layout_count_line_down(V, hd);
    c = hd->dc;
    
  }

  // Calculate gap
  double gap = 0.0;
  if (node->links->top)
    gap = dv_view_calculate_gap(V, node->parent);

  // Call following links
  dv_llist_iterate_init(node->links);
  dv_dag_node_t * n;
  double max = 0L;
  double num;
  while (n = (dv_dag_node_t *) dv_llist_iterate_next(node->links)) {
    n->dc = dv_layout_count_line_down(V, n);
    if (n->dc > max)
      max = n->dc;
  }
  
  return c + gap + max;
}

static void dv_layout_align_line_rightleft(dv_view_t *V, dv_dag_node_t *node) {
  dv_grid_line_t * l = node->vl_in;
  double gap = dv_view_calculate_gap(V, node);
  dv_grid_line_t * ll;
  dv_grid_line_t * lll;
  // Set c right
  ll = l;
  while (ll->r) {
    lll = ll->r;
    lll->c = ll->c + gap * DV_HDIS + (ll->rc + lll->lc) * DV_HDIS;
    ll = lll;
  }
  // Set c left
  ll = l;
  while (ll->l) {
    lll = ll->l;
    lll->c = ll->c - gap * DV_HDIS - (ll->lc + lll->rc) * DV_HDIS;
    ll = lll;
  }
  // Call recursive right
  ll = l;
  while (ll) {
    dv_llist_iterate_init(ll->L);
    dv_dag_node_t * n;
    while (n = (dv_dag_node_t *) dv_llist_iterate_next(ll->L))
      if (dv_is_inward_callable(n)) {
        n->vl_in->c = ll->c;
        dv_layout_align_line_rightleft(V, n);
      }
    ll = ll->r;
  }
  // Call recursive left
  ll = l->l;
  while (ll) {
    dv_llist_iterate_init(ll->L);
    dv_dag_node_t * n;
    while (n = (dv_dag_node_t *) dv_llist_iterate_next(ll->L))
      if (dv_is_inward_callable(n)) {
        n->vl_in->c = ll->c;
        dv_layout_align_line_rightleft(V, n);
      }
    ll = ll->l;
  }
}

static void dv_layout_align_line_down(dv_dag_node_t *node) {
  dv_check(node);
  dv_dag_node_t * n;
  double max = 0.0;
  dv_llist_iterate_init(node->links);
  while (n = (dv_dag_node_t *) dv_llist_iterate_next(node->links)) {
    if (n->dc > max)
      max = n->dc;
  }
  dv_llist_iterate_init(node->links);
  while (n = (dv_dag_node_t *) dv_llist_iterate_next(node->links)) {
    n->c = node->c + (node->dc - max) * DV_VDIS;
    dv_layout_align_line_down(n);
  }
  if (dv_is_inward_callable(node)) {
    n = node->head;
    n->c = node->c;
    dv_layout_align_line_down(n);
  }
}

void dv_view_layout_glike(dv_view_t *V) {
  dv_dag_t *D = V->D;
  
  if (!D->rt->vl) {
    // Bind nodes to lines
    dv_grid_line_init(D->rvl);
    dv_layout_bind_node(D->rt, D->rvl);
  }

  // Count lines
  D->rvl->rc = dv_layout_count_line_right(V, D->rvl);
  D->rvl->lc = dv_layout_count_line_left(V, D->rvl);
  D->rt->dc = dv_layout_count_line_down(V, D->rt);

  // Align lines
  D->rvl->c = 0.0;
  dv_layout_align_line_rightleft(V, D->rt);
  D->rt->c = 0.0;
  dv_layout_align_line_down(D->rt);

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
  double x = node->vl->c;
  double y = node->c;
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
      xx = x - node->lc * DV_HDIS - DV_RADIUS - margin;
      yy = y - DV_RADIUS - margin;
      dv_dag_node_t * hd = node->head;
      w = (node->lc + node->rc) * DV_HDIS + 2 * (DV_RADIUS + margin);
      h = hd->dc * DV_VDIS + 2 * (DV_RADIUS + margin);
      
    } else {
      
      // Normal-sized box
      xx = x - DV_RADIUS;
      yy = y - DV_RADIUS;
      w = 2 * DV_RADIUS;
      h = 2 * DV_RADIUS;
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
    cairo_arc(cr, x, y, DV_RADIUS, 0.0, 2*M_PI);
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
  dv_dag_node_t * v;
  dv_llist_iterate_init(u->links);
  while (v = (dv_dag_node_t *) dv_llist_iterate_next(u->links)) {
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
  dv_dag_node_t * v;
  dv_llist_iterate_init(u->links);
  while (v = (dv_dag_node_t *) dv_llist_iterate_next(u->links)) {

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
      
      dv_llist_iterate_init(u->tails);
      while (u_tail = (dv_dag_node_t *) dv_llist_iterate_next(u->tails)) {
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
  // Layout
  dv_view_layout_glike(V);
  // Draw nodes
  dv_view_draw_glike_node_r(V, cr, V->D->rt);
  // Draw edges
  dv_view_draw_glike_edge_r(V, cr, V->D->rt);
}

/*-----------------end of Grid-like Drawing functions-----------*/
