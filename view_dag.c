#include "dagviz-gtk.h"


/****** Utility ******/

static dm_dag_node_t *
dv_view_dag_find_clicked_node_1(dv_view_t * V, double x, double y, dm_dag_node_t * node) {
  dm_dag_node_t * ret = NULL;
  dm_node_coordinate_t * co = &node->c[V->S->coord];
  if (co->x - co->lw < x && x < co->x + co->rw && co->y < y && y < co->y + co->dw) {
    ret = node;
  }
  return ret;
}

static dm_dag_node_t *
dv_view_dag_find_clicked_node_r(dv_view_t * V, double x, double y, dm_dag_node_t * node) {
  dm_dag_node_t * ret = NULL;
  dm_node_coordinate_t * co = &node->c[V->S->coord];
  /* Call inward */
  if (dm_is_single(node)) {
    if (dv_view_dag_find_clicked_node_1(V, x, y, node)) {
      return node;
    }
  } else if (co->x - co->lw < x && x < co->x + co->rw && co->y < y && y < co->y + co->dw) {
    ret = dv_view_dag_find_clicked_node_r(V, x, y, node->head);
    if (ret)
      return ret;
  }
  /* Call link-along */
  if (co->x - co->link_lw < x && x < co->x + co->link_rw && co->y < y && y < co->y + co->link_dw) {
    dm_dag_node_t * next = NULL;
    while ( (next = dm_dag_node_traverse_nexts(node, next)) ) {
      ret = dv_view_dag_find_clicked_node_r(V, x, y, next);
      if (ret)
        return ret;
    }
  }
  return NULL;
}

dm_dag_node_t *
dv_view_dag_find_clicked_node(dv_view_t * V, double x, double y) {
  double time = dm_get_time();
  if (DVG->verbose_level >= 2) {
    fprintf(stderr, "dv_view_dag_find_clicked_node()\n");
  }
  dm_dag_node_t * ret = dv_view_dag_find_clicked_node_r(V, x, y, V->D->rt);
  if (DVG->verbose_level >= 2) {
    fprintf(stderr, "... done dv_view_dag_find_clicked_node(): %lf\n", dm_get_time() - time);
  }
  return ret;
}

int
dv_rectangle_is_invisible(dv_view_t * V, double x, double y, double w, double h) {
  double bound_left = dv_view_clip_get_bound_left(V);
  double bound_right = dv_view_clip_get_bound_right(V);
  double bound_up = dv_view_clip_get_bound_up(V);
  double bound_down = dv_view_clip_get_bound_down(V);
  int ret = 0;
  if (y + h < bound_up)
    ret |= DV_DAG_NODE_HIDDEN_ABOVE;
  if (x > bound_right)
    ret |= DV_DAG_NODE_HIDDEN_RIGHT;
  if (y > bound_down)
    ret |= DV_DAG_NODE_HIDDEN_BELOW;
  if (x + w < bound_left)
    ret |= DV_DAG_NODE_HIDDEN_LEFT;
  return ret;
}

int
dv_rectangle_trim(dv_view_t * V, double * x, double * y, double * w, double * h) {
  double bound_left = dv_view_clip_get_bound_left(V);
  double bound_right = dv_view_clip_get_bound_right(V);
  double bound_up = dv_view_clip_get_bound_up(V);
  double bound_down = dv_view_clip_get_bound_down(V);
  double ww = (w)?(*w):0.0;
  double hh = (h)?(*h):0.0;
  int ret = 0;
  if (*y + hh < bound_up)
    ret |= DV_DAG_NODE_HIDDEN_ABOVE;
  if (*x > bound_right)
    ret |= DV_DAG_NODE_HIDDEN_RIGHT;
  if (*y > bound_down)
    ret |= DV_DAG_NODE_HIDDEN_BELOW;
  if (*x + ww < bound_left)
    ret |= DV_DAG_NODE_HIDDEN_LEFT;
  /* Trim */
  if (!ret) {
    if (*x < bound_left) {
      ww -= (bound_left - *x);
      *x = bound_left;
    }
    if (*x + ww > bound_right)
      ww = bound_right - *x;
    if (*y < bound_up) {
      hh -= (bound_up - *y);
      *y = bound_up;
    }
    if (*y + hh > bound_down)
      hh = bound_down - *y;
  }
  if (w) *w = ww;
  if (h) *h = hh;
  return ret;
}

int
dm_dag_node_is_invisible(dv_view_t * V, dm_dag_node_t * node) {
  int ret = -1;
  if (!node) return ret;
  
  dm_node_coordinate_t * nodeco = &node->c[V->S->coord];
  double x = nodeco->x;
  double y = nodeco->y;
  double margin = 0.0;
  if (dm_is_set(node) && dm_is_union(node) && dm_is_inner_loaded(node)
      && (dm_is_expanding(node) || dm_is_shrinking(node))) {
    margin = DVG->opts.union_node_puffing_margin;
  }
  double xx, yy, w, h;
  xx = x - nodeco->lw - margin;
  yy = y - margin;
  w = nodeco->lw + nodeco->rw + 2 * margin;
  h = nodeco->dw + 2 * margin;
  
  ret = dv_rectangle_is_invisible(V, xx, yy, w, h);
  return ret;
}

int
dm_dag_node_link_is_invisible(dv_view_t * V, dm_dag_node_t * node) {
  int ret = -1;
  if (!node) return ret;
  
  dm_node_coordinate_t * nodeco = &node->c[V->S->coord];
  double x = nodeco->x;
  double y = nodeco->y;
  double margin = 0.0;
  if (dm_is_set(node) && dm_is_union(node) && dm_is_inner_loaded(node)
      && (dm_is_expanding(node) || dm_is_shrinking(node))) {
    margin = DVG->opts.union_node_puffing_margin;
  }
  double xx, yy, w, h;
  xx = x - nodeco->link_lw - margin;
  yy = y - margin;
  w = nodeco->link_lw + nodeco->link_rw + 2 * margin;
  h = nodeco->link_dw + 2 * margin;
  
  ret = dv_rectangle_is_invisible(V, xx, yy, w, h);
  return ret;
}

/****** end of Utility ******/


/****** Layout ******/

static void
dv_view_layout_dag_node(dv_view_t * V, dm_dag_node_t * node) {
  V->S->nl++;
  if (dm_is_shrinking(node) && (V->D->collapsing_d == 0 || node->d < V->D->collapsing_d))
    V->D->collapsing_d = node->d;
  dm_node_coordinate_t * nodeco = &node->c[V->S->coord];
  /* Calculate inward */
  if (dm_is_inward_callable(node)) {
    dv_check(node->head);
    // node's head's outward
    dm_node_coordinate_t *headco = &node->head->c[V->S->coord];
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
  dm_dag_node_t * u, * v; // linked nodes
  dm_node_coordinate_t * uco, * vco;
  double rate, ypre, hgap;
  switch ( dm_dag_node_count_nexts(node) ) {
  case 0:
    // node's link-along
    nodeco->link_lw = nodeco->lw;
    nodeco->link_rw = nodeco->rw;
    nodeco->link_dw = nodeco->dw;
    break;
  case 1:
    u = node->next;
    uco = &u->c[V->S->coord];
    // node & u's rate
    rate = dv_view_calculate_rate(V, node->parent);
    // node's linked u's outward
    uco->xpre = 0.0;
    ypre = (nodeco->dw - 2 * V->D->radius + DVG->opts.vnd) * rate;
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
    uco = &u->c[V->S->coord];
    vco = &v->c[V->S->coord];
    // node & u,v's rate
    rate = dv_view_calculate_rate(V, node->parent);
    // node's linked u,v's outward
    ypre = (nodeco->dw - 2 * V->D->radius + DVG->opts.vnd) * rate;
    uco->y = nodeco->y + ypre;
    vco->y = nodeco->y + ypre;
    // Recursive call
    dv_view_layout_dag_node(V, u);
    dv_view_layout_dag_node(V, v);
    
    // node's linked u,v's outward
    hgap = DVG->opts.hnd * rate;
    // u
    uco->xpre = (uco->link_lw - V->D->radius) + hgap;
    if (u->spawn)
      uco->xpre = - u->spawn->c[V->S->coord].xpre;
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

static void
dv_view_layout_dag_node_2nd(dv_view_t * V, dm_dag_node_t * node) {
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
    dv_view_layout_dag_node_2nd(V, node->head);
  }
    
  /* Calculate link-along */
  dm_dag_node_t *u, *v; // linked nodes
  dm_node_coordinate_t *uco, *vco;
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
    dv_view_layout_dag_node_2nd(V, u);
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
    dv_view_layout_dag_node_2nd(V, u);
    dv_view_layout_dag_node_2nd(V, v);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

void
dv_view_layout_dag(dv_view_t * V) {
  dm_dag_t * D = V->D;
  int coord = V->S->coord;
  dm_node_coordinate_t * rtco = &D->rt->c[coord];

  // Relative coord
  V->D->collapsing_d = 0;
  rtco->xpre = 0.0; // pre-based
  rtco->y = 0.0;
  dv_view_layout_dag_node(V, D->rt);

  // Absolute coord
  rtco->xp = 0.0; // parent-based
  rtco->x = 0.0;
  dv_view_layout_dag_node_2nd(V, D->rt);
  
}

/****** end of Layout ******/


/****** Draw ******/

static void
dv_view_draw_dag_node_1(dv_view_t * V, cairo_t * cr, dm_dag_node_t * node, int * on_global_cp) {
  cairo_save(cr);
  /* Get inputs */
  dm_dag_t * D = V->D;
  dv_view_status_t * S = V->S;
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, node);
  dr_dag_node_kind_t kind = pi->info.kind;
  dm_node_coordinate_t * nodeco = &node->c[V->S->coord];
  double x = nodeco->x;
  double y = nodeco->y;

  /* Count drawn node */
  S->nd++;
  if (node->d > D->cur_d) {
    D->cur_d = node->d;
  }
  if (dm_is_union(node) && dm_is_inner_loaded(node)
      && dm_is_shrinked(node)
      && node->d < D->cur_d_ex) {
    D->cur_d_ex = node->d;
  }
  
  /* Alpha, Margin */
  double alpha = 1.0;
  double margin = 0.0;

  /* Coordinates */
  double xx, yy, w, h;
  
  /* Draw path */
  cairo_new_path(cr);
  if (kind == dr_dag_node_kind_section || kind == dr_dag_node_kind_task) {
    
    if (dm_is_union(node)) {

      if (dm_is_inner_loaded(node)
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

      /* Draw path */
      /*
      double extra = 0.2 * w;
      xx -= extra;
      yy -= extra;
      w += 2 * extra;
      h += 2 * extra;
      */
      cairo_set_line_width(cr, DVG->opts.nlw_collective_node_factor * DVG->opts.nlw);
      if (kind == dr_dag_node_kind_section) {
        dv_draw_path_rounded_rectangle(cr, xx, yy, w, h);
      } else {
        dv_draw_path_rectangle(cr, xx, yy, w, h);
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
  cairo_save(cr);
  cairo_clip(cr);
  cairo_rectangle(cr, xx, yy + (h * fill_rate), w, h * (1 - fill_rate));
  cairo_set_source_rgba(cr, white->red, white->green, white->blue, white->alpha);
  cairo_fill(cr);
  cairo_restore(cr);
  
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

    int cp;
    for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++)
      if (on_global_cp[cp]) {
        gdk_rgba_parse(color, DVG->cp_colors[cp]);
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
dv_view_draw_dag_edge_1(dv_view_t * V, cairo_t * cr, dm_dag_node_t * node, dm_dag_node_t * next) {
  dv_view_draw_edge_1(V, cr, node, next);
}

static void
dv_view_draw_dag_node_r(dv_view_t * V, cairo_t * cr, dm_dag_node_t * node, int * parent_on_global_cp) {
  /* Count node */
  V->S->ndh++;
  if (!node || !dm_is_set(node))
    return;

  /* Check if node is still on the global critical paths */
  int me_on_global_cp[DV_NUM_CRITICAL_PATHS];
  int cp;
  for (cp = 0; cp < DV_NUM_CRITICAL_PATHS; cp++) {
    if (parent_on_global_cp[cp] && dm_node_flag_check(node->f, DMG->oncp_flags[cp]))
      me_on_global_cp[cp] = 1;
    else
      me_on_global_cp[cp] = 0;
  }
  
  if (!dm_dag_node_is_invisible(V, node)) {
    /* Draw node */
    if (!dm_is_union(node) || !dm_is_inner_loaded(node)
        || dm_is_shrinked(node) || dm_is_shrinking(node)) {
      dv_view_draw_dag_node_1(V, cr, node, me_on_global_cp);
    }
    /* Call inward */
    if (!dm_is_single(node)) {
      dv_view_draw_dag_node_r(V, cr, node->head, me_on_global_cp);
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
          dv_view_draw_dag_edge_1(V, cr, node, x);
        else
          dv_view_draw_dag_edge_1(V, cr, node, dm_dag_node_get_single_head(x->head));      
      } else {
        dm_dag_node_t * tail = NULL;
        while ( (tail = dm_dag_node_traverse_tails(node, tail)) ) {
          dm_dag_node_t * last = dm_dag_node_get_single_last(tail);        
          if (dm_is_single(x))
            dv_view_draw_dag_edge_1(V, cr, last, x);
          else
            dv_view_draw_dag_edge_1(V, cr, last, dm_dag_node_get_single_head(x->head));
        }      
      }
      cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, 1.0);
      cairo_stroke(cr);
      cairo_restore(cr);
      /* Call recursively then */
      dv_view_draw_dag_node_r(V, cr, x, parent_on_global_cp);
    }
  }
}

void
dv_view_draw_dag(dv_view_t * V, cairo_t * cr) {
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
  dv_view_draw_dag_node_r(V, cr, V->D->rt, on_global_cp);
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
  char s[DV_STRING_LENGTH];
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
  cairo_set_line_width(cr, DVG->opts.nlw_collective_node_factor * DVG->opts.nlw);
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
  sprintf(s, "multiple workers");
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

/****** end of Draw ******/


