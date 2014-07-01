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
  bind_node_1(u, l);
  // Bind child nodes
  switch (u->pi->info.kind) {
  case dr_dag_node_kind_wait_tasks:
  case dr_dag_node_kind_end_task:
  case dr_dag_node_kind_create_task:
    break;
  case dr_dag_node_kind_section:
  case dr_dag_node_kind_task:
    if (dv_is_union(u)) {
      
      dv_check(u->head);
      if (!u->vl_in)
        u->vl_in = dv_grid_line_create();
      dv_layout_bind_node(u->head, u->vl_in);
      
    }
    break;
  default:
    dv_check(0);
    break;
  }

  // Call following links
  int count = 0;
  dv_llist_cell_t * c = u->links->top;
  while (c) {
    count++;
    c = c->next;
  }
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

static double dv_layout_count_line_left(dv_grid_line_t *);

static double dv_layout_count_line_right(dv_grid_line_t *l) {
  dv_llist_iterate_init(l->L);
  dv_dag_node_t * node;
  double max = 0.0;
  while ( node = (dv_dag_node_t *) dv_llist_iterate_next(l->L) ) {
    if (dv_is_union(node)
        && ( !dv_is_shrinked(node) || dv_is_expanding(node) )) {
      dv_grid_line_t * ll = node->vl_in;
      double num = 0.0;
      double gap = dv_layout_calculate_gap(node);
      while (ll->r) {
        ll->rc = dv_layout_count_line_right(ll);
        ll->r->lc = dv_layout_count_line_left(ll->r);
        num += ll->rc + gap + ll->r->lc;
        ll = ll->r;
      }
      ll->rc = dv_layout_count_line_right(ll);
      num += ll->rc;
      node->rc = num;
      if (num > max) {
        max = num;
      }
    }
  }
  return max;
}

static double dv_layout_count_line_left(dv_grid_line_t *l) {
  dv_llist_iterate_init(l->L);
  dv_dag_node_t * node;
  double max = 0.0;
  while ( node = (dv_dag_node_t *) dv_llist_iterate_next(l->L) ) {
    if (dv_is_union(node)
        && ( !dv_is_shrinked(node) || dv_is_expanding(node) )) {
      dv_grid_line_t * ll = node->vl_in;
      double num = 0.0;
      double gap = dv_layout_calculate_gap(node);
      while (ll->l) {
        ll->lc = dv_layout_count_line_left(ll);
        ll->l->rc = dv_layout_count_line_right(ll->l);
        num += ll->lc + gap + ll->l->rc;
        ll = ll->l;
      }
      ll->lc = dv_layout_count_line_left(ll);
      num += ll->lc;
      node->lc = num;
      if (num > max)
        max = num;
    }
  }
  return max;
}

static double dv_layout_count_line_down(dv_dag_node_t *node) {
  double c = 0.0;
  switch (node->pi->info.kind) {
  case dr_dag_node_kind_wait_tasks:
  case dr_dag_node_kind_end_task:
  case dr_dag_node_kind_create_task:
    break;
  case dr_dag_node_kind_section:
  case dr_dag_node_kind_task:
    if (dv_is_union(node)
        && ( !dv_is_shrinked(node) || dv_is_expanding(node) )) {

      // Call recursive head
      dv_check(node->head);
      dv_dag_node_t * hd = node->head;
      hd->dc = dv_layout_count_line_down(hd);
      c = hd->dc;
      
    }
    break;
  default:
    dv_check(0);
    break;
  }

  // Calculate gap
  double gap = 0.0;
  if (node->links->top)
    gap = dv_layout_calculate_gap(node->parent);

  // Call following links
  dv_llist_iterate_init(node->links);
  dv_dag_node_t * n;
  double max = 0L;
  double num;
  while (n = (dv_dag_node_t *) dv_llist_iterate_next(node->links)) {
    n->dc = dv_layout_count_line_down(n);
    if (n->dc > max)
      max = n->dc;
  }
  
  return c + gap + max;
}

static void dv_layout_align_line_rightleft(dv_dag_node_t *node) {
  dv_grid_line_t * l = node->vl_in;
  double gap = dv_layout_calculate_gap(node);
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
      if (dv_is_union(n)
          && ( !dv_is_shrinked(n) || dv_is_expanding(n) )) {
        n->vl_in->c = ll->c;
        dv_layout_align_line_rightleft(n);
      }
    ll = ll->r;
  }
  // Call recursive left
  ll = l->l;
  while (ll) {
    dv_llist_iterate_init(ll->L);
    dv_dag_node_t * n;
    while (n = (dv_dag_node_t *) dv_llist_iterate_next(ll->L))
      if (dv_is_union(n)
          && ( !dv_is_shrinked(n) || dv_is_expanding(n) )) {
        n->vl_in->c = ll->c;
        dv_layout_align_line_rightleft(n);
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
  if (dv_is_union(node)
      && ( !dv_is_shrinked(node) || dv_is_expanding(node) )) {
    n = node->head;
    n->c = node->c;
    dv_layout_align_line_down(n);
  }
}

void dv_layout_glike_dvdag(dv_dag_t *G) {

  if (!G->rt->vl) {
    // Bind nodes to lines
    dv_grid_line_init(G->rvl);
    dv_layout_bind_node(G->rt, G->rvl);
  }

  // Count lines
  G->rvl->rc = dv_layout_count_line_right(G->rvl);
  G->rvl->lc = dv_layout_count_line_left(G->rvl);
  G->rt->dc = dv_layout_count_line_down(G->rt);

  // Align lines
  G->rvl->c = 0.0;
  dv_layout_align_line_rightleft(G->rt);
  G->rt->c = 0.0;
  dv_layout_align_line_down(G->rt);

}

/*-----------------end of Grid-like layout functions-----------*/



/*-----------------Gridlike Drawing functions-----------*/

static void draw_glike_node_1(cairo_t *cr, dv_dag_node_t *node) {
  // Count node drawn
  S->nd++;
  if (node->d > G->cur_d)
    G->cur_d = node->d;
  if (dv_is_union(node) && dv_is_shrinked(node)
      && node->d < G->cur_d_ex)
    G->cur_d_ex = node->d;
  // Node color
  double x = node->vl->c;
  double y = node->c;
  double c[4];
  dv_lookup_color(node, c, c+1, c+2, c+3);
  // Alpha
  double alpha = 1.0;
  // Draw path
  cairo_save(cr);
  cairo_new_path(cr);
  if (dv_is_union(node)) {

    double xx, yy, w, h;
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
      xx = x - node->lc * DV_HDIS - DV_RADIUS - margin;
      yy = y - DV_RADIUS - margin;
      dv_dag_node_t * hd = node->head;
      h = hd->dc * DV_VDIS + 2 * (DV_RADIUS + margin);
      w = (node->lc + node->rc) * DV_HDIS + 2 * (DV_RADIUS + margin);
      
    } else {
      
      // Normal-sized box
      xx = x - DV_RADIUS;
      yy = y - DV_RADIUS;
      w = 2 * DV_RADIUS;
      h = 2 * DV_RADIUS;
      alpha = 1.0;
      if (dv_is_shrinking(node->parent)) {
        // Fading out
        alpha = dv_get_alpha_fading_out(node->parent);
      } else if (dv_is_expanding(node->parent)) {
        // Fading in
        alpha = dv_get_alpha_fading_in(node->parent);
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
      alpha = dv_get_alpha_fading_out(node->parent);
    } else if (dv_is_expanding(node->parent)) {
      // Fading in
      alpha = dv_get_alpha_fading_in(node->parent);
    }
    
  }
  
  // Draw node
  cairo_set_source_rgba(cr, c[0], c[1], c[2], c[3] * alpha);
  cairo_fill_preserve(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
  cairo_stroke(cr);
  cairo_restore(cr);
}

static void draw_grid_vl(cairo_t *cr, dv_grid_line_t *l, double y1, double y2) {
  double x;
  x = l->c;
  cairo_save(cr);
  cairo_new_path(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0 ,0.8);
  cairo_move_to(cr, x, y1);
  cairo_line_to(cr, x, y2);
  cairo_stroke(cr);
  cairo_restore(cr);
}

static void draw_grid(cairo_t *cr, dv_dag_node_t *node) {
  // VL
  double y1, y2;
  y1 = node->c - DV_RADIUS - DV_UNION_NODE_MARGIN;
  y2 = node->c + DV_RADIUS + DV_UNION_NODE_MARGIN;
  dv_dag_node_t *nn;
  if (nn = node->head) {
    y2 += nn->dc * DV_VDIS;
  }
  dv_grid_line_t *l;
  l = node->vl_in;
  draw_grid_vl(cr, l, y1, y2);
  while (l->l) {
    l = l->l;
    draw_grid_vl(cr, l, y1, y2);
  }
  l = node->vl_in;
  while (l->r) {
    l = l->r;
    draw_grid_vl(cr, l, y1, y2);
  }
}

static void draw_glike_node_r(cairo_t *cr, dv_dag_node_t *u) {
  if (!u) return;
  int call_head = 0;
  if (!dv_is_union(u)
      || dv_is_shrinked(u) || dv_is_shrinking(u)) {
    draw_glike_node_1(cr, u);
  }
  // Iterate links
  dv_dag_node_t * v;
  dv_llist_iterate_init(u->links);
  while (v = (dv_dag_node_t *) dv_llist_iterate_next(u->links)) {
    draw_glike_node_r(cr, v);    
  }
  // Call head
  if (!dv_is_single(u)) {
		draw_glike_node_r(cr, u->head);
    // Draw grid
    //draw_grid(cr, u);
  }
}

static void draw_glike_edge_1(cairo_t *cr, dv_dag_node_t *u, dv_dag_node_t *v) {
  if (u->c + DV_RADIUS > v->c - DV_RADIUS)
    return;
  double alpha = 1.0;
  if ((!u->parent || dv_is_shrinking(u->parent))
      && (!v->parent || dv_is_shrinking(v->parent)))
    alpha = dv_get_alpha_fading_out(u->parent);
  else if ((!u->parent || dv_is_expanding(u->parent))
           && (!v->parent || dv_is_expanding(v->parent)))            
    alpha = dv_get_alpha_fading_in(u->parent);
  cairo_save(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
  cairo_move_to(cr, u->vl->c, u->c + DV_RADIUS);
  cairo_line_to(cr, v->vl->c, v->c - DV_RADIUS);
  cairo_stroke(cr);
  cairo_restore(cr);
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

static void draw_glike_edge_r(cairo_t *cr, dv_dag_node_t *u) {
  if (!u) return;
  // Call head
  if (!dv_is_single(u)) {
		draw_glike_edge_r(cr, u->head);
  }
  // Iterate links
  dv_dag_node_t * v;
  dv_llist_iterate_init(u->links);
  while (v = (dv_dag_node_t *) dv_llist_iterate_next(u->links)) {

    dv_dag_node_t *u_tail, *v_head;
    if (dv_is_single(u)) {
      
      if (dv_is_single(v)) {
        draw_glike_edge_1(cr, u, v);        
      } else {
        v_head = v->head;
				dv_dag_node_t * v_first = dv_dag_node_get_first(v_head);
				draw_glike_edge_1(cr, u, v_first);
        
      }
      
    } else {
      
      dv_llist_iterate_init(u->tails);
      while (u_tail = (dv_dag_node_t *) dv_llist_iterate_next(u->tails)) {
        dv_dag_node_t * u_last = dv_dag_node_get_last(u_tail);
        
        if (dv_is_single(v)) {
          draw_glike_edge_1(cr, u_last, v);
        } else {
          
          v_head = v->head;
					dv_dag_node_t * v_first = dv_dag_node_get_first(v_head);
					draw_glike_edge_1(cr, u_last, v_first);
          
        }
        
      }
      
    }
    draw_glike_edge_r(cr, v);
    
  }
}

void dv_draw_glike_dvdag(cairo_t *cr, dv_dag_t *G) {
  cairo_set_line_width(cr, 2.0);
  int i;
  // Draw nodes
  draw_glike_node_r(cr, G->rt);
  // Draw edges
  draw_glike_edge_r(cr, G->rt);
}

/*-----------------end of Gridlike Drawing functions-----------*/

