#include "dagviz.h"

/****** Layout ******/

/*
double
dv_dag_scale_down(dm_dag_t * D, double val) {
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
*/

double
dv_dag_scale_down_linear(dm_dag_t * D, double val) {
  return val / D->linear_radix;
}

double
dv_view_scale_down_linear(dv_view_t * V, double val) {
  return val / V->D->linear_radix;
}

double
dv_view_scale_down(dv_view_t * V, double val) {
  double ret;
  switch (V->S->lt) {
  case DV_LAYOUT_TYPE_DAG_BOX_LOG:
    ret = log(val) / log(V->D->log_radix);
    break;
  case DV_LAYOUT_TYPE_DAG_BOX_POWER:
    ret = pow(val, V->D->power_radix);
    break;
  case DV_LAYOUT_TYPE_DAG_BOX_LINEAR:
    ret = val / V->D->linear_radix;
    break;
  default:
    fprintf(stderr, "Error: V->S->lt = %d\n", V->S->lt);
    ret = val;
    //dv_check(0);
    break;
  }
  return ret;
}

static double
dv_view_calculate_vgap(dv_view_t * V, dm_dag_node_t * parent, dm_dag_node_t * node1, dm_dag_node_t * node2) {
  dm_dag_t * D = V->D;
  dr_pi_dag_node * pi1 = dm_pidag_get_node_by_dag_node(D->P, node1);
  dr_pi_dag_node * pi2 = dm_pidag_get_node_by_dag_node(D->P, node2);
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
dv_view_calculate_vsize(dv_view_t * V, dm_dag_node_t * node) {
  dm_dag_t * D = V->D;
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, node);
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
static double dv_view_calculate_vsize_pure(dv_view_t *V, dm_dag_node_t *node) {
  dm_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, node);
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

static dm_dag_node_t *
dv_layout_node_get_last_finished_tail(dv_view_t * V, dm_dag_node_t * node) {
  dm_dag_node_t * ret = 0;
  dm_dag_node_t * tail = NULL;
  while ( (tail = dm_dag_node_traverse_tails(node, tail)) ) {
    dr_pi_dag_node * ret_pi = dm_pidag_get_node_by_dag_node(V->D->P, ret);
    dr_pi_dag_node * tail_pi = dm_pidag_get_node_by_dag_node(V->D->P, tail);
    if (!ret || ret_pi->info.end.t < tail_pi->info.end.t)
      ret = tail;
  }
  return ret;
}

static double
dv_layout_node_get_xp_by_accumulating_xpre(dv_view_t * V, dm_dag_node_t * node) {
  int coord = V->S->coord;
  dm_dag_node_t * u = node;
  double dis = 0.0;
  while (u) {
    dis += u->c[coord].xpre;
    u = u->pre;
  }
  return dis;
}

static double
dv_layout_node_get_last_tail_xp_r(dv_view_t * V, dm_dag_node_t * node) {
  dv_check(node);
  double ret = 0.0;
  if (!dm_is_single(node)) {
    dm_dag_node_t * last = dv_layout_node_get_last_finished_tail(V, node);
    if (last) {
      ret += dv_layout_node_get_xp_by_accumulating_xpre(V, last);
      ret += dv_layout_node_get_last_tail_xp_r(V, last);
    }
  }
  return ret;
}

static void
dv_view_layout_dagbox_node(dv_view_t * V, dm_dag_node_t * node) {
  V->S->nl++;
  if (dm_is_shrinking(node) && (V->D->collapsing_d == 0 || node->d < V->D->collapsing_d))
    V->D->collapsing_d = node->d;
  int coord = V->S->coord;
  dm_node_coordinate_t * nodeco = &node->c[coord];
  /* Calculate inward */
  if (dm_is_inward_callable(node)) {
    dv_check(node->head);
    // node's head's outward
    dm_node_coordinate_t *headco = &node->head->c[coord];
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
  dm_dag_node_t * u, * v; // linked nodes
  dm_node_coordinate_t * uco, * vco;
  double rate, ugap, vgap, hgap;
  switch ( dm_dag_node_count_nexts(node) ) {
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
    hgap = rate * DVG->opts.hnd;
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
dv_view_layout_dagbox_node_2nd(dv_view_t * V, dm_dag_node_t * node) {
  int coord = V->S->coord;
  dm_node_coordinate_t * nodeco = &node->c[coord];
  /* Calculate inward */
  if (dm_is_inward_callable(node)) {
    dv_check(node->head);
    // node's head's outward
    dm_node_coordinate_t * headco = &node->head->c[coord];
    headco->xp = 0.0;
    headco->x = nodeco->x;
    // Recursive call
    dv_view_layout_dagbox_node_2nd(V, node->head);
  }
    
  /* Calculate link-along */
  dm_dag_node_t * u, * v; // linked nodes
  dm_node_coordinate_t * uco, * vco;
  switch ( dm_dag_node_count_nexts(node) ) {
  case 0:
    break;
  case 1:
    u = node->next;
    uco = &u->c[coord];
    // node's linked u's outward
    uco->xp = uco->xpre + nodeco->xp;
    uco->x = uco->xp + u->parent->c[coord].x;
    // Recursive call
    dv_view_layout_dagbox_node_2nd(V, u);
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
    dv_view_layout_dagbox_node_2nd(V, u);
    dv_view_layout_dagbox_node_2nd(V, v);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

void dv_view_layout_dagbox(dv_view_t *V) {
  dm_dag_t *D = V->D;
  int coord = V->S->coord;
  dm_node_coordinate_t *rtco = &D->rt->c[coord];

  // Relative coord
  V->D->collapsing_d = 0;
  rtco->xpre = 0.0; // pre-based
  rtco->y = 0.0;
  dv_view_layout_dagbox_node(V, D->rt);

  // Absolute coord
  rtco->xp = 0.0; // parent-based
  rtco->x = 0.0;
  dv_view_layout_dagbox_node_2nd(V, D->rt);

  // Check
  //print_layout(D);

}

/****** end of Layout ******/


/****** Draw ******/

static void
dv_view_draw_dagbox_node_1(dv_view_t * V, cairo_t * cr, dm_dag_node_t * node, int * on_global_cp) {
  cairo_save(cr);
  /* Get inputs */
  dm_dag_t * D = V->D;
  dv_view_status_t * S = V->S;
  int coord = S->coord;
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, node);
  dm_node_coordinate_t * nodeco = &node->c[coord];
  double x = nodeco->x;
  double y = nodeco->y;

  /* Count drawn node */
  S->nd++;
  if (node->d > D->cur_d)
    D->cur_d = node->d;
  if (dm_is_union(node) && dm_is_inner_loaded(node)
      && dm_is_shrinked(node)
      && node->d < D->cur_d_ex)
    D->cur_d_ex = node->d;
  
  /* Alpha, Margin */
  double alpha = 1.0;
  double margin = 0.0;
  
  /* Coordinates */
  double xx, yy, w, h;
  if (dm_is_union(node)) {

    cairo_set_line_width(cr, DVG->opts.nlw_collective_node_factor * DVG->opts.nlw);
    
    if ( dm_is_inner_loaded(node)
         && (dm_is_expanding(node) || dm_is_shrinking(node))) {

      /* Calculate alpha, margin */
      margin = DVG->opts.union_node_puffing_margin;
      if (dm_is_expanding(node)) {
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
    cairo_set_line_width(cr, DVG->opts.nlw);
    xx = x - nodeco->lw;
    yy = y;
    w = nodeco->lw + nodeco->rw;
    h = nodeco->dw;
    
  }
  
  /* Draw path */
  cairo_new_path(cr);
  cairo_rectangle(cr, xx, yy, w, h);

  /* Calculate alpha (based on parent) */
  if (dm_is_shrinking(node->parent)) {
    alpha *= dv_view_get_alpha_fading_out(V, node->parent);
  } else if (dm_is_expanding(node->parent)) {
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
  if (((dv_dag_t*)V->D->g)->nr == 0 || !V->S->color_remarked_only || node->r != 0) {
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
  if (dm_llist_has(V->D->P->itl, (void *) node->pii)) {
    cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.6);
    cairo_fill_preserve(cr);
    dm_llist_add(V->D->itl, (void *) node);
  }
  
  /* Highlight */
  if ( node->highlight ) {
    cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.5);
    cairo_fill_preserve(cr);
  }
  
  /* Fill node partly according to D->current_time */
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
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");
  cairo_new_path(cr);
  cairo_rectangle(cr, xx, yy + (h * fill_rate), w, h * (1 - fill_rate));
  cairo_set_source_rgba(cr, white->red, white->green, white->blue, white->alpha);
  cairo_fill(cr);

  /* Highlight critical paths */
  {
    cairo_new_path(cr);
    double margin, line_width, margin_increment;
    GdkRGBA color[1];
    
    //line_width = 2 * DVG->opts.nlw;
    line_width = 2 * DVG->opts.nlw / V->S->zoom_ratio_x;
    if (line_width < DVG->opts.nlw)
      line_width = DVG->opts.nlw;
    margin = 0.5 * DVG->opts.nlw + 0.5 * line_width;
    margin_increment = line_width;

    int i;
    for (i = 0; i < DV_NUM_CRITICAL_PATHS; i++)
      if (on_global_cp[i]) {
        gdk_rgba_parse(color, DVG->cp_colors[i]);
        cairo_set_source_rgba(cr, color->red, color->green, color->blue, color->alpha);
        cairo_set_line_width(cr, line_width);
        cairo_rectangle(cr, xx - margin, yy - margin, w + 2 * margin, h + 2 * margin);
        cairo_stroke(cr);
        margin += margin_increment;
      }
  }
  
  cairo_restore(cr);
}

static void
dv_view_draw_dagbox_edge_1(dv_view_t * V, cairo_t * cr, dm_dag_node_t * node, dm_dag_node_t * next) {
  dv_view_draw_edge_1(V, cr, node, next);
}

static void
dv_view_draw_dagbox_node_r(dv_view_t * V, cairo_t * cr, dm_dag_node_t * node, int * parent_on_global_cp) {
  /* Counting statistics */
  V->S->ndh++;
  if (!node || !dm_is_set(node))
    return;
  
  /* Check if node is still on the global critical paths */
  int me_on_global_cp[DV_NUM_CRITICAL_PATHS];
  int i;
  for (i = 0; i < DV_NUM_CRITICAL_PATHS; i++) {
    if (parent_on_global_cp[i] && dm_node_flag_check(node->f, DMG->oncp_flags[i]))
      me_on_global_cp[i] = 1;
    else
      me_on_global_cp[i] = 0;
  }
  
  if (!dm_dag_node_is_invisible(V, node)) {
    /* Draw node */
    if (!dm_is_union(node) || !dm_is_inner_loaded(node)
        || dm_is_shrinked(node) || dm_is_shrinking(node)) {
      dv_view_draw_dagbox_node_1(V, cr, node, me_on_global_cp);
    }
    /* Call inward */
    if (!dm_is_single(node)) {
      dv_view_draw_dagbox_node_r(V, cr, node->head, me_on_global_cp);
    }
  }
  /* Call link-along */
  if (!dm_dag_node_link_is_invisible(V, node)) {
    dm_dag_node_t * x = NULL;
    while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
      /* Draw edge first */
      cairo_save(cr);
      cairo_new_path(cr);
      if (dm_is_single(node)) {      
        if (dm_is_single(x))
          dv_view_draw_dagbox_edge_1(V, cr, node, x);
        else
          dv_view_draw_dagbox_edge_1(V, cr, node, dm_dag_node_get_single_head(x->head));
      } else {
        dm_dag_node_t * tail = NULL;
        while ( (tail = dm_dag_node_traverse_tails(node, tail)) ) {
          dm_dag_node_t * last = dm_dag_node_get_single_last(tail);
          if (dm_is_single(x))
            dv_view_draw_dagbox_edge_1(V, cr, last, x);
          else
            dv_view_draw_dagbox_edge_1(V, cr, last, dm_dag_node_get_single_head(x->head));
        }
      }
      cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
      cairo_stroke(cr);
      cairo_restore(cr);
      /* Call recursively then */
      dv_view_draw_dagbox_node_r(V, cr, x, parent_on_global_cp);
    }
  }
}

void
dv_view_draw_dagbox(dv_view_t * V, cairo_t * cr) {
  /* Prepare */
  cairo_set_line_width(cr, DVG->opts.nlw);
  dm_llist_init(V->D->itl);
  V->S->nd = 0;
  V->S->ndh = 0;
  V->D->cur_d = 0;
  V->D->cur_d_ex = V->D->dmax;
  int on_global_cp[DV_NUM_CRITICAL_PATHS];
  int i;
  for (i = 0; i < DV_NUM_CRITICAL_PATHS; i++) {
    if (V->D->show_critical_paths[i])
      on_global_cp[i] = 1;
    else
      on_global_cp[i] = 0;
  }

  /* Draw */
  dv_view_draw_dagbox_node_r(V, cr, V->D->rt, on_global_cp);
}

/****** end of Draw ******/
