#include "dagviz.h"

/*-----------------Grid-like Layout functions-----------*/

/*-----------------Grid-related functions-----------*/

void dv_grid_line_init(dv_grid_line_t * l) {
  l->l = 0;
  l->r = 0;
  dv_llist_init(l->L);
  l->c = 0;
}

dv_grid_line_t * dv_grid_line_create() {
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
  S->fcc++;
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
  S->fcc++;
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
  S->fcc++;
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
  S->fcc++;
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
  S->fcc++;
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
  
  // Bind nodes to lines
  dv_grid_line_init(G->rvl);
  dv_layout_bind_node(G->rt, G->rvl);
  
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

void dv_relayout_glike_dvdag(dv_dag_t *G) {

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
