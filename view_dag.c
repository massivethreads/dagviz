#include "dagviz.h"


/*-----------------DAG layout functions-----------*/

static void
dv_view_layout_dag_node(dv_view_t * V, dv_dag_node_t * node) {
  int lt = 0;
  dv_node_coordinate_t * nodeco = &node->c[lt];
  /* Calculate inward */
  if (dv_is_inward_callable(node)) {
    dv_check(node->head);
    // node's head's outward
    dv_node_coordinate_t *headco = &node->head->c[lt];
    headco->xpre = 0.0;
    headco->y = nodeco->y;
    // Recursive call
    dv_view_layout_dag_node(V, node->head);
    // node's inward
    nodeco->lw = headco->link_lw;
    nodeco->rw = headco->link_rw;
    nodeco->dw = headco->link_dw;
  } else {
    // node's inward
    nodeco->lw = V->D->radius;
    nodeco->rw = V->D->radius;
    nodeco->dw = 2 * V->D->radius;
  }
    
  /* Calculate link-along */
  int n_links = 0;
  if (node->next && node->spawn)
    n_links = 2;
  else if (node->next)
    n_links = 1;
  dv_dag_node_t * u, * v; // linked nodes
  dv_node_coordinate_t * uco, * vco;
  double rate, ypre, hgap;
  switch (n_links) {
  case 0:
    // node's link-along
    nodeco->link_lw = nodeco->lw;
    nodeco->link_rw = nodeco->rw;
    nodeco->link_dw = nodeco->dw;
    break;
  case 1:
    u = node->next;
    uco = &u->c[lt];
    // node & u's rate
    rate = dv_view_calculate_rate(V, node->parent);
    // node's linked u's outward
    uco->xpre = 0.0;
    ypre = (nodeco->dw - 2 * V->D->radius + DV_VDIS) * rate;
    uco->y = nodeco->y + ypre;
    // Recursive call
    dv_view_layout_dag_node(V, u);
    // node's link-along
    nodeco->link_lw = dv_max(nodeco->lw, uco->link_lw);
    nodeco->link_rw = dv_max(nodeco->rw, uco->link_rw);
    nodeco->link_dw = ypre + uco->link_dw;
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    uco = &u->c[lt];
    vco = &v->c[lt];
    // node & u,v's rate
    rate = dv_view_calculate_rate(V, node->parent);
    // node's linked u,v's outward
    ypre = (nodeco->dw - 2 * V->D->radius + DV_VDIS) * rate;
    uco->y = nodeco->y + ypre;
    vco->y = nodeco->y + ypre;
    // Recursive call
    dv_view_layout_dag_node(V, u);
    dv_view_layout_dag_node(V, v);
    
    // node's linked u,v's outward
    hgap = DV_HDIS * rate;
    // u
    uco->xpre = (uco->link_lw - V->D->radius) + hgap;
    if (u->spawn)
      uco->xpre = - u->spawn->c[lt].xpre;
    // v
    vco->xpre = (vco->link_rw - V->D->radius) + hgap;
    double left_push = 0.0;
    if (u->spawn)
      left_push = (uco->link_lw - V->D->radius) - uco->xpre;
    if (left_push > 0)
      vco->xpre += left_push;
    vco->xpre = - vco->xpre;
    
    // node's link-along
    nodeco->link_lw = - vco->xpre + vco->link_lw;
    nodeco->link_rw = uco->xpre + uco->link_rw;
    nodeco->link_dw = ypre + dv_max(uco->link_dw, vco->link_dw);
    break;
  default:
    dv_check(0);
    break;
  }  
}

static void dv_view_layout_dag_node_2nd(dv_dag_node_t *node) {
  int lt = 0;
  dv_node_coordinate_t *nodeco = &node->c[lt];
  /* Calculate inward */
  if (dv_is_inward_callable(node)) {
    dv_check(node->head);
    // node's head's outward
    dv_node_coordinate_t *headco = &node->head->c[lt];
    headco->xp = 0.0;
    headco->x = nodeco->x;
    // Recursive call
    dv_view_layout_dag_node_2nd(node->head);
  }
    
  /* Calculate link-along */
  int n_links = 0;
  if (node->next && node->spawn)
    n_links = 2;
  else if (node->next)
    n_links = 1;
  dv_dag_node_t *u, *v; // linked nodes
  dv_node_coordinate_t *uco, *vco;
  switch (n_links) {
  case 0:
    break;
  case 1:
    u = node->next;
    uco = &u->c[lt];
    // node's linked u's outward
    uco->xp = uco->xpre + nodeco->xp;
    uco->x = uco->xp + u->parent->c[lt].x;
    // Recursive call
    dv_view_layout_dag_node_2nd(u);
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    uco = &u->c[lt];
    vco = &v->c[lt];
    // node's linked u,v's outward
    uco->xp = uco->xpre + nodeco->xp;
    uco->x = uco->xp + u->parent->c[lt].x;
    vco->xp = vco->xpre + nodeco->xp;
    vco->x = vco->xp + v->parent->c[lt].x;
    // Recursive call
    dv_view_layout_dag_node_2nd(u);
    dv_view_layout_dag_node_2nd(v);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

void dv_view_layout_dag(dv_view_t *V) {
  dv_dag_t *D = V->D;
  int lt = 0;
  dv_node_coordinate_t *rtco = &D->rt->c[lt];

  // Relative coord
  rtco->xpre = 0.0; // pre-based
  rtco->y = 0.0;
  dv_view_layout_dag_node(V, D->rt);

  // Absolute coord
  rtco->xp = 0.0; // parent-based
  rtco->x = 0.0;
  dv_view_layout_dag_node_2nd(D->rt);
  
}

/*-----------------end of DAG layout functions-----------*/



/*-----------------DAG Drawing functions-----------*/

static void
dv_view_draw_dag_node_1(dv_view_t * V, cairo_t * cr, dv_dag_node_t * node) {
  /* Get inputs */
  dv_dag_t * D = V->D;
  dv_view_status_t * S = V->S;
  int lt = 0;
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
  dr_dag_node_kind_t kind = pi->info.kind;
  dv_node_coordinate_t * nodeco = &node->c[lt];
  double x = nodeco->x;
  double y = nodeco->y;
  
  /* Count drawn node */
  S->nd++;
  if (node->d > D->cur_d)
    D->cur_d = node->d;
  if (dv_is_union(node) && dv_is_inner_loaded(node)
      && dv_is_shrinked(node)
      && node->d < D->cur_d_ex)
    D->cur_d_ex = node->d;
  
  /* Alpha, Margin */
  double alpha = 1.0;
  double margin = 0.0;

  /* Coordinates */
  double xx, yy, w, h;
  
  /* Draw path */
  cairo_save(cr);
  cairo_new_path(cr);
  if (kind == dr_dag_node_kind_section || kind == dr_dag_node_kind_task) {
    
    if (dv_is_union(node)) {

      if (dv_is_inner_loaded(node)
          && (dv_is_expanding(node) || dv_is_shrinking(node))) {

        /* Calculate alpha, margin */
        margin = DV_UNION_NODE_MARGIN;
        if (dv_is_expanding(node)) {
          alpha *= dv_view_get_alpha_fading_out(V, node);
          margin *= dv_view_get_alpha_fading_in(V, node);
        } else {
          alpha *= dv_view_get_alpha_fading_in(V, node);
          margin *= dv_view_get_alpha_fading_out(V, node);
        }
        /* Calculate coordinates: large-sized box */
        xx = x - nodeco->lw - margin;
        yy = y - margin;
        w = nodeco->lw + nodeco->rw + 2 * margin;
        h = nodeco->dw + 2 * margin;
      
      } else {
      
        /* Calculate coordinates: normal-sized box */
        xx = x - nodeco->lw;
        yy = y;
        w = nodeco->lw + nodeco->rw;
        h = nodeco->dw;
      
      }

      /* Draw path */
      /*
      double extra = 0.2 * w;
      xx -= extra;
      yy -= extra;
      w += 2 * extra;
      h += 2 * extra;
      */
      cairo_set_line_width(cr, DV_NODE_LINE_WIDTH_COLLECTIVE_FACTOR * DV_NODE_LINE_WIDTH);
      if (kind == dr_dag_node_kind_section) {
        dv_draw_path_rounded_rectangle(cr, xx, yy, w, h);
        /*
        dv_draw_path_rounded_rectangle(cr,
                                       xx + DV_UNION_NODE_DOUBLE_BORDER,
                                       yy + DV_UNION_NODE_DOUBLE_BORDER,
                                       w - 2 * DV_UNION_NODE_DOUBLE_BORDER,
                                       h - 2 * DV_UNION_NODE_DOUBLE_BORDER); */
      } else {
        dv_draw_path_rectangle(cr, xx, yy, w, h);
        /* dv_draw_path_rectangle(cr,
                               xx + DV_UNION_NODE_DOUBLE_BORDER,
                               yy + DV_UNION_NODE_DOUBLE_BORDER,
                               w - 2 * DV_UNION_NODE_DOUBLE_BORDER,
                               h - 2 * DV_UNION_NODE_DOUBLE_BORDER); */
      }

    } else { // is collective but not union

      /* Calculate coordinates: normal-sized box */
      xx = x - nodeco->lw;
      yy = y;
      w = nodeco->lw + nodeco->rw;
      h = nodeco->dw;

      /* Draw path */
      if (kind == dr_dag_node_kind_section) {
        dv_draw_path_rounded_rectangle(cr, xx, yy, w, h);
      } else {
        dv_draw_path_rectangle(cr, xx, yy, w, h);
      }
      
    }
    
  } else {
    
    /* Calculate coordinates */
    xx = x - nodeco->lw;
    yy = y;
    w = nodeco->lw + nodeco->rw;
    h = nodeco->dw;
    
    /* Draw path */
    if (kind == dr_dag_node_kind_create_task)
      dv_draw_path_isosceles_triangle(cr, xx, yy, w, h);
    else if (kind == dr_dag_node_kind_wait_tasks)
      dv_draw_path_isosceles_triangle_upside_down(cr, xx, yy, w, h);
    else
      dv_draw_path_circle(cr, xx, yy, w);

  }
  
  /* Calculate alpha (based on parent) */
  if (dv_is_shrinking(node->parent)) {
    alpha *= dv_view_get_alpha_fading_out(V, node->parent);
  } else if (dv_is_expanding(node->parent)) {
    alpha *= dv_view_get_alpha_fading_in(V, node->parent);
  }

  /* Color */
  GdkRGBA c;
  cairo_pattern_t * pat = NULL;
  dv_lookup_color(pi, S->nc, &c.red, &c.green, &c.blue, &c.alpha);
  if ((S->nc == 0 && pi->info.worker == -1)
      || (S->nc == 1 && pi->info.cpu == -1)) {
    /*
    int nstops = 3;
    int stops[] = {0, 1, 2};
    pat = dv_create_color_linear_pattern(stops, nstops, w, alpha);
    */
    pat = dv_get_color_linear_pattern(V, w, alpha, node->r);
    //pat = dv_get_color_radial_pattern(dv_min(w/2.0, h/2.0), alpha);
  }

  /* Draw node's filling */
  if (V->D->nr == 0 || !V->S->color_remarked_only || node->r != 0) {
    if (!pat) {
      cairo_set_source_rgba(cr, c.red, c.green, c.blue, c.alpha * alpha);
      cairo_fill_preserve(cr);      
    } else {
      cairo_translate(cr, xx, yy);
      cairo_set_source(cr, pat);
      cairo_fill_preserve(cr);
      cairo_pattern_destroy(pat);      
    }
  }

  /* Draw node's border */
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
  cairo_stroke_preserve(cr);
  
  /* Draw node's infotag's mark */
  if (dv_llist_has(V->D->P->itl, (void *) node->pii)) {
    cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.6);
    cairo_fill(cr);
    dv_llist_add(V->D->itl, (void *) node);
  }
  
  cairo_restore(cr);
}

static void
dv_view_draw_dag_node_r(dv_view_t * V, cairo_t * cr, dv_dag_node_t * u) {
  // Count node
  V->S->ndh++;
  if (!u || !dv_is_set(u))
    return;
  /* Draw node */
  if (!dv_is_union(u) || !dv_is_inner_loaded(u)
      || dv_is_shrinked(u) || dv_is_shrinking(u)) {
    dv_view_draw_dag_node_1(V, cr, u);
  }
  /* Call inward */
  if (!dv_is_single(u)) {
    dv_view_draw_dag_node_r(V, cr, u->head);
  }
  /* Call link-along */
  if (u->next)
    dv_view_draw_dag_node_r(V, cr, u->next);
  if (u->spawn)
    dv_view_draw_dag_node_r(V, cr, u->spawn);
}

static void
dv_view_draw_dag_edge_1(dv_view_t * V, cairo_t * cr, dv_dag_node_t * u, dv_dag_node_t * v) {
  dv_view_draw_edge_1(V, cr, u, v);
}

static dv_dag_node_t *
dv_dag_node_get_first(dv_dag_node_t * u) {
  dv_check(u);
  while (!dv_is_single(u)) {
    dv_check(u->head);
    u = u->head;
  }
  return u;
}

static dv_dag_node_t *
dv_dag_node_get_last(dv_dag_node_t * u) {
  dv_check(u);
  while (!dv_is_single(u)) {
    dv_check(u->tail);
    u = u->tail;
  }
  return u;
}

static void
dv_view_draw_dag_edge_r(dv_view_t * V, cairo_t * cr, dv_dag_node_t * u) {
  if (!u || !dv_is_set(u))
    return;
  /* Call inward */
  if (!dv_is_single(u)) {
    dv_view_draw_dag_edge_r(V, cr, u->head);
  }
  /* Call link-along */
  dv_dag_node_t * v = NULL;
  dv_dag_node_t * a[2];
  a[0] = u->next;
  a[1] = u->spawn;
  int i;
  for (i = 0; i < 2; i++) {
    
    v = a[i];
    if (!v) break;
    dv_dag_node_t * u_tail, * v_head;
    if (dv_is_single(u)) {
      
      if (dv_is_single(v)) {
        dv_view_draw_dag_edge_1(V, cr, u, v);        
      } else {
        v_head = v->head;
        dv_dag_node_t * v_first = dv_dag_node_get_first(v_head);
        dv_view_draw_dag_edge_1(V, cr, u, v_first);        
      }
      
    } else {

      /* Traverse all tails */
      dv_dag_node_t * x = u->head;
      while (x) {
        u_tail = x->spawn;
        if (!u_tail && !x->next)
          u_tail = x;
        if (u_tail) {
          
          dv_dag_node_t * u_last = dv_dag_node_get_last(u_tail);
        
          if (dv_is_single(v)) {
            dv_view_draw_dag_edge_1(V, cr, u_last, v);
          } else {
            v_head = v->head;
            dv_dag_node_t * v_first = dv_dag_node_get_first(v_head);
            dv_view_draw_dag_edge_1(V, cr, u_last, v_first);
          }
          
        }
        x = x->next;
      }
      
    }
    dv_view_draw_dag_edge_r(V, cr, v);
    
  }
}

void
dv_view_draw_dag(dv_view_t * V, cairo_t * cr) {
  cairo_set_line_width(cr, DV_NODE_LINE_WIDTH);
  int i;
  // Draw nodes
  dv_llist_init(V->D->itl);
  dv_view_draw_dag_node_r(V, cr, V->D->rt);
  // Draw edges
  cairo_save(cr);
  cairo_new_path(cr);
  dv_view_draw_dag_edge_r(V, cr, V->D->rt);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
  cairo_stroke(cr);
  cairo_restore(cr);
}

void
dv_view_draw_legend_dag(dv_view_t * V, cairo_t * cr) {
  cairo_save(cr);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);

  const int L = 20;
  double box_w = 20;
  double box_h = 20;
  double box_dis = 12;
  double box_down_margin = 2;
  char s[L];
  const double char_width = 7;
  const double line_height = 27;
  double x = V->S->vpw  - DV_STATUS_PADDING - L * char_width;
  double y = DV_STATUS_PADDING;
  double xx, yy, xxt;
  cairo_new_path(cr);

  /* create */
  sprintf(s, "create");
  y += line_height;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);
  // box
  cairo_new_path(cr);
  xx = x - box_dis - box_w;
  yy = y + box_down_margin - box_h;
  dv_draw_path_isosceles_triangle(cr, xx, yy, box_w, box_h);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_stroke(cr);
  
  /* wait */
  sprintf(s, "wait");
  y += line_height;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);
  // box
  cairo_new_path(cr);
  xx = x - box_dis - box_w;
  yy = y + box_down_margin - box_h;
  dv_draw_path_isosceles_triangle_upside_down(cr, xx, yy, box_w, box_h);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_stroke(cr);
  
  /* end */
  sprintf(s, "end");
  y += line_height;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);
  // circle
  xx = x - box_dis - box_w;
  yy = y + box_down_margin - box_h;
  dv_draw_path_circle(cr, xx, yy, dv_min(box_w, box_h));
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_stroke(cr);

  /* section */
  sprintf(s, "section");
  y += line_height;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);
  // box
  xx = x - box_dis - box_w;
  yy = y + box_down_margin - box_h;
  dv_draw_path_rounded_rectangle(cr, xx, yy, box_w, box_h);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_stroke(cr);
  
  /* task */
  sprintf(s, "task");
  y += line_height;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);
  // box
  xx = x - box_dis - box_w;
  yy = y + box_down_margin - box_h;
  dv_draw_path_rectangle(cr, xx, yy, box_w, box_h);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_stroke(cr);
  
  /* expandable */
  sprintf(s, "expandable");
  y += line_height;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);
  // boxes
  cairo_save(cr);
  cairo_set_line_width(cr, DV_NODE_LINE_WIDTH_COLLECTIVE_FACTOR * DV_NODE_LINE_WIDTH);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  xx = xxt = x - box_dis - box_w;
  yy = y + box_down_margin - box_h;
  dv_draw_path_rectangle(cr, xx, yy, box_w, box_h);
  cairo_stroke(cr);
  xx = xxt = xxt - box_dis * 0.5 - box_w;
  yy = y + box_down_margin - box_h;
  dv_draw_path_rounded_rectangle(cr, xx, yy, box_w, box_h);
  cairo_stroke(cr);
  cairo_restore(cr);
  
  /* worker */
  sprintf(s, "worker 0, 1, 2, ...");
  y += line_height;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);
  // boxes
  double r, g, b, a;
  xx = xxt = x - box_dis - box_w;
  yy = y + box_down_margin - box_h;
  dv_draw_path_rectangle(cr, xx, yy, box_w, box_h);
  dv_lookup_color_value(2, &r, &g, &b, &a);
  cairo_set_source_rgba(cr, r, g, b, a);
  cairo_fill(cr);
  xx = xxt = xxt - box_dis * 0.5 - box_w;
  yy = y + box_down_margin - box_h;
  dv_draw_path_rectangle(cr, xx, yy, box_w, box_h);
  dv_lookup_color_value(1, &r, &g, &b, &a);
  cairo_set_source_rgba(cr, r, g, b, a);
  cairo_fill(cr);
  xx = xxt = xxt - box_dis * 0.5 - box_w;
  yy = y + box_down_margin - box_h;
  dv_draw_path_rectangle(cr, xx, yy, box_w, box_h);
  dv_lookup_color_value(0, &r, &g, &b, &a);
  cairo_set_source_rgba(cr, r, g, b, a);
  cairo_fill(cr);
  
  /* many workers */
  sprintf(s, "many workers");
  y += line_height;
  cairo_move_to(cr, x, y);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_show_text(cr, s);
  // box
  xx = x - box_dis - box_w;
  yy = y + box_down_margin - box_h;
  cairo_pattern_t * pat = dv_get_color_linear_pattern(V, box_w, 1.0, 0);
  dv_draw_path_rectangle(cr, xx, yy, box_w, box_h);
  cairo_translate(cr, xx, yy);
  cairo_set_source(cr, pat);
  cairo_fill(cr);
  
  cairo_restore(cr);
}



/*-----------------end of DAG Drawing functions-----------*/
