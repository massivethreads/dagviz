#include "dagviz.h"

/****** Utility ******/

static dv_dag_node_t *
dv_timeline_find_clicked_node_r(dv_view_t * V, double cx, double cy, dv_dag_node_t * node) {
  dv_dag_node_t * ret = NULL;
  const int coord = (V->S->lt == DV_LAYOUT_TYPE_PARAPROF) ? DV_LAYOUT_TYPE_TIMELINE : DV_LAYOUT_TYPE_PARAPROF;
  dv_node_coordinate_t * co = &node->c[coord];
  /* Call inward */
  double x, y, w, h;
  x = co->x;
  y = co->y;
  w = co->rw;
  h = co->dw;
  if (x <= cx && cx <= x + w && y <= cy && cy <= y + h) {
    if (dv_is_single(node))
      ret = node;
    else
      ret = dv_timeline_find_clicked_node_r(V, cx, cy, node->head);
  } else if ( (V->S->lt == DV_LAYOUT_TYPE_TIMELINE && cx > x + w)
              || (V->S->lt == DV_LAYOUT_TYPE_PARAPROF && cx > x + w)
              || (V->S->lt == DV_LAYOUT_TYPE_TIMELINE_VER && cy > y + h) ) {
    /* Call link-along */
    dv_dag_node_t * next = NULL;
    while ( (next = dv_dag_node_traverse_nexts(node, next)) ) {
      ret = dv_timeline_find_clicked_node_r(V, cx, cy, next);
      if (ret)
        break;
    }
  }
  return ret;
}

dv_dag_node_t *
dv_timeline_find_clicked_node(dv_view_t * V, double x, double y) {
  dv_dag_node_t * ret = dv_timeline_find_clicked_node_r(V, x, y, V->D->rt);
  return ret;
}

void
dv_timeline_trim_rectangle(dv_view_t * V, double * x, double * y, double * w, double * h) {
  double bound_left = dv_view_clip_get_bound_left(V);
  double bound_right = dv_view_clip_get_bound_right(V);
  double bound_up = dv_view_clip_get_bound_up(V);
  double bound_down = dv_view_clip_get_bound_down(V);
  if (*x < bound_left) {
    *w -= (bound_left - *x);
    *x = bound_left;
  }
  if (*x + *w > bound_right)
    *w = bound_right - *x;
  if (*y < bound_up) {
    *h -= (bound_up - *y);
    *y = bound_up;
  }
  if (*y + *h > bound_down)
    *h = bound_down - *y;
}

int
dv_timeline_node_is_invisible(dv_view_t * V, dv_dag_node_t * node) {
  int ret = -1;
  if (!node) return ret;
  
  dv_node_coordinate_t * nodeco = &node->c[V->S->coord];
  double x, y, w, h;
  x = nodeco->x;
  y = nodeco->y;
  w = nodeco->rw;
  h = nodeco->dw;
  
  ret = dv_rectangle_is_invisible(V, x, y, w, h);
  return ret;
}

/****** end of Utility ******/


/****** Layout ******/

static void
dv_view_layout_timeline_node(dv_view_t * V, dv_dag_node_t * node) {
  V->S->nl++;
  if (node->d > V->D->collapsing_d)
    V->D->collapsing_d = node->d;
  
  int coord = DV_LAYOUT_TYPE_TIMELINE;
  dv_node_coordinate_t * nodeco = &node->c[coord];
  dv_dag_t * D = V->D;
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
  
  /* Calculate inward */
  nodeco->x = dv_dag_scale_down_linear(D, pi->info.start.t - D->bt);
  nodeco->lw = 0.0;
  nodeco->rw = dv_dag_scale_down_linear(D, pi->info.end.t - D->bt) - dv_dag_scale_down_linear(D, pi->info.start.t - D->bt);
  int worker = pi->info.worker;
  nodeco->y = worker * (2 * V->D->radius);
  nodeco->dw = V->D->radius * 2;      
  if (dv_is_union(node)) {
    if (worker < 0) {
      nodeco->y = 0.0;
      nodeco->dw = V->D->P->num_workers * (2 * V->D->radius);
    }
    if (dv_is_inner_loaded(node) && dv_is_expanded(node))
      dv_view_layout_timeline_node(V, node->head);
  }
    
  /* Calculate link-along */
  dv_dag_node_t * u, * v;
  switch ( dv_dag_node_count_nexts(node) ) {
  case 0:
    break;
  case 1:
    u = node->next;
    dv_view_layout_timeline_node(V, u);
    break;
  case 2:
    u = node->next;  // cont node
    v = node->spawn; // task node
    dv_view_layout_timeline_node(V, u);
    dv_view_layout_timeline_node(V, v);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

void
dv_view_layout_timeline(dv_view_t * V) {
  V->D->collapsing_d = 0;
  dv_view_layout_timeline_node(V, V->D->rt);
}

/****** end of Layout ******/


/****** Draw ******/

static void
dv_view_draw_timeline_node_1(dv_view_t * V, cairo_t * cr, dv_dag_node_t * node, int * on_global_cp) {
  cairo_save(cr);
  /* Get inputs */
  dv_dag_t * D = V->D;
  dv_view_status_t * S = V->S;
  int coord = DV_LAYOUT_TYPE_TIMELINE;
  dv_node_coordinate_t * nodeco = &node->c[coord];
  
  /* Count drawn node */
  S->nd++;
  if (node->d > D->cur_d)
    D->cur_d = node->d;
  if (dv_is_union(node) && dv_is_inner_loaded(node)
      && dv_is_shrinked(node)
      && node->d < D->cur_d_ex)
    D->cur_d_ex = node->d;
  
  /* Node color */
  double x = nodeco->x;
  double y = nodeco->y;
  double c[4];
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
  dv_lookup_color(pi, S->nc, c, c+1, c+2, c+3);
  /* Alpha */
  double alpha = 1.0;
  
  /* Coordinates */
  double xx, yy, w, h;
  // Normal-sized box (terminal node)
  xx = x;
  yy = y;
  w = nodeco->rw;
  h = nodeco->dw;
  
  /* Draw path */
  cairo_new_path(cr);
  if (!dv_timeline_node_is_invisible(V, node)) {
    dv_timeline_trim_rectangle(V, &xx, &yy, &w, &h);
    
    cairo_rectangle(cr, xx, yy, w, h);
    
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
      /* Draw node's infotag's mark */
      if (dv_llist_has(V->D->P->itl, (void *) node->pii)) {
        cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.6);
        cairo_fill_preserve(cr);
      }
      
      /* Highlight critical paths */
      {
        cairo_new_path(cr);
        double margin, line_width, margin_increment;
        GdkRGBA color[1];
        
        //line_width = 2 * DV_NODE_LINE_WIDTH;
        line_width = 2 * DV_NODE_LINE_WIDTH / V->S->zoom_ratio_x;
        if (line_width > 40 * DV_NODE_LINE_WIDTH)
          line_width = 40 * DV_NODE_LINE_WIDTH;
        margin = - 0.5 * line_width;
        margin_increment = - line_width;
    
        int i;
        for (i = 0; i < DV_NUM_CRITICAL_PATHS; i++)
          if (on_global_cp[i]) {
            gdk_rgba_parse(color, CS->cp_colors[i]);
            cairo_set_source_rgba(cr, color->red, color->green, color->blue, color->alpha);
            cairo_set_line_width(cr, line_width);
            cairo_rectangle(cr, xx - margin, yy - margin, w + 2 * margin, h + 2 * margin);
            cairo_stroke(cr);
            margin += margin_increment;
          }
      }

    }
    
  }
  
  /* Flag to draw infotag */
  if (dv_llist_has(V->D->P->itl, (void *) node->pii)) {
    dv_llist_add(V->D->itl, (void *) node);
  }
  
  cairo_restore(cr);
}

static void
dv_view_draw_timeline_node_r(dv_view_t * V, cairo_t * cr, dv_dag_node_t * node, int * parent_on_global_cp) {
  /* Counting statistics */
  V->S->ndh++;
  if (!node || !dv_is_set(node))
    return;
  
  /* Check if node is still on the global critical paths */
  int me_on_global_cp[DV_NUM_CRITICAL_PATHS];
  int i;
  for (i = 0; i < DV_NUM_CRITICAL_PATHS; i++) {
    if (parent_on_global_cp[i] && dv_node_flag_check(node->f, CS->oncp_flags[i]))
      me_on_global_cp[i] = 1;
    else
      me_on_global_cp[i] = 0;
  }
  
  /* Call inward */
  int hidden = dv_timeline_node_is_invisible(V, node);
  if (!hidden) {
    if (dv_is_union(node) && dv_is_inner_loaded(node) && dv_is_expanded(node)) {
      dv_view_draw_timeline_node_r(V, cr, node->head, me_on_global_cp);
    } else {
      dv_view_draw_timeline_node_1(V, cr, node, me_on_global_cp);
    }
  }
  
  /* Call link-along */
  if (!(hidden & DV_DAG_NODE_HIDDEN_RIGHT)) {
    dv_dag_node_t * next = NULL;
    while ( (next = dv_dag_node_traverse_nexts(node, next)) ) {
      dv_view_draw_timeline_node_r(V, cr, next, parent_on_global_cp);
    }
  }
}

void
dv_view_draw_timeline(dv_view_t * V, cairo_t * cr) {
  double time = dv_get_time();
  if (CS->verbose_level >= 2) {
    fprintf(stderr, "dv_view_draw_timeline()\n");
  }
  
  /* Set adaptive line width */
  double line_width = dv_min(DV_NODE_LINE_WIDTH, DV_NODE_LINE_WIDTH / dv_min(V->S->zoom_ratio_x, V->S->zoom_ratio_y));
  cairo_set_line_width(cr, line_width);
  
  /* White & grey colors */
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");
  GdkRGBA grey[1];
  gdk_rgba_parse(grey, "light grey");
  
  /* Draw background */
  /*
  cairo_new_path(cr);
  cairo_set_source_rgb(cr, grey->red, grey->green, grey->blue);
  cairo_paint(cr);
  cairo_set_source_rgb(cr, white->red, white->green, white->blue);
  double width = V->D->rt->c[V->S->coord].rw;
  double height = V->D->P->num_workers * (2 * V->D->radius);
  cairo_rectangle(cr, 0.0, 0.0, width, height);
  cairo_fill(cr);
  */
  
  /* Draw nodes */
  {
    dv_llist_init(V->D->itl);
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
    dv_view_draw_timeline_node_r(V, cr, V->D->rt, on_global_cp);
  }
  
  /* Draw worker numbers */
  cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);
  char s[DV_STRING_LENGTH];
  double xx, yy;
  xx = -75;
  yy = V->D->radius * 1.2;
  int i;
  for (i=0; i<V->D->P->num_workers; i++) {
    sprintf(s, "Worker %d", i);            
    cairo_move_to(cr, xx, yy);
    cairo_show_text(cr, s);
    yy += 2 * V->D->radius;
  }
  if (CS->verbose_level >= 2) {
    fprintf(stderr, "... done dv_view_draw_timeline(): %lf\n", dv_get_time() - time);
  }
}

/****** end of Draw ******/
