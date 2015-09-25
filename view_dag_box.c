#include "dagviz.h"

/*-----------------Dagbox layout functions-----------*/

double
dv_dag_scale_down(dv_dag_t * D, double val) {
  double ret;
  switch (D->sdt) {
  case 0:
    ret = log(val) / log(D->log_radix);
    break;
  case 1:
    ret = pow(val, D->power_radix);
    break;
  case 2:
    ret = val / D->linear_radix;
    break;
  default:
    dv_check(0);
    break;
  }
  return ret;
}

double
dv_dag_scale_down_linear(dv_dag_t * D, double val) {
  return val / D->linear_radix;
}

double
dv_view_scale_down(dv_view_t * V, double val) {
  return dv_dag_scale_down(V->D, val);
}

static double
dv_view_calculate_vgap(dv_view_t * V, dv_dag_node_t * parent, dv_dag_node_t * node1, dv_dag_node_t * node2) {
  dv_dag_t * D = V->D;
  dr_pi_dag_node * pi1 = dv_pidag_get_node_by_dag_node(D->P, node1);
  dr_pi_dag_node * pi2 = dv_pidag_get_node_by_dag_node(D->P, node2);
  double rate = dv_view_calculate_rate(V, parent);
  double vgap;
  if (!D->frombt) {
    // begin - end
    double time_gap = dv_view_scale_down(V, pi2->info.start.t - pi1->info.end.t);
    vgap = rate * time_gap;
  } else {
    // from beginning
    double time1 = dv_view_scale_down(V, pi1->info.end.t - D->bt);
    double time2 = dv_view_scale_down(V, pi2->info.start.t - D->bt);
    vgap = rate * (time2 - time1);
  }
  return vgap;
}

double
dv_view_calculate_vsize(dv_view_t * V, dv_dag_node_t * node) {
  dv_dag_t * D = V->D;
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
  double rate = 1.0;//dv_view_calculate_rate(V, node->parent);
  double vsize;
  if (!D->frombt) {
    // begin - end
    double time_gap = dv_view_scale_down(V, pi->info.end.t - pi->info.start.t);
    vsize = rate * time_gap;
  } else {
    // from beginning
    double time1 = dv_view_scale_down(V, pi->info.start.t - D->bt);
    double time2 = dv_view_scale_down(V, pi->info.end.t - D->bt);
    vsize = rate * (time2 - time1);
  }
  return vsize;
}

/*
static double dv_view_calculate_vsize_pure(dv_view_t *V, dv_dag_node_t *node) {
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
  double vsize;
  if (!S->frombt) {
    double time = pi->info.end.t - pi->info.start.t;
    vsize = dv_view_scale_down(V, time);
  } else {
    double time1 = dv_view_scale_down(V, pi->info.start.t - D->bt);
    double time2 = dv_view_scale_down(V, pi->info.end.t - D->bt);
    double vsize = time2 - time1;
  }
  return vsize;
}
*/

static dv_dag_node_t *
dv_layout_node_get_last_finished_tail(dv_view_t * V, dv_dag_node_t * node) {
  dv_dag_node_t * ret = 0;
  dv_dag_node_t * tail = NULL;
  while ( (tail = dv_dag_node_traverse_tails(node, tail)) ) {
    dr_pi_dag_node * ret_pi = dv_pidag_get_node_by_dag_node(V->D->P, ret);
    dr_pi_dag_node * tail_pi = dv_pidag_get_node_by_dag_node(V->D->P, tail);
    if (!ret || ret_pi->info.end.t < tail_pi->info.end.t)
      ret = tail;
  }
  return ret;
}

static double
dv_layout_node_get_xp_by_accumulating_xpre(dv_dag_node_t * node) {
  int coord = 1;
  dv_dag_node_t * u = node;
  double dis = 0.0;
  while (u) {
    dis += u->c[coord].xpre;
    u = u->pre;
  }
  return dis;
}

static double
dv_layout_node_get_last_tail_xp_r(dv_view_t * V, dv_dag_node_t * node) {
  dv_check(node);
  double ret = 0.0;
  if (!dv_is_single(node)) {
    dv_dag_node_t * last = dv_layout_node_get_last_finished_tail(V, node);
    if (last) {
      ret += dv_layout_node_get_xp_by_accumulating_xpre(last);
      ret += dv_layout_node_get_last_tail_xp_r(V, last);
    }
  }
  return ret;
}

static void
dv_view_layout_dagbox_node(dv_view_t * V, dv_dag_node_t * node) {
  V->S->nl++;
  if (dv_is_shrinking(node) && (V->D->collapsing_d == 0 || node->d < V->D->collapsing_d))
    V->D->collapsing_d = node->d;
  int coord = 1;
  dv_node_coordinate_t * nodeco = &node->c[coord];
  /* Calculate inward */
  if (dv_is_inward_callable(node)) {
    dv_check(node->head);
    // node's head's outward
    dv_node_coordinate_t *headco = &node->head->c[coord];
    headco->xpre = 0.0;
    headco->y = nodeco->y;
    // Recursive call
    dv_view_layout_dagbox_node(V, node->head);
    // node's inward
    nodeco->lw = headco->link_lw;
    nodeco->rw = headco->link_rw;
    nodeco->dw = headco->link_dw;
    // for enhancing expand/collapse animation
    if (nodeco->lw < V->D->radius)
      nodeco->lw = V->D->radius;
    if (nodeco->rw < V->D->radius)
      nodeco->rw = V->D->radius;
    double vsize = dv_view_calculate_vsize(V, node);
    if (nodeco->dw < vsize)
      nodeco->dw = vsize;
  } else {
    // node's inward
    nodeco->lw = V->D->radius;
    nodeco->rw = V->D->radius;
    nodeco->dw = dv_view_calculate_vsize(V, node);
  }
    
  /* Calculate link-along */
  dv_dag_node_t * u, * v; // linked nodes
  dv_node_coordinate_t * uco, * vco;
  double rate, ugap, vgap, hgap;
  switch ( dv_dag_node_count_nexts(node) ) {
  case 0:
    // node's link-along
    nodeco->link_lw = nodeco->lw;
    nodeco->link_rw = nodeco->rw;
    nodeco->link_dw = nodeco->dw;
    break;
  case 1:
    u = node->next;
    uco = &u->c[coord];
    // node & u's rate
    rate = dv_view_calculate_rate(V, node->parent);
    ugap = dv_view_calculate_vgap(V, node->parent, node, u);
    // node's linked u's outward    
    uco->xpre = dv_layout_node_get_last_tail_xp_r(V, node);
    uco->y = nodeco->y + nodeco->dw * rate + ugap;
    // Recursive call
    dv_view_layout_dagbox_node(V, u);
    // node's link-along
    nodeco->link_lw = dv_max(nodeco->lw, uco->link_lw - uco->xpre);
    nodeco->link_rw = dv_max(nodeco->rw, uco->link_rw + uco->xpre);
    nodeco->link_dw = nodeco->dw * rate + ugap + uco->link_dw;
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    uco = &u->c[coord];
    vco = &v->c[coord];
    // node & u,v's rate
    rate = dv_view_calculate_rate(V, node->parent);
    ugap = dv_view_calculate_vgap(V, node->parent, node, u);
    vgap = dv_view_calculate_vgap(V, node->parent, node, v);
    // node's linked u,v's outward
    uco->y = nodeco->y + nodeco->dw * rate + ugap;
    vco->y = nodeco->y + nodeco->dw * rate + vgap;
    // Recursive call
    dv_view_layout_dagbox_node(V, u);
    dv_view_layout_dagbox_node(V, v);
    
    // node's linked u,v's outward
    hgap = rate * DV_HDIS;
    // u
    uco->xpre = (uco->link_lw - V->D->radius) + hgap;
    if (u->spawn)
      uco->xpre = - u->spawn->c[coord].xpre;
    // v
    vco->xpre = (vco->link_rw - V->D->radius) + hgap;
    if (u->spawn)
      vco->xpre += (uco->link_lw - V->D->radius) - uco->xpre;
    vco->xpre = - vco->xpre;
    
    // node's link-along
    nodeco->link_lw = - vco->xpre + vco->link_lw;
    nodeco->link_rw = uco->xpre + uco->link_rw;
    nodeco->link_dw = nodeco->dw * rate + dv_max(ugap + uco->link_dw, vgap + vco->link_dw);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

static void
dv_view_layout_dagbox_node_2nd(dv_dag_node_t * node) {
  int coord = 1;
  dv_node_coordinate_t * nodeco = &node->c[coord];
  /* Calculate inward */
  if (dv_is_inward_callable(node)) {
    dv_check(node->head);
    // node's head's outward
    dv_node_coordinate_t * headco = &node->head->c[coord];
    headco->xp = 0.0;
    headco->x = nodeco->x;
    // Recursive call
    dv_view_layout_dagbox_node_2nd(node->head);
  }
    
  /* Calculate link-along */
  dv_dag_node_t * u, * v; // linked nodes
  dv_node_coordinate_t * uco, * vco;
  switch ( dv_dag_node_count_nexts(node) ) {
  case 0:
    break;
  case 1:
    u = node->next;
    uco = &u->c[coord];
    // node's linked u's outward
    uco->xp = uco->xpre + nodeco->xp;
    uco->x = uco->xp + u->parent->c[coord].x;
    // Recursive call
    dv_view_layout_dagbox_node_2nd(u);
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    uco = &u->c[coord];
    vco = &v->c[coord];
    // node's linked u,v's outward
    uco->xp = uco->xpre + nodeco->xp;
    uco->x = uco->xp + u->parent->c[coord].x;
    vco->xp = vco->xpre + nodeco->xp;
    vco->x = vco->xp + v->parent->c[coord].x;
    // Recursive call
    dv_view_layout_dagbox_node_2nd(u);
    dv_view_layout_dagbox_node_2nd(v);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

void dv_view_layout_dagbox(dv_view_t *V) {
  dv_dag_t *D = V->D;
  int coord = 1;
  dv_node_coordinate_t *rtco = &D->rt->c[coord];

  // Relative coord
  V->D->collapsing_d = 0;
  rtco->xpre = 0.0; // pre-based
  rtco->y = 0.0;
  dv_view_layout_dagbox_node(V, D->rt);

  // Absolute coord
  rtco->xp = 0.0; // parent-based
  rtco->x = 0.0;
  dv_view_layout_dagbox_node_2nd(D->rt);

  // Check
  //print_layout(D);

}

/*-----------------end of Dagbox layout functions-----------*/


/*-----------------DAG Dagbox Drawing functions-----------*/

static void
dv_view_draw_dagbox_node_1(dv_view_t * V, cairo_t * cr, dv_dag_node_t * node) {
  /* Get inputs */
  dv_dag_t * D = V->D;
  dv_view_status_t * S = V->S;
  int coord = 1;
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
  dv_node_coordinate_t * nodeco = &node->c[coord];
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
  if (dv_is_union(node)) {

    cairo_set_line_width(cr, DV_NODE_LINE_WIDTH_COLLECTIVE_FACTOR * DV_NODE_LINE_WIDTH);
    
    if ( dv_is_inner_loaded(node)
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

  } else {
    
    /* Calculate coordinates: normal-sized box (leaf node) */
    xx = x - nodeco->lw;
    yy = y;
    w = nodeco->lw + nodeco->rw;
    h = nodeco->dw;
    
  }
  /* Draw path */
  cairo_move_to(cr, xx, yy);
  cairo_line_to(cr, xx + w, yy);
  cairo_line_to(cr, xx + w, yy + h);
  cairo_line_to(cr, xx, yy + h);
  cairo_close_path(cr);

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
    cairo_save(cr);
    if (!pat) {
      cairo_set_source_rgba(cr, c.red, c.green, c.blue, c.alpha * alpha);
      cairo_fill_preserve(cr);      
    } else {
      cairo_translate(cr, xx, yy);
      cairo_set_source(cr, pat);
      cairo_fill_preserve(cr);
      cairo_pattern_destroy(pat);      
    }
    cairo_restore(cr);
  }

  /* Draw node's border */
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
  cairo_stroke_preserve(cr);
  
  /* Draw node's infotag's mark */
  if (dv_llist_has(V->D->P->itl, (void *) node->pii)) {
    cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.6);
    cairo_fill_preserve(cr);
    dv_llist_add(V->D->itl, (void *) node);
  }
  
  /* Fill rate */
  double fill_rate = 1.0;
  if (D->draw_with_current_time) {
    double st = pi->info.start.t - D->bt;
    double et = pi->info.end.t - D->bt;
    if (D->current_time <= st)
      fill_rate = 0.0;
    else if (D->current_time <= et)
      fill_rate = (D->current_time - st) / (et - st);
    else
      fill_rate = 1.0;
  }
        
  /* Clip */
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");
  cairo_clip(cr);
  cairo_rectangle(cr, xx, yy + (h * fill_rate), w, h * (1 - fill_rate));
  cairo_clip_preserve(cr);
  cairo_set_source_rgba(cr, white->red, white->green, white->blue, white->alpha);
  cairo_fill(cr);
  
  cairo_restore(cr);
}

static void
dv_view_draw_dagbox_node_r(dv_view_t * V, cairo_t * cr, dv_dag_node_t * node) {
  // Count node
  V->S->ndh++;
  if (!node || !dv_is_set(node))
    return;
  /* Draw node */
  if (!dv_is_union(node) || !dv_is_inner_loaded(node)
      || dv_is_shrinked(node) || dv_is_shrinking(node)) {
    dv_view_draw_dagbox_node_1(V, cr, node);
  }
  /* Call inward */
  if (!dv_is_single(node)) {
    dv_view_draw_dagbox_node_r(V, cr, node->head);
  }
  /* Call link-along */
  dv_dag_node_t * x = NULL;
  while ( (x = dv_dag_node_traverse_nexts(node, x)) ) {
    dv_view_draw_dagbox_node_r(V, cr, x);
  }
}

static void
dv_view_draw_dagbox_edge_1(dv_view_t * V, cairo_t * cr, dv_dag_node_t * node, dv_dag_node_t * next) {
  dv_view_draw_edge_1(V, cr, node, next);
}

static void
dv_view_draw_dagbox_edge_r(dv_view_t * V, cairo_t * cr, dv_dag_node_t * node) {
  if (!node || !dv_is_set(node))
    return;
  /* Call inward */
  if (!dv_is_single(node)) {
    dv_view_draw_dagbox_edge_r(V, cr, node->head);
  }
  /* Call link-along */
  dv_dag_node_t * next = NULL;
  while ( (next = dv_dag_node_traverse_nexts(node, next)) ) {
    
    if (dv_is_single(node)) {
      
      if (dv_is_single(next))
        dv_view_draw_dagbox_edge_1(V, cr, node, next);
      else
        dv_view_draw_dagbox_edge_1(V, cr, node, dv_dag_node_get_single_head(next->head));
      
    } else {

      dv_dag_node_t * tail = NULL;
      while ( (tail = dv_dag_node_traverse_tails(node, tail)) ) {
          dv_dag_node_t * last = dv_dag_node_get_single_last(tail);

          if (dv_is_single(next))
            dv_view_draw_dagbox_edge_1(V, cr, last, next);
          else
            dv_view_draw_dagbox_edge_1(V, cr, last, dv_dag_node_get_single_head(next->head));

      }

    }
    dv_view_draw_dagbox_edge_r(V, cr, next);
    
  }
}

void
dv_view_draw_dagbox(dv_view_t * V, cairo_t * cr) {
  cairo_set_line_width(cr, DV_NODE_LINE_WIDTH);
  // Draw nodes
  dv_llist_init(V->D->itl);
  V->S->nd = 0;
  V->S->ndh = 0;
  V->D->cur_d = 0;
  V->D->cur_d_ex = V->D->dmax;
  dv_view_draw_dagbox_node_r(V, cr, V->D->rt);
  // Draw edges
  cairo_save(cr);
  cairo_new_path(cr);
  dv_view_draw_dagbox_edge_r(V, cr, V->D->rt);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
  cairo_stroke(cr);
  cairo_restore(cr);
}


/*-----------------end of DAG Dagbox drawing functions-----------*/


