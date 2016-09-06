#include "dagviz.h"

/****** Layout ******/

static void
dv_view_layout_timeline_ver_node(dv_view_t * V, dv_dag_node_t * node) {
  V->S->nl++;
  if (node->d > V->D->collapsing_d)
    V->D->collapsing_d = node->d;
  
  int coord = DV_LAYOUT_TYPE_TIMELINE_VER;
  dv_node_coordinate_t * nodeco = &node->c[coord];
  dv_dag_t * D = V->D;
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
  
  /* Calculate inward */
  nodeco->y = dv_view_scale_down(V, pi->info.start.t - D->bt);
  nodeco->dw = dv_view_scale_down(V, pi->info.end.t - D->bt) - dv_view_scale_down(V, pi->info.start.t - D->bt);
  int worker = pi->info.worker;
  nodeco->x = worker * (2 * V->D->radius);
  nodeco->lw = 0.0;
  nodeco->rw = 2 * V->D->radius;
  if (dv_is_union(node)) {
    if (worker < 0) {
      nodeco->x = 0.0;
      nodeco->rw = V->D->P->num_workers * (2 * V->D->radius);
    }
    if (dv_is_inner_loaded(node) && dv_is_expanded(node))
      dv_view_layout_timeline_ver_node(V, node->head);
  }
    
  /* Calculate link-along */
  dv_dag_node_t * u, * v;
  switch ( dv_dag_node_count_nexts(node) ) {
  case 0:
    break;
  case 1:
    u = node->next;
    dv_view_layout_timeline_ver_node(V, u);
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    dv_view_layout_timeline_ver_node(V, u);
    dv_view_layout_timeline_ver_node(V, v);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

void
dv_view_layout_timeline_ver(dv_view_t * V) {
  V->D->collapsing_d = 0;
  dv_view_layout_timeline_ver_node(V, V->D->rt);
}

/****** end of Layout ******/


/****** Draw ******/

static void
dv_view_draw_timeline_ver_node_1(dv_view_t * V, cairo_t * cr, dv_dag_node_t * node) {
  dv_dag_t * D = V->D;
  dv_view_status_t * S = V->S;
  int coord = DV_LAYOUT_TYPE_TIMELINE_VER;
  dv_node_coordinate_t * nodeco = &node->c[coord];
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
  dr_pi_dag_node *pi = dv_pidag_get_node_by_dag_node(D->P, node);
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
  
  if (!dv_timeline_node_is_invisible(V, node)) {
    dv_timeline_trim_rectangle(V, &xx, &yy, &w, &h);
    
    /* Draw path */
    cairo_move_to(cr, xx, yy);
    cairo_line_to(cr, xx + w, yy);
    cairo_line_to(cr, xx + w, yy + h);
    cairo_line_to(cr, xx, yy + h);
    cairo_close_path(cr);

    /* Draw node */
    if (dv_is_union(node)) {
      double c[4] = { 0.15, 0.15, 0.15, 0.2 };
      cairo_set_source_rgba(cr, c[0], c[1], c[2], c[3]);
      cairo_fill(cr);
    } else {
      cairo_set_source_rgba(cr, c[0], c[1], c[2], c[3] * alpha);
      cairo_fill_preserve(cr);
      if (DV_TIMELINE_NODE_WITH_BORDER) {
        cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
        cairo_stroke_preserve(cr);
      }
      /* Highlight */
      if ( node->highlight ) {
        cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.5);
        cairo_fill_preserve(cr);
      }
      /* Draw opaque for infotag node */
      if (dv_llist_has(V->D->P->itl, (void *) node->pii)) {
        cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.6);
        cairo_fill(cr);
      }
    }
    
  }
  // Flag to draw infotag
  if (dv_llist_has(V->D->P->itl, (void *) node->pii)) {
    dv_llist_add(V->D->itl, (void *) node);
  }
  cairo_restore(cr);
}

static void
dv_view_draw_timeline_ver_node_r(dv_view_t * V, cairo_t * cr, dv_dag_node_t * node) {
  /* Counting statistics */
  V->S->ndh++;
  if (!node || !dv_is_set(node))
    return;
  /* Call inward */
  int hidden = dv_timeline_node_is_invisible(V, node);
  if (!hidden) {
    if (dv_is_union(node) && dv_is_inner_loaded(node) && dv_is_expanded(node)) {
      dv_view_draw_timeline_ver_node_r(V, cr, node->head);
    } else {
      dv_view_draw_timeline_ver_node_1(V, cr, node);
    }
  }
  /* Call link-along */
  if (!(hidden & DV_DAG_NODE_HIDDEN_BELOW)) {
    dv_dag_node_t * next = NULL;
    while ( (next = dv_dag_node_traverse_nexts(node, next)) ) {
      dv_view_draw_timeline_ver_node_r(V, cr, next);
    }
  }
}

void
dv_view_draw_timeline_ver(dv_view_t * V, cairo_t * cr) {
  // Set adaptive line width
  double line_width = dv_min(CS->opts.nlw, CS->opts.nlw / dv_min(V->S->zoom_ratio_x, V->S->zoom_ratio_y));
  cairo_set_line_width(cr, line_width);
  int i;
  // Draw nodes
  dv_llist_init(V->D->itl);
  V->S->nd = 0;
  V->S->ndh = 0;
  V->D->cur_d = 0;
  V->D->cur_d_ex = V->D->dmax;
  dv_view_draw_timeline_ver_node_r(V, cr, V->D->rt);
  // Draw worker numbers
  cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);
  char s[DV_STRING_LENGTH];
  double xx, yy;
  xx = V->D->radius;
  yy = -4;
  for (i=0; i<V->D->P->num_workers; i++) {
    sprintf(s, "Worker %d", i);            
    cairo_move_to(cr, xx - 30, yy);
    cairo_show_text(cr, s);
    xx += 2 * V->D->radius;
  }
}

/****** end of Draw ******/


