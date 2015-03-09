#include "dagviz.h"

dv_global_state_t  CS[1];

const char * const DV_COLORS[] =
  {"orange", "gold", "cyan", "azure", "green",
   "magenta", "brown1", "burlywood1", "peachpuff", "aquamarine",
   "chartreuse", "skyblue", "burlywood", "cadetblue", "chocolate",
   "coral", "cornflowerblue", "cornsilk4", "darkolivegreen1", "darkorange1",
   "khaki3", "lavenderblush2", "lemonchiffon1", "lightblue1", "lightcyan",
   "lightgoldenrod", "lightgoldenrodyellow", "lightpink2", "lightsalmon2", "lightskyblue1",
   "lightsteelblue3", "lightyellow3", "maroon1", "yellowgreen"};

const char * const DV_HISTOGRAM_COLORS[] =
  {"red", "green1", "blue", "magenta1", "cyan1", "yellow"};

const int DV_LINEAR_PATTERN_STOPS_NUM = 3;
const char * const DV_LINEAR_PATTERN_STOPS[] =
  //{"white", "black", "white"};
  //{"black", "white", "black"};
  ///{"green", "yellowgreen", "yellowgreen", "cyan"};
  //{"orange", "yellow", "coral"};
  {"orange", "yellow", "cyan"};

const char * const DV_RADIAL_PATTERN_STOPS[] =
  {"black", "white"};

const int DV_RADIAL_PATTERN_STOPS_NUM = 2;

/*---------------Environment Variables-----*/

static int dv_get_env_int(char * s, int * t) {
  char * v = getenv(s);
  if (v) {
    *t = atoi(v);
    return 1;
  }
  return 0;
}

static int dv_get_env_long(char * s, long * t) {
  char * v = getenv(s);
  if (v) {
    *t = atol(v);
    return 1;
  }
  return 0;
}

static int dv_get_env_string(char * s, char ** t) {
  char * v = getenv(s);
  if (v) {
    *t = strdup(v);
    return 1;
  }
  return 0;
}

static void dv_get_env() {
  //dv_get_env_int("DV_DEPTH", &S->cur_d);
}

/*---------------end of Environment Variables-----*/



/*-----------------Global State-----------------*/

void dv_global_state_init(dv_global_state_t *CS) {
  CS->nP = 0;
  CS->nD = 0;
  CS->nV = 0;
  CS->nVP = 0;
  CS->FL = NULL;
  CS->window = NULL;
  CS->activeV = NULL;
  CS->err = DV_OK;
  int i;
  for (i=0; i<DV_NUM_COLOR_POOLS; i++)
    CS->CP_sizes[i] = 0;
  dv_btsample_viewer_init(CS->btviewer);
  CS->box_viewport_configure = NULL;
  CS->nH = 0;
}

void dv_global_state_set_active_view(dv_view_t *V) {
  CS->activeV = V;
}

dv_view_t * dv_global_state_get_active_view() {
  return CS->activeV;
}

/*-----------------end of Global State-----------------*/



/*--------Interactive processing functions------------*/

void
dv_queue_draw(dv_view_t * V) {
  int i;
  for (i=0; i<CS->nVP; i++)
    if (V->I[i])
      gtk_widget_queue_draw(V->I[i]->VP->darea);
}

void
dv_queue_draw_d(dv_view_t * V) {
  dv_dag_t * D = V->D;
  int i;
  for (i=0; i<CS->nV; i++)
    if (CS->V[i].D == D)
      dv_queue_draw(&CS->V[i]);
}

void
dv_queue_draw_d_p(dv_view_t * V) {
  dv_pidag_t * P = V->D->P;
  int i;
  for (i=0; i<CS->nV; i++)
    if (CS->V[i].D->P == P)
      dv_queue_draw(&CS->V[i]);
}

static void
dv_do_zooming_x(dv_view_t * V, double new_zrx, double posx) {
  dv_view_status_t * S = V->S;
  posx -= S->basex + S->x;
  double deltax = posx / S->zoom_ratio_x * new_zrx - posx;
  S->x -= deltax;
  S->zoom_ratio_x = new_zrx;
  dv_queue_draw(V);
}

static void
dv_do_zooming_y(dv_view_t * V, double new_zry, double posy) {
  dv_view_status_t * S = V->S;
  posy -= S->basey + S->y;
  double deltay = posy / S->zoom_ratio_y * new_zry - posy;
  S->y -= deltay;
  S->zoom_ratio_y = new_zry;
  dv_queue_draw(V);
}

static void
dv_do_zooming(dv_view_t * V, double new_zrx, double new_zry, double posx, double posy) {
  dv_view_status_t * S = V->S;
  posx -= S->basex + S->x;
  posy -= S->basey + S->y;
  double deltax = posx / S->zoom_ratio_x * new_zrx - posx;
  double deltay = posy / S->zoom_ratio_y * new_zry - posy;
  S->x -= deltax;
  S->y -= deltay;
  S->zoom_ratio_x = new_zrx;
  S->zoom_ratio_y = new_zry;
  dv_queue_draw(V);
}

static void
dv_do_zoomfit_hor_(dv_view_t * V) {
  dv_dag_t * D = V->D;
  dv_view_status_t * S = V->S;
  double w = S->vpw;
  double h = S->vph;
  double zoom_ratio = 1.0;
  double x = 0.0;
  double y = 0.0;
  double d1, d2, dw;
  dv_node_coordinate_t *rtco = &D->rt->c[S->lt];
  switch (S->lt) {
  case 0:
    // DAG
    d1 = rtco->lw + rtco->rw;
    d2 = w - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    x -= (rtco->rw - rtco->lw) * 0.5 * zoom_ratio;
    break;
  case 1:
    // DAG with Boxes
    d1 = rtco->lw + rtco->rw;
    d2 = w - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    x -= (rtco->rw - rtco->lw) * 0.5 * zoom_ratio;
    break;
  case 2:
    // Vertical Timeline
    d1 = 2 * D->radius + (D->P->num_workers - 1) * DV_HDIS;
    d2 = w - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    break;
  case 3:
    // Horizontal Timeline
    d1 = 10 + rtco->rw;
    d2 = w - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    dw = D->P->num_workers * (D->radius * 2);
    y += (h - dw * zoom_ratio) * 0.4;
    break;
  case 4:
    // Parallelism profile
    d1 = dv_dag_scale_down(V->D, V->D->et - V->D->bt);
    d2 = w - 2 * DV_HISTOGRAM_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    y -= V->D->P->num_workers * 2 * V->D->radius * zoom_ratio;
    double dy = (h - DV_HISTOGRAM_MARGIN_DOWN
                 - DV_HISTOGRAM_MARGIN
                 - zoom_ratio
                 * (D->P->num_workers * 2 * D->radius
                    + D->H->max_e->sum_h)
                 ) / 2.0;
    if (dy > 0)
      y -= dy;
    break;
  default:
    dv_check(0);
  }
  S->zoom_ratio_x = S->zoom_ratio_y  = zoom_ratio;
  S->x = x;
  S->y = y;
}

static void
dv_do_zoomfit_hor(dv_view_t * V) {
  dv_do_zoomfit_hor_(V);
  dv_queue_draw(V);
}

static void
dv_do_zoomfit_ver_(dv_view_t * V) {
  dv_dag_t * D = V->D;
  dv_view_status_t * S = V->S;
  double w = S->vpw;
  double h = S->vph;
  double zoom_ratio = 1.0;
  double x = 0.0;
  double y = 0.0;
  double d1, d2;
  dv_node_coordinate_t * rtco = &D->rt->c[S->lt];
  switch (S->lt) {
  case 0:
    d1 = rtco->dw;
    d2 = h - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    x -= (rtco->rw - rtco->lw) * 0.5 * zoom_ratio;
    break;
  case 1:
    d1 = rtco->dw;
    d2 = h - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;    
    x -= (rtco->rw - rtco->lw) * 0.5 * zoom_ratio;
    break;
  case 2:
    // Vertical Timeline
    d1 = 10 + rtco->dw;
    d2 = h - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    double lrw = 2 * D->radius + (D->P->num_workers - 1) * DV_HDIS;
    x += (w - lrw * zoom_ratio) * 0.5;
    break;
  case 3:
    // Horizontal Timeline
    d1 = D->P->num_workers * (D->radius * 2);
    d2 = h - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    break;
  case 4:
    // Parallelism profile
    d1 = D->P->num_workers * (2 * D->radius) + D->H->max_e->sum_h;
    d2 = h - DV_HISTOGRAM_MARGIN - DV_HISTOGRAM_MARGIN_DOWN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    y -= D->P->num_workers * (2 * D->radius) * zoom_ratio;
    double dx = (w - 2 * DV_HISTOGRAM_MARGIN - zoom_ratio * dv_dag_scale_down(V->D, V->D->et - V->D->bt)) / 2.0;
    if (dx > 0)
      x += dx;
    break;
  default:
    dv_check(0);
  }
  S->zoom_ratio_x = S->zoom_ratio_y = zoom_ratio;
  S->x = x;
  S->y = y;
}

static void
dv_do_zoomfit_ver(dv_view_t * V) {
  dv_do_zoomfit_ver_(V);
  dv_queue_draw(V);
}

static void
dv_view_set_entry_radix_text(dv_view_t * V) {
  char str[DV_ENTRY_RADIX_MAX_LENGTH];
  double radix = dv_dag_get_radix(V->D);
  sprintf(str, "%lf", radix);
  int i;
  for (i=0; i<CS->nVP; i++)
    if (V->I[i]) {      
      gtk_entry_set_width_chars(GTK_ENTRY(V->I[i]->entry_radix), strlen(str));
      gtk_entry_set_text(GTK_ENTRY(V->I[i]->entry_radix), str);
    }
}

static void
dv_view_change_radix(dv_view_t * V, double radix) {
  dv_dag_set_radix(V->D, radix);
  dv_view_set_entry_radix_text(V);
}

static void
dv_view_change_sdt(dv_view_t * V, int new_sdt) {
  if (new_sdt != V->D->sdt) {
    V->D->sdt = new_sdt;
    dv_view_set_entry_radix_text(V);
  }
}

static void
dv_view_change_eaffix(dv_view_t * V, int active) {
  if (active)
    V->S->edge_affix = DV_EDGE_AFFIX_LENGTH;
  else
    V->S->edge_affix = 0;
  int i;
  for (i=0; i<CS->nVP; i++)
    if (V->I[i]) {
      if (active)
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(V->I[i]->togg_eaffix), TRUE);
      else
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(V->I[i]->togg_eaffix), FALSE);
    }
}

static void
dv_view_change_nc(dv_view_t * V, int new_nc) {
  int old_nc = V->S->nc;
  if (new_nc != old_nc) {
    V->S->nc = new_nc;
    int i;
    for (i=0; i<CS->nVP; i++)
      if (V->I[i])
        gtk_combo_box_set_active(GTK_COMBO_BOX(V->I[i]->combobox_nc), new_nc);
  }
}

static void
dv_view_do_zoomfit_based_on_lt(dv_view_t * V) {
  switch (V->S->lt) {
  case 0:
  case 1:
  case 2:
    dv_do_zoomfit_ver(V);
    break;
  case 3:
    dv_do_zoomfit_hor(V);
    break;
  case 4:
    dv_do_zoomfit_hor(V);
    break;
  default:
    dv_do_zoomfit_ver(V);
    break;
  }
}

static void
dv_view_change_lt(dv_view_t * V, int new_lt) {
  int old_lt = V->S->lt;
  if (new_lt != old_lt) {
    // edge affix, sdt
    switch (new_lt) {
    case 0:
      dv_view_change_nc(V, 0);
      dv_view_change_eaffix(V, 0);
      break;
    case 1:
      dv_view_change_nc(V, 0);
      dv_view_change_eaffix(V, 0);
      dv_view_change_sdt(V, 0);
      break;
    case 2:
    case 3:
    case 4:
      dv_view_change_sdt(V, 2);
      dv_view_change_nc(V, 2); // node kind
      break;
    default:
      break;
    }
    // Paraprof: check H structure
    if (new_lt == 4) {
      if (!V->D->H && CS->nH < DV_MAX_HISTOGRAM) {
        V->D->H = &CS->H[CS->nH];
        dv_histogram_init(V->D->H);
        V->D->H->D = V->D;
        CS->nH++;
      } else if (!V->D->H) {
        fprintf(stderr, "Error: run out of CS->H to allocate.\n");
        return;
      }      
    }
    // Change lt
    V->D->tolayout[old_lt]--;
    V->S->lt = new_lt;
    V->D->tolayout[new_lt]++;
    // Update I
    int i;
    for (i=0; i<CS->nVP; i++)
      if (V->I[i])
        gtk_combo_box_set_active(GTK_COMBO_BOX(V->I[i]->combobox_lt), new_lt);
    // Re-layout
    dv_view_layout(V);
    // zoomfit
    dv_view_do_zoomfit_based_on_lt(V);
  }
}

void
dv_view_clip(dv_view_t * V, cairo_t * cr) {
  cairo_rectangle(cr, DV_CLIPPING_FRAME_MARGIN, DV_CLIPPING_FRAME_MARGIN, V->S->vpw - DV_CLIPPING_FRAME_MARGIN * 2, V->S->vph - DV_CLIPPING_FRAME_MARGIN * 2);
  cairo_clip(cr);
}

double
dv_view_clip_get_bound_left(dv_view_t * V) {
  return (DV_CLIPPING_FRAME_MARGIN - V->S->basex - V->S->x) / V->S->zoom_ratio_x;
}

double
dv_view_clip_get_bound_right(dv_view_t * V) {
  return ((V->S->vpw - DV_CLIPPING_FRAME_MARGIN) - V->S->basex - V->S->x) / V->S->zoom_ratio_x;
}

double
dv_view_clip_get_bound_up(dv_view_t * V) {
  return (DV_CLIPPING_FRAME_MARGIN - V->S->basey - V->S->y) / V->S->zoom_ratio_y;
}

double
dv_view_clip_get_bound_down(dv_view_t * V) {
  return ((V->S->vph - DV_CLIPPING_FRAME_MARGIN) - V->S->basey - V->S->y) / V->S->zoom_ratio_y;
}

double
dv_view_cairo_coordinate_bound_left(dv_view_t * V) {
  return (DV_CAIRO_BOUND_MIN - V->S->basex - V->S->x) / V->S->zoom_ratio_x;
}

double
dv_view_cairo_coordinate_bound_right(dv_view_t * V) {
  return (DV_CAIRO_BOUND_MAX - V->S->basex - V->S->x) / V->S->zoom_ratio_x;
}

double
dv_view_cairo_coordinate_bound_up(dv_view_t * V) {
  return (DV_CAIRO_BOUND_MIN - V->S->basey - V->S->y) / V->S->zoom_ratio_y;
}

double
dv_view_cairo_coordinate_bound_down(dv_view_t * V) {
  return (DV_CAIRO_BOUND_MAX - V->S->basey - V->S->y) / V->S->zoom_ratio_y;
}

static void
dv_view_prepare_drawing(dv_view_t * V, cairo_t * cr) {
  dv_view_status_t * S = V->S;
  if (S->do_zoomfit) {
    dv_view_do_zoomfit_based_on_lt(V);
    S->do_zoomfit = 0;
  }
  
  cairo_save(cr);
  
  /* Draw graph */
  // Clipping
  dv_view_clip(V, cr);
  // Transforming
  cairo_matrix_t mt[1];
  cairo_matrix_init_translate(mt, S->basex + S->x, S->basey + S->y);
  cairo_matrix_scale(mt, S->zoom_ratio_x, S->zoom_ratio_y);
  cairo_transform(cr, mt);
  //fprintf(stderr, "matrix:\n%lf %lf %lf\n%lf %lf %lf\n", mt->xx, mt->xy, mt->x0, mt->yx, mt->yy, mt->y0);
  //fprintf(stderr, "to compare with:\n%lf %lf %lf\n%lf %lf %lf\n", S->zoom_ratio_x, 0.0, S->basex + S->x, 0.0, S->zoom_ratio_y, S->basey + S->y);
  // Transforming
  /*
  cairo_translate(cr, S->basex + S->x, S->basey + S->y);
  cairo_scale(cr, S->zoom_ratio_x, S->zoom_ratio_y);
  cairo_matrix_t mt_[1];
  cairo_get_matrix(cr, mt_);
  fprintf(stderr, "matrix:\n%lf %lf %lf\n%lf %lf %lf\n", mt_->xx, mt_->xy, mt_->x0, mt_->yx, mt_->yy, mt_->y0);
  fprintf(stderr, "to compare with:\n%lf %lf %lf\n%lf %lf %lf\n", mt->xx, mt->xy, mt->x0, mt->yx, mt->yy, mt->y0);
  */
  
  dv_view_draw(V, cr);
  
  /* Draw infotags */
  /* TODO: to make it not scale unequally infotags */
  //dv_view_draw_infotags(V, cr, NULL);
  
  cairo_restore(cr);
}

static void
dv_viewport_draw(dv_viewport_t * VP, cairo_t * cr) {
  dv_view_t * V;
  dv_dag_t * D;
  dv_view_status_t * S;
  int count = 0;
  int i;
  for (i=0; i<CS->nV; i++)
    if (VP->I[i]) {
      V = VP->I[i]->V;
      D = V->D;
      S = V->S;
      switch (S->lt) {
      case 0:
        S->basex = 0.5 * S->vpw;
        S->basey = DV_ZOOM_TO_FIT_MARGIN;
        break;
      case 1:
        //G->basex = 0.5 * S->vpw - 0.5 * (G->rt->rw - G->rt->lw);
        S->basex = 0.5 * S->vpw;
        S->basey = DV_ZOOM_TO_FIT_MARGIN;
        break;
      case 2:
      case 3:
        S->basex = DV_ZOOM_TO_FIT_MARGIN;
        S->basey = DV_ZOOM_TO_FIT_MARGIN;
        break;
      case 4:
        S->basex = DV_HISTOGRAM_MARGIN;
        S->basey = S->vph - DV_HISTOGRAM_MARGIN_DOWN;
        break;
      default:
        dv_check(0);
      }
      // Draw
      dv_view_prepare_drawing(V, cr);
      dv_view_draw_status(V, cr, count);
      count++;
    }
  dv_viewport_draw_label(VP, cr);
}

static void
dv_do_expanding_one_1(dv_view_t * V, dv_dag_node_t * node) {
  if (!dv_is_inner_loaded(node))
    if (dv_dag_build_node_inner(V->D, node) != DV_OK) return;
  dv_view_status_t * S = V->S;
  switch (S->lt) {
  case 0:
  case 1:
  case 2:
  case 3:
    // add to animation
    if (dv_is_shrinking(node)) {
      dv_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKING);
      dv_node_flag_set(node->f, DV_NODE_FLAG_SHRINKED);
      dv_node_flag_set(node->f, DV_NODE_FLAG_EXPANDING);
      dv_animation_reverse(S->a, node);
    } else {
      dv_node_flag_set(node->f, DV_NODE_FLAG_EXPANDING);
      dv_animation_add(S->a, node);
    }
    break;
  case 4:
    if (dv_is_shrinking(node)) {
      dv_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKING);
    } else if (dv_is_expanding(node)) {
      dv_node_flag_remove(node->f, DV_NODE_FLAG_EXPANDING);
    }
    dv_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKED);
    break;
  default:
    dv_check(0);
  }
}

static void
dv_do_expanding_one_r(dv_view_t * V, dv_dag_node_t * node) {
  if (!dv_is_set(node))
    dv_dag_node_set(V->D, node);
  if (dv_is_union(node)) {
    if ((!dv_is_inner_loaded(node)
         || dv_is_shrinked(node)
         || dv_is_shrinking(node))
        && !dv_is_expanding(node)) {
      // expand node
      dv_do_expanding_one_1(V, node);
    } else {
      /* Call inward */
      dv_check(node->head);
      dv_do_expanding_one_r(V, node->head);
    }
  }
  
  /* Call link-along */
  dv_dag_node_t *u = NULL;
  while (u = (dv_dag_node_t *) dv_llist_iterate_next(node->links, u)) {
    dv_do_expanding_one_r(V, u);
  }
}

static void
dv_do_expanding_one(dv_view_t * V) {
  dv_do_expanding_one_r(V, V->D->rt);
  if (!V->S->a->on) {
    dv_view_layout(V);
    dv_queue_draw_d(V);
  }
}

static void
dv_do_collapsing_one_1(dv_view_t * V, dv_dag_node_t * node) {
  dv_view_status_t * S = V->S;
  switch (S->lt) {
  case 0:
  case 1:
  case 2:
  case 3:
    // add to animation
    if (dv_is_expanding(node)) {
      dv_node_flag_remove(node->f, DV_NODE_FLAG_EXPANDING);
      dv_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKED);
      dv_node_flag_set(node->f, DV_NODE_FLAG_SHRINKING);
      dv_animation_reverse(S->a, node);
    } else {
      dv_node_flag_set(node->f, DV_NODE_FLAG_SHRINKING);
      dv_animation_add(S->a, node);
    }
    break;
  case 4:
    if (dv_is_expanding(node)) {
      dv_node_flag_remove(node->f, DV_NODE_FLAG_EXPANDING);
    } else if (dv_is_shrinking(node)) {
      dv_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKING);
    }
    dv_node_flag_set(node->f, DV_NODE_FLAG_SHRINKED);
    break;
  default:
    dv_check(0);
  }
}

static void
dv_do_collapsing_one_r(dv_view_t * V, dv_dag_node_t * node) {
  if (!dv_is_set(node))
    return;
  if (dv_is_union(node) && dv_is_inner_loaded(node)
      && !dv_is_shrinking(node)
      && (dv_is_expanded(node) || dv_is_expanding(node))) {
    // check if node has expanded node, excluding shrinking nodes
    int has_expanded_node = 0;
    dv_stack_t s[1];
    dv_stack_init(s);
    dv_check(node->head);
    dv_stack_push(s, (void *) node->head);
    while (s->top) {
      dv_dag_node_t * x = (dv_dag_node_t *) dv_stack_pop(s);
      if (dv_is_union(x) && dv_is_inner_loaded(x)
          && (dv_is_expanded(x) || dv_is_expanding(x))
          && !dv_is_shrinking(x))
        has_expanded_node = 1;
      dv_dag_node_t * xx = NULL;
      while (xx = (dv_dag_node_t *) dv_llist_iterate_next(x->links, xx)) {
        dv_stack_push(s, (void *) xx);
      }      
    }
    if (!has_expanded_node) {
      // collapsing node's parent
      dv_do_collapsing_one_1(V, node);
    } else {
      /* Call inward */
      dv_do_collapsing_one_r(V, node->head);
    }
  }
  
  /* Call link-along */
  dv_dag_node_t *u = NULL;
  while (u = (dv_dag_node_t *) dv_llist_iterate_next(node->links, u)) {
    dv_do_collapsing_one_r(V, u);
  }
}

static void
dv_do_collapsing_one(dv_view_t * V) {
  dv_do_collapsing_one_r(V, V->D->rt);
  if (!V->S->a->on) {
    dv_view_layout(V);
    dv_queue_draw_d(V);
  }
}

static dv_dag_node_t *
dv_do_finding_clicked_node_1(dv_view_t * V, double x, double y, dv_dag_node_t * node) {
  dv_dag_node_t * ret = NULL;
  dv_node_coordinate_t * c = &node->c[V->S->lt];
  double vc, hc;
  switch (V->S->lt) {
  case 0:
    // DAG
    vc = c->x;
    hc = c->y;
    if (vc - V->D->radius < x && x < vc + V->D->radius
        && hc < y && y < hc + 2 * V->D->radius) {
      ret = node;
    }
    break;
  case 1:
  case 2:
  case 3:
  case 4:
    // dagbox/timeline_ver/timeline/paraprof layouts
    if (c->x - c->lw < x && x < c->x + c->rw
        && c->y < y && y < c->y + c->dw) {
      ret = node;
    }
    break;
  default:
    dv_check(0);
  }
  return ret;
}

static dv_dag_node_t *
dv_do_finding_clicked_node_r(dv_view_t * V, double x, double y, dv_dag_node_t * node) {
  dv_dag_node_t * ret;
  /* Call inward */
  if (dv_is_union(node) && dv_is_inner_loaded(node)
      && !dv_is_shrinking(node)
      && (dv_is_expanded(node) || dv_is_expanding(node))) {
    ret = dv_do_finding_clicked_node_r(V, x, y, node->head);
    if (ret)
      return ret;
  } else if (dv_do_finding_clicked_node_1(V, x, y, node)) {
      return node;
  }
  /* Call link-along */
  dv_dag_node_t * u = NULL;
  while (u = (dv_dag_node_t *) dv_llist_iterate_next(node->links, u)) {
    ret = dv_do_finding_clicked_node_r(V, x, y, u);
    if (ret)
      return ret;
  }
  return 0;
}

static dv_dag_node_t *
dv_do_finding_clicked_node(dv_view_t * V, double x, double y) {
  return dv_do_finding_clicked_node_r(V, x, y, V->D->rt);
}

static void dv_do_set_focused_view(dv_view_t *V, int focused) {
  if (focused) {
    dv_global_state_set_active_view(V);
    V->S->focused = 1;
    int i;
    for (i=0; i<CS->nVP; i++)
      if (V->I[i])
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(V->I[i]->togg_focused), TRUE);
    for (i=0; i<CS->nV; i++)
      if (V != CS->V + i)
        dv_do_set_focused_view(CS->V + i, 0);
  } else {
    if (V == dv_global_state_get_active_view())
      dv_global_state_set_active_view(NULL);
    V->S->focused = 0;
    int i;
    for (i=0; i<CS->nVP; i++)
      if (V->I[i])
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(V->I[i]->togg_focused), FALSE);
  }
}

/*--------end of Interactive processing functions------------*/


/*-----------------VIEW's functions-----------------*/

static gboolean
on_draw_event(GtkWidget * widget, cairo_t * cr, gpointer user_data)
{
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  dv_viewport_draw(VP, cr);
  return FALSE;
}

static void on_btn_zoomfit_hor_clicked(GtkToolButton *toolbtn, gpointer user_data)
{
  dv_view_t *V = (dv_view_t *) user_data;
  dv_do_zoomfit_hor(V);
}

static void on_btn_zoomfit_ver_clicked(GtkToolButton *toolbtn, gpointer user_data)
{
  dv_view_t *V = (dv_view_t *) user_data;
  dv_do_zoomfit_ver(V);
}

static void on_btn_shrink_clicked(GtkToolButton *toolbtn, gpointer user_data)
{
  dv_view_t *V = (dv_view_t *) user_data;
  dv_do_collapsing_one(V);
}

static void on_btn_expand_clicked(GtkToolButton *toolbtn, gpointer user_data)
{
  dv_view_t *V = (dv_view_t *) user_data;
  dv_do_expanding_one(V);
}

static void
dv_do_scrolling(dv_view_t * V, GdkEventScroll * event) {
  double factor = 1.0;
  
  if (V->S->do_scale_radix || V->S->do_scale_radius) {
    
    if (V->S->do_scale_radix) {
      
      // Cal factor    
      if (event->direction == GDK_SCROLL_UP)
        factor /= DV_SCALE_INCREMENT;
      else if (event->direction == GDK_SCROLL_DOWN)
        factor *= DV_SCALE_INCREMENT;
      // Apply factor    
      double radix = dv_dag_get_radix(V->D);
      radix *= factor;
      dv_view_change_radix(V, radix);
      dv_view_layout(V);
      dv_queue_draw_d(V);
      
    }
    
    if (V->S->do_scale_radius) {
      
      // Cal factor
      factor = 1.0;
      if (event->direction == GDK_SCROLL_UP)
        factor *= DV_SCALE_INCREMENT;
      else if (event->direction == GDK_SCROLL_DOWN)
        factor /= DV_SCALE_INCREMENT;
      // Apply factor    
      V->D->radius *= factor;
      dv_view_layout(V);
      dv_queue_draw_d(V);
      
    }
    
  } else {
    
    // Cal factor    
    if (event->direction == GDK_SCROLL_UP)
      factor *= DV_ZOOM_INCREMENT;
    else if (event->direction == GDK_SCROLL_DOWN)
      factor /= DV_ZOOM_INCREMENT;
    // Apply factor    
    double zoomx = V->S->zoom_ratio_x;
    double zoomy = V->S->zoom_ratio_y;
    if (V->S->do_zoom_x)
      zoomx *= factor;
    if (V->S->do_zoom_y)
      zoomy *= factor;
    dv_do_zooming(V, zoomx, zoomy, event->x, event->y);
    
  }
}

static gboolean
on_scroll_event(GtkWidget * widget, GdkEventScroll * event, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  dv_view_interface_t * I = dv_viewport_get_interface_to_view(VP, CS->activeV);
  if (I) {
    dv_do_scrolling(CS->activeV, event);
  } else {
    int i;
    for (i=0; i<CS->nV; i++)
      if (VP->I[i])
        dv_do_scrolling(VP->I[i]->V, event);
  }
  return TRUE;
}

static void
dv_do_button_event(dv_view_t * V, GdkEventButton * event) {
  dv_dag_t * D = V->D;
  dv_llist_t * itl = D->P->itl;
  dv_view_status_t * S = V->S;
  if (event->type == GDK_BUTTON_PRESS) {
    // Drag
    S->drag_on = 1;
    S->pressx = event->x;
    S->pressy = event->y;
    S->accdisx = 0.0;
    S->accdisy = 0.0;
  }  else if (event->type == GDK_BUTTON_RELEASE) {
    // Drag
    S->drag_on = 0;
    // Node clicked
    if (S->accdisx < DV_SAFE_CLICK_RANGE
        && S->accdisy < DV_SAFE_CLICK_RANGE) {
      double ox = (event->x - S->basex - S->x) / S->zoom_ratio_x;
      double oy = (event->y - S->basey - S->y) / S->zoom_ratio_y;
      dv_dag_node_t * node = dv_do_finding_clicked_node(V, ox, oy);
      if (node) {
        switch (S->cm) {
        case 0:
          // Info tag
          if (!dv_llist_remove(itl, (void *) node->pii)) {
            dv_llist_add(itl, (void *) node->pii);
          }
          dv_queue_draw_d_p(V);
          break;
        case 1:
          // Expand/Collapse
          if (dv_is_union(node)) {
            if ((!dv_is_inner_loaded(node) || dv_is_shrinked(node) || dv_is_shrinking(node))
                && !dv_is_expanding(node))
              dv_do_expanding_one_1(V, node);
          } else {
            dv_do_collapsing_one_r(V, node->parent);
          }
          break;
        default:
          dv_check(0);
        }
      }
    }
  } else if (event->type == GDK_2BUTTON_PRESS) {
    // Shrink/Expand
  }
}

static gboolean on_button_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  dv_viewport_t *VP = (dv_viewport_t *) user_data;
  dv_view_interface_t *I = dv_viewport_get_interface_to_view(VP, CS->activeV);
  if (I) {
    dv_do_button_event(CS->activeV, event);
  } else {
    int i;
    for (i=0; i<CS->nV; i++)
      if (VP->I[i])
        dv_do_button_event(VP->I[i]->V, event);
  }
  return TRUE;
}

static void dv_do_motion_event(dv_view_t *V, GdkEventMotion *event) {
  dv_view_status_t *S = V->S;
  if (S->drag_on) {
    // Drag
    double deltax = event->x - S->pressx;
    double deltay = event->y - S->pressy;
    S->x += deltax;
    S->y += deltay;
    S->accdisx += deltax;
    S->accdisy += deltay;
    S->pressx = event->x;
    S->pressy = event->y;
    dv_queue_draw_d(V);
  }
}

static gboolean on_motion_event(GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
  dv_viewport_t *VP = (dv_viewport_t *) user_data;
  dv_view_interface_t *I = dv_viewport_get_interface_to_view(VP, CS->activeV);
  if (I) {
    dv_do_motion_event(CS->activeV, event);
  } else {
    int i;
    for (i=0; i<CS->nV; i++)
      if (VP->I[i])
        dv_do_motion_event(VP->I[i]->V, event);
  }
  return TRUE;
}

static gboolean
on_combobox_lt_changed(GtkComboBox * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  int new_lt = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  dv_check(0 <= new_lt && new_lt < DV_NUM_LAYOUT_TYPES);
  dv_view_change_lt(V, new_lt);
  return TRUE;  
}

static gboolean
on_combobox_nc_changed(GtkComboBox * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  dv_view_change_nc(V, gtk_combo_box_get_active(GTK_COMBO_BOX(widget)));
  dv_queue_draw(V);
  return TRUE;
}

static gboolean on_combobox_sdt_changed(GtkComboBox *widget, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  dv_view_change_sdt(V, gtk_combo_box_get_active(GTK_COMBO_BOX(widget)));
  dv_view_layout(V);
  dv_queue_draw_d(V);
  return TRUE;
}

static gboolean on_entry_radix_activate(GtkEntry *entry, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  double radix = atof(gtk_entry_get_text(GTK_ENTRY(entry)));
  dv_view_change_radix(V, radix);
  dv_view_layout(V);
  dv_queue_draw_d(V);
  return TRUE;
}

static dv_dag_node_t * dv_find_node_with_pii_r(dv_view_t *V, long pii, dv_dag_node_t *node) {
  if (node->pii == pii)
    return node;
  dv_dag_node_t *ret;
  /* Call inward */
  if (dv_is_union(node) && dv_is_inner_loaded(node)
      && !dv_is_shrinking(node)
      && (dv_is_expanded(node) || dv_is_expanding(node))) {
    ret = dv_find_node_with_pii_r(V, pii, node->head);
    if (ret)
      return ret;
  }
  /* Call link-along */
  dv_dag_node_t *u = NULL;
  while (u = (dv_dag_node_t *) dv_llist_iterate_next(node->links, u)) {
    ret = dv_find_node_with_pii_r(V, pii, u);
    if (ret)
      return ret;
  }
  return 0;
}

static gboolean on_entry_search_activate(GtkEntry *entry, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  // Get node
  long pii = atol(gtk_entry_get_text(GTK_ENTRY(entry)));
  dv_dag_node_t *node = dv_find_node_with_pii_r(V, pii, D->rt);
  if (!node)
    return TRUE;
  // Get target zoom ratio, x, y
  dv_node_coordinate_t *co = &node->c[S->lt];
  double zoom_ratio = 1.0;
  double x = 0.0;
  double y = 0.0;
  double d1, d2;
  switch (S->lt) {
  case 0:
    // DAG
    d1 = co->lw + co->rw;
    d2 = S->vpw - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    d1 = co->dw;
    d2 = S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN;
    if (d1 > d2)
      zoom_ratio = dv_min(zoom_ratio, d2 / d1);
    zoom_ratio = dv_min(zoom_ratio, dv_min(S->zoom_ratio_x, S->zoom_ratio_y));
    x -= (co->x + (co->rw - co->lw) * 0.5) * zoom_ratio;
    y -= co->y * zoom_ratio - (S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN - co->dw * zoom_ratio) * 0.5;
    break;
  case 1:
    // DAG box
    d1 = co->lw + co->rw;
    d2 = S->vpw - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    d1 = co->dw;
    d2 = S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN;
    if (d1 > d2)
      zoom_ratio = dv_min(zoom_ratio, d2 / d1);
    zoom_ratio = dv_min(zoom_ratio, dv_min(S->zoom_ratio_x, S->zoom_ratio_y));
    x -= (co->x + (co->rw - co->lw) * 0.5) * zoom_ratio;
    y -= co->y * zoom_ratio - (S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN - co->dw * zoom_ratio) * 0.5;
    break;
  case 2:
    // Vertical Timeline
    d1 = 2*D->radius + (D->P->num_workers - 1) * DV_HDIS;
    d2 = S->vpw - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    d1 = co->dw;
    d2 = S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN;
    if (d1 > d2)
      zoom_ratio = dv_min(zoom_ratio, d2 / d1);
    zoom_ratio = dv_min(zoom_ratio, dv_min(S->zoom_ratio_x, S->zoom_ratio_y));
    x -= (co->x + (co->rw - co->lw) * 0.5) * zoom_ratio - S->vpw * 0.5;
    y -= co->y * zoom_ratio - (S->vph - DV_ZOOM_TO_FIT_MARGIN  - DV_ZOOM_TO_FIT_MARGIN_DOWN - co->dw * zoom_ratio) * 0.5;
    break;
  case 3:
    // Horizontal Timeline
    d1 = D->P->num_workers * (D->radius * 2);
    d2 = S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    d1 = co->rw;
    d2 = S->vpw - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = dv_min(zoom_ratio, d2 / d1);
    x -= (co->x + co->rw * 0.5) * zoom_ratio - S->vpw * 0.5;
    y -= co->y * zoom_ratio - (S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN - co->dw * zoom_ratio) * 0.5;
    break;
  case 4:
    // Parallelism profile
    d1 = D->P->num_workers * (D->radius * 2);
    d2 = S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    d1 = co->rw;
    d2 = S->vpw - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = dv_min(zoom_ratio, d2 / d1);
    x -= (co->x + co->rw * 0.5) * zoom_ratio - S->vpw * 0.5;
    y -= co->y * zoom_ratio - (S->vph - DV_ZOOM_TO_FIT_MARGIN - DV_ZOOM_TO_FIT_MARGIN_DOWN - co->dw * zoom_ratio) * 0.5;
    break;
  default:
    dv_check(0);
  }
  // Set info tag
  if (!dv_llist_has(D->P->itl, (void *) pii)) {
    dv_llist_add(D->P->itl, (void *) pii);
  }  
  // Set motion
  if (!S->m->on)
    dv_motion_start(S->m, pii, x, y, zoom_ratio, zoom_ratio);
  else
    dv_motion_reset_target(S->m, pii, x, y, zoom_ratio, zoom_ratio);
  return TRUE;
}

static gboolean on_combobox_frombt_changed(GtkComboBox *widget, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  V->D->frombt = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  dv_view_layout(V);
  dv_queue_draw_d(V);
  return TRUE;
}

static gboolean on_combobox_et_changed(GtkComboBox *widget, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  V->S->et = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  dv_queue_draw(V);
  return TRUE;
}

static void
on_togg_eaffix_toggled(GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    dv_view_change_eaffix(V, 1);
  } else {
    dv_view_change_eaffix(V, 0);
  }
  dv_queue_draw(V);
}

static void on_togg_focused_toggled(GtkWidget *widget, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  int focused = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  dv_do_set_focused_view(V, focused);
}

static gboolean on_combobox_cm_changed(GtkComboBox *widget, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  V->S->cm = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  return TRUE;
}

static gboolean
on_darea_configure_event(GtkWidget * widget, GdkEventConfigure * event, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  VP->vpw = event->width;
  VP->vph = event->height;
  dv_view_t * V = NULL;
  int i;
  for (i=0; i<CS->nV; i++)
    if (VP->I[i]) {
      V = VP->I[i]->V;
      if (V->mainVP == VP) {
        V->S->vpw = event->width;
        V->S->vph = event->height;
      }
    }
  return TRUE;
}

static gboolean on_window_key_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  dv_view_t *aV = dv_global_state_get_active_view();
  if (!aV)
    return FALSE;
  
  GdkModifierType modifiers = gtk_accelerator_get_default_mod_mask();
  
  GdkEventKey *e = (GdkEventKey *) event;
  int i;
  //printf("key: %d\n", e->keyval);
  switch (e->keyval) {
  case 120: /* x */
    dv_do_expanding_one(aV);    
    return TRUE;
  case 99: /* c */
    dv_do_collapsing_one(aV);
    return TRUE;
  case 104: /* h */
    dv_do_zoomfit_hor(aV);
    return TRUE;
  case 118: /* v */
    dv_do_zoomfit_ver(aV);
    return TRUE;
  case 49: /* Ctrl + 1 */
    if ((e->state & modifiers) == GDK_CONTROL_MASK) {
      dv_view_change_lt(aV, 0);
      return TRUE;
    }
    break;
  case 50: /* Ctrl + 2 */
    if ((e->state & modifiers) == GDK_CONTROL_MASK) {
      dv_view_change_lt(aV, 1);
      return TRUE;
    }
    break;
  case 51: /* Ctrl + 3 */
    if ((e->state & modifiers) == GDK_CONTROL_MASK) {
      dv_view_change_lt(aV, 2);
      return TRUE;
    }
    break;
  case 52: /* Ctrl + 4 */
    if ((e->state & modifiers) == GDK_CONTROL_MASK) {
      dv_view_change_lt(aV, 3);
      return TRUE;
    }
    break;
  case 53: /* Ctrl + 5 */
    if ((e->state & modifiers) == GDK_CONTROL_MASK) {
      dv_view_change_lt(aV, 4);
      return TRUE;
    }
    break;
  case 65289: /* tab */
    if (!aV)
      i = 0;
    else {
      i = aV - CS->V;
      int boo = 0;
      int count = 0;
      while (boo == 0 && count < CS->nV) {
        i = (i + 1) % CS->nV;
        int j;
        for (j=0; j<CS->nVP; j++)
          if (CS->V[i].I[j])
            boo = 1;
        count++;
      }
    }
    dv_do_set_focused_view(CS->V + i, 1);
    break;
  case 65361: /* left */
    aV->S->x += 15;
    dv_queue_draw(aV);
    return TRUE;
  case 65362: /* up */
    aV->S->y += 15;
    dv_queue_draw(aV);
    return TRUE;
  case 65363: /* right */
    aV->S->x -= 15;
    dv_queue_draw(aV);
    return TRUE;
  case 65364: /* down */
    aV->S->y -= 15;
    dv_queue_draw(aV);
    return TRUE;
  default:
    return FALSE;
  }
  return FALSE;
}

static void
on_btn_view_attributes_clicked(GtkToolButton * toolbtn, gpointer user_data) {
  dv_view_interface_t * I = (dv_view_interface_t *) user_data;

  // Adjust I's attribute values
  dv_view_interface_set_values(I->V, I);
  
  // Build dialog
  GtkWidget * dialog = gtk_dialog_new();
  char s[30];
  sprintf(s, "VIEW %ld's Attributes", I->V - CS->V);
  gtk_window_set_title(GTK_WINDOW(dialog), s);
  //gtk_window_set_default_size(GTK_WINDOW(dialog), 600, -1);
  gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
  GtkWidget * dialog_vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  gtk_box_pack_start(GTK_BOX(dialog_vbox), I->grid, TRUE, TRUE, 0);
  
  // Run
  gtk_widget_show_all(dialog_vbox);
  gtk_dialog_run(GTK_DIALOG(dialog));
  
  // Destroy
  gtk_container_remove(GTK_CONTAINER(dialog_vbox), I->grid);
  gtk_widget_destroy(dialog);
}

static void on_btn_choose_bt_file_clicked(GtkToolButton *toolbtn, gpointer user_data) {
  dv_btsample_viewer_t * btviewer = (dv_btsample_viewer_t *) user_data;

  // Build dialog
  GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
  GtkWidget * dialog = gtk_file_chooser_dialog_new("Choose BT File",
                                                   0,
                                                   action,
                                                   "_Cancel",
                                                   GTK_RESPONSE_CANCEL,
                                                   "Ch_oose",
                                                   GTK_RESPONSE_ACCEPT,
                                                   NULL);
  gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), btviewer->P->fn);
  gint res = gtk_dialog_run(GTK_DIALOG(dialog));
  if (res == GTK_RESPONSE_ACCEPT) {
    GtkFileChooser * chooser = GTK_FILE_CHOOSER(dialog);
    char * filename = gtk_file_chooser_get_filename(chooser);
    gtk_entry_set_text(GTK_ENTRY(btviewer->entry_bt_file_name), filename);
    g_free(filename);
  }
  
  gtk_widget_destroy(dialog);  
}

static void on_btn_choose_binary_file_clicked(GtkToolButton *toolbtn, gpointer user_data) {
  dv_btsample_viewer_t * btviewer = (dv_btsample_viewer_t *) user_data;

  // Build dialog
  GtkFileChooserAction action = GTK_FILE_CHOOSER_ACTION_OPEN;
  GtkWidget * dialog = gtk_file_chooser_dialog_new("Choose Binary File",
                                                   0,
                                                   action,
                                                   "_Cancel",
                                                   GTK_RESPONSE_CANCEL,
                                                   "Ch_oose",
                                                   GTK_RESPONSE_ACCEPT,
                                                   NULL);
  gtk_file_chooser_set_filename(GTK_FILE_CHOOSER(dialog), btviewer->P->fn);
  gint res = gtk_dialog_run(GTK_DIALOG(dialog));
  if (res == GTK_RESPONSE_ACCEPT) {
    GtkFileChooser * chooser = GTK_FILE_CHOOSER(dialog);
    char * filename = gtk_file_chooser_get_filename(chooser);
    gtk_entry_set_text(GTK_ENTRY(btviewer->entry_binary_file_name), filename);
    g_free(filename);
  }
  
  gtk_widget_destroy(dialog);  
}

static void on_btn_find_node_clicked(GtkToolButton *toolbtn, gpointer user_data) {
  dv_btsample_viewer_t * btviewer = (dv_btsample_viewer_t *) user_data;
  long pii = atol(gtk_entry_get_text(GTK_ENTRY(btviewer->entry_node_id)));
  //dv_dag_node_t *node = dv_find_node_with_pii_r(CS->activeV, pii, CS->activeV->D->rt);
  dr_pi_dag_node *pi = dv_pidag_get_node_with_id(btviewer->P, pii);
  if (pi) {
    gtk_combo_box_set_active(GTK_COMBO_BOX(btviewer->combobox_worker), pi->info.worker);
    char str[30];
    sprintf(str, "%llu", (unsigned long long) pi->info.start.t);
    gtk_entry_set_text(GTK_ENTRY(btviewer->entry_time_from), str);
    sprintf(str, "%llu", (unsigned long long) pi->info.end.t);
    gtk_entry_set_text(GTK_ENTRY(btviewer->entry_time_to), str);
  } else {
    gtk_combo_box_set_active(GTK_COMBO_BOX(btviewer->combobox_worker), -1);
    gtk_entry_set_text(GTK_ENTRY(btviewer->entry_time_from), "");
    gtk_entry_set_text(GTK_ENTRY(btviewer->entry_time_to), "");
  }
}

static void on_btn_run_view_bt_samples_clicked(GtkToolButton *toolbtn, gpointer user_data) {
  dv_btsample_viewer_t * btviewer = (dv_btsample_viewer_t *) user_data;

  int worker = gtk_combo_box_get_active(GTK_COMBO_BOX(btviewer->combobox_worker));
  unsigned long long from = atoll(gtk_entry_get_text(GTK_ENTRY(btviewer->entry_time_from)));
  unsigned long long to = atoll(gtk_entry_get_text(GTK_ENTRY(btviewer->entry_time_to)));
  if (from < btviewer->P->start_clock)
    from += btviewer->P->start_clock;
  if (to < btviewer->P->start_clock)
    to += btviewer->P->start_clock;

  dv_btsample_viewer_extract_interval(btviewer, worker, from, to);
}

static void
on_checkbox_xzoom_toggled(GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->do_zoom_x = 1 - V->S->do_zoom_x;
}

static void
on_checkbox_yzoom_toggled(GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->do_zoom_y = 1 - V->S->do_zoom_y;
}

static void
on_checkbox_scale_radix_toggled(GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->do_scale_radix = 1 - V->S->do_scale_radix;
}

static void
on_checkbox_scale_radius_toggled(GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->do_scale_radius = 1 - V->S->do_scale_radius;
}


void dv_view_status_init(dv_view_t *V, dv_view_status_t *S) {
  S->drag_on = 0;
  S->pressx = S->pressy = 0.0;
  S->accdisx = S->accdisy = 0.0;
  S->nc = DV_NODE_COLOR_INIT;
  S->vpw = S->vph = 0.0;
  dv_animation_init(V, S->a);
  S->nd = 0;
  S->lt = DV_LAYOUT_TYPE_INIT;
  S->et = DV_EDGE_TYPE_INIT;
  S->edge_affix = DV_EDGE_AFFIX_LENGTH;
  S->cm = DV_CLICK_MODE_INIT;
  S->ndh = 0;
  S->focused = 0;
  // Drawing parameters
  S->do_zoomfit = 1;
  S->x = S->y = 0.0;
  S->basex = S->basey = 0.0;
  S->zoom_ratio_x = 1.0;
  S->zoom_ratio_y = 1.0;
  S->do_zoom_x = 1;
  S->do_zoom_y = 1;
  S->do_scale_radix = 0;
  S->do_scale_radius = 0;
  dv_motion_init(S->m, V);
}

void dv_view_init(dv_view_t *V) {
  V->D = NULL;
  dv_view_status_init(V, V->S);
  int i;
  for (i=0; i<CS->nVP; i++)
    V->I[i] = NULL;
  V->mainVP = NULL;
}

dv_view_t * dv_view_create_new_with_dag(dv_dag_t *D) {
  /* Get new VIEW */
  dv_check(CS->nV < DV_MAX_VIEW);
  dv_view_t * V = &CS->V[CS->nV++];
  dv_view_init(V);

  // Set values
  V->D = D;
  D->tolayout[V->S->lt]++;

  return V;
}

void * dv_view_interface_set_values(dv_view_t *V, dv_view_interface_t *I) {
  dv_view_status_t *S = V->S;
  // Focused toggle
  if (S->focused)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(I->togg_focused), TRUE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(I->togg_focused), FALSE);
  // Layout type combobox
  gtk_combo_box_set_active(GTK_COMBO_BOX(I->combobox_lt), S->lt);
  // Node color combobox
  gtk_combo_box_set_active(GTK_COMBO_BOX(I->combobox_nc), S->nc);
  // Scale-down type combobox
  gtk_combo_box_set_active(GTK_COMBO_BOX(I->combobox_sdt), V->D->sdt);
  // Frombt combobox
  gtk_combo_box_set_active(GTK_COMBO_BOX(I->combobox_frombt), V->D->frombt);
  // Edge type combobox
  gtk_combo_box_set_active(GTK_COMBO_BOX(I->combobox_et), S->et);
  // Edge affix toggle
  if (S->edge_affix == 0)
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(I->togg_eaffix), FALSE);
  else
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(I->togg_eaffix), TRUE);
  // Radix value input
  dv_view_set_entry_radix_text(I->V);
}

dv_view_interface_t *
dv_view_interface_create_new(dv_view_t * V, dv_viewport_t * VP) {
  // Create a new
  dv_view_interface_t * I = (dv_view_interface_t *) dv_malloc(sizeof(dv_view_interface_t));
  I->V = V;
  I->VP = VP;

  // For toolbar
  I->toolbar = gtk_toolbar_new();
  g_object_ref(I->toolbar);
  
  char s[10];
  sprintf(s, "VIEW %ld", V - CS->V);
  I->togg_focused = gtk_toggle_button_new_with_label(s);
  
  I->entry_search = gtk_entry_new();
  I->checkbox_xzoom = gtk_check_button_new_with_label("X Zoom");
  I->checkbox_yzoom = gtk_check_button_new_with_label("Y Zoom");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(I->checkbox_xzoom), V->S->do_zoom_x);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(I->checkbox_yzoom), V->S->do_zoom_y);
  I->checkbox_scale_radix = gtk_check_button_new_with_label("Scale Node Length");
  I->checkbox_scale_radius = gtk_check_button_new_with_label("Scale Node Width");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(I->checkbox_scale_radix), V->S->do_scale_radix);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(I->checkbox_scale_radius), V->S->do_scale_radius);

  // For grid
  I->grid = gtk_grid_new();
  g_object_ref(I->grid);
  
  I->combobox_lt = gtk_combo_box_text_new();
  I->combobox_nc = gtk_combo_box_text_new();
  I->combobox_sdt = gtk_combo_box_text_new();
  I->entry_radix = gtk_entry_new();
  I->combobox_frombt = gtk_combo_box_text_new();
  I->combobox_et = gtk_combo_box_text_new();
  I->togg_eaffix = gtk_toggle_button_new_with_label("Edge Affix");

  dv_view_status_t * S = V->S;
  
  // White color
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");

  // Toolbar
  GtkWidget * toolbar = I->toolbar;
  //gtk_widget_override_background_color(GTK_WIDGET(toolbar), GTK_STATE_FLAG_NORMAL, white);

  // Focused toggle
  GtkToolItem * btn_togg_focused = gtk_tool_item_new();
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_togg_focused, -1);
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_togg_focused), "Indicates if this VIEW is focused for hotkeys (use tab key to change btw VIEWs)");
  GtkWidget *togg_focused = I->togg_focused;
  gtk_container_add(GTK_CONTAINER(btn_togg_focused), togg_focused);
  g_signal_connect(G_OBJECT(togg_focused), "toggled", G_CALLBACK(on_togg_focused_toggled), (void *) V);

  // Separator
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

  // View-attribute dialog button
  GtkToolItem *btn_attrs = gtk_tool_button_new(NULL, NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_attrs, -1);
  gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_attrs), "preferences-system");
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_attrs), "Open dialog to adjust VIEW's attributes");
  g_signal_connect(G_OBJECT(btn_attrs), "clicked", G_CALLBACK(on_btn_view_attributes_clicked), (void *) I);

  // Click mode combobox
  GtkToolItem *btn_combo_cm = gtk_tool_item_new();
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo_cm, -1);
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_combo_cm), "What to do when clicking a node");
  GtkWidget *combobox_cm = gtk_combo_box_text_new();
  gtk_container_add(GTK_CONTAINER(btn_combo_cm), combobox_cm);
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_cm), "info", "Show info tag of node");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_cm), "expand", "Expand/Collapse node");
  gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_cm), S->cm);
  g_signal_connect(G_OBJECT(combobox_cm), "changed", G_CALLBACK(on_combobox_cm_changed), (void *) V);

  // Zoomfit-horizontally button
  GtkToolItem *btn_zoomfit_hor = gtk_tool_button_new(NULL, NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoomfit_hor, -1);
  gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_zoomfit_hor), "zoom-fit-best");
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_zoomfit_hor), "Fit horizontally (h)");
  g_signal_connect(G_OBJECT(btn_zoomfit_hor), "clicked", G_CALLBACK(on_btn_zoomfit_hor_clicked), (void *) V);

  // Zoomfit-vertically button
  GtkToolItem *btn_zoomfit_ver = gtk_tool_button_new(NULL, NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoomfit_ver, -1);
  gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_zoomfit_ver), "zoom-fit-best");
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_zoomfit_ver), "Fit vertically (v)");
  g_signal_connect(G_OBJECT(btn_zoomfit_ver), "clicked", G_CALLBACK(on_btn_zoomfit_ver_clicked), (void *) V);

  // Shrink/Expand buttons
  GtkToolItem *btn_shrink = gtk_tool_button_new(NULL, NULL);
  GtkToolItem *btn_expand = gtk_tool_button_new(NULL, NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_shrink, -1);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_expand, -1);
  gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_shrink), "zoom-out");
  gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_expand), "zoom-in");
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_shrink), "Collapse one depth (c)"); 
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_expand), "Expand one depth (x)");
  g_signal_connect(G_OBJECT(btn_shrink), "clicked", G_CALLBACK(on_btn_shrink_clicked), (void *) V);  
  g_signal_connect(G_OBJECT(btn_expand), "clicked", G_CALLBACK(on_btn_expand_clicked), (void *) V);

  // Separator
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

  // X, Y zoom check boxes
  GtkToolItem * btn_xzoom = gtk_tool_item_new();
  GtkToolItem * btn_yzoom = gtk_tool_item_new();
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_xzoom, -1);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_yzoom, -1);
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_xzoom), "Cairo's horizontal zoom when scrolling");
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_yzoom), "Cairo's vertical zoom when scrolling");
  gtk_container_add(GTK_CONTAINER(btn_xzoom), I->checkbox_xzoom);
  gtk_container_add(GTK_CONTAINER(btn_yzoom), I->checkbox_yzoom);
  g_signal_connect(G_OBJECT(I->checkbox_xzoom), "toggled", G_CALLBACK(on_checkbox_xzoom_toggled), (void *) V);
  g_signal_connect(G_OBJECT(I->checkbox_yzoom), "toggled", G_CALLBACK(on_checkbox_yzoom_toggled), (void *) V);

  // Separator
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

  // Scale radix, radius
  GtkToolItem * btn_scale_radix = gtk_tool_item_new();
  GtkToolItem * btn_scale_radius = gtk_tool_item_new();
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_scale_radix, -1);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_scale_radius, -1);
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_scale_radix), "Scale node's lenth when scrolling");
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_scale_radius), "Scale node's width when scrolling");
  gtk_container_add(GTK_CONTAINER(btn_scale_radix), I->checkbox_scale_radix);
  gtk_container_add(GTK_CONTAINER(btn_scale_radius), I->checkbox_scale_radius);
  g_signal_connect(G_OBJECT(I->checkbox_scale_radix), "toggled", G_CALLBACK(on_checkbox_scale_radix_toggled), (void *) V);
  g_signal_connect(G_OBJECT(I->checkbox_scale_radius), "toggled", G_CALLBACK(on_checkbox_scale_radius_toggled), (void *) V);

  // Separator
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

  // Search Entry
  GtkToolItem *btn_entry_search = gtk_tool_item_new();
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_entry_search, -1);
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_entry_search), "Search by node's index in DAG file (first number in node's info tag)");
  GtkWidget *entry_search = I->entry_search;
  gtk_container_add(GTK_CONTAINER(btn_entry_search), entry_search);
  gtk_entry_set_placeholder_text(GTK_ENTRY(entry_search), "Search");
  gtk_entry_set_max_length(GTK_ENTRY(entry_search), 7);
  g_signal_connect(G_OBJECT(entry_search), "activate", G_CALLBACK(on_entry_search_activate), (void *) V);


  // Layout type combobox
  GtkWidget * combobox_lt = I->combobox_lt;
  gtk_widget_set_tooltip_text(GTK_WIDGET(combobox_lt), "How to layout nodes");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "dag", "DAG");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "dagbox", "DAG with boxes");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "timelinev", "Vertical timeline");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "timeline", "Horizontal timeline");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "paraprof", "Parallelism profile");
  g_signal_connect(G_OBJECT(combobox_lt), "changed", G_CALLBACK(on_combobox_lt_changed), (void *) V);

  // Node color combobox
  GtkWidget *combobox_nc = I->combobox_nc;
  gtk_widget_set_tooltip_text(GTK_WIDGET(combobox_nc), "How to color nodes");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "worker", "Worker");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "cpu", "CPU");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "kind", "Node kind");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "code_start", "Code start");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "code_end", "Code end");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "code_start_end", "Code start-end pair");
  g_signal_connect(G_OBJECT(combobox_nc), "changed", G_CALLBACK(on_combobox_nc_changed), (void *) V);

  // Scale-down type combobox
  GtkWidget *combobox_sdt = I->combobox_sdt;
  gtk_widget_set_tooltip_text(GTK_WIDGET(combobox_sdt), "How to scale down nodes' length");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_sdt), "log", "Log");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_sdt), "power", "Power");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_sdt), "linear", "Linear");
  g_signal_connect(G_OBJECT(combobox_sdt), "changed", G_CALLBACK(on_combobox_sdt_changed), (void *) V);

  // Radix value input
  GtkWidget *entry_radix = I->entry_radix;
  gtk_widget_set_tooltip_text(GTK_WIDGET(entry_radix), "Radix of the scale-down function");
  g_signal_connect(G_OBJECT(entry_radix), "activate", G_CALLBACK(on_entry_radix_activate), (void *) V);

  // Frombt combobox
  GtkWidget *combobox_frombt = I->combobox_frombt;
  gtk_widget_set_tooltip_text(GTK_WIDGET(combobox_frombt), "Scale down overall from beginning time (must for timeline)");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_frombt), "not", "No");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_frombt), "frombt", "Yes");
  g_signal_connect(G_OBJECT(combobox_frombt), "changed", G_CALLBACK(on_combobox_frombt_changed), (void *) V);

  // Edge type combobox
  GtkWidget *combobox_et = I->combobox_et;
  gtk_widget_set_tooltip_text(GTK_WIDGET(combobox_et), "How to draw edges");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_et), "none", "None");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_et), "straight", "Straight");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_et), "down", "Down");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_et), "winding", "Winding");
  g_signal_connect(G_OBJECT(combobox_et), "changed", G_CALLBACK(on_combobox_et_changed), (void *) V);

  // Edge affix toggle
  GtkWidget *togg_eaffix = I->togg_eaffix;
  gtk_widget_set_tooltip_text(GTK_WIDGET(togg_eaffix), "Add a short line segment btw edges & nodes");
  g_signal_connect(G_OBJECT(togg_eaffix), "toggled", G_CALLBACK(on_togg_eaffix_toggled), (void *) V);

  // Grid
  GtkWidget * grid = I->grid;
  
  // HBox 1
  GtkWidget * label_1 = gtk_label_new("              VIEW mode: ");
  gtk_widget_set_hexpand(GTK_WIDGET(label_1), TRUE);
  gtk_label_set_justify(GTK_LABEL(label_1), GTK_JUSTIFY_RIGHT);
  gtk_grid_attach(GTK_GRID(grid), label_1, 0, 0, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), I->combobox_lt, 1, 0, 1, 1);

  // HBox 2
  GtkWidget * label_2 = gtk_label_new("             Node color: ");
  gtk_grid_attach(GTK_GRID(grid), label_2, 0, 1, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), I->combobox_nc, 1, 1, 1, 1);

  // HBox 3
  GtkWidget * label_3 = gtk_label_new("        Scale-down type: ");
  gtk_grid_attach(GTK_GRID(grid), label_3, 0, 2, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), I->combobox_sdt, 1, 2, 1, 1);

  // HBox 4
  GtkWidget * label_4 = gtk_label_new("       Scale-down radix: ");
  gtk_grid_attach(GTK_GRID(grid), label_4, 0, 3, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), I->entry_radix, 1, 3, 1, 1);

  // HBox 5
  GtkWidget * label_5 = gtk_label_new("    From beginning time: ");
  gtk_grid_attach(GTK_GRID(grid), label_5, 0, 4, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), I->combobox_frombt, 1, 4, 1, 1);

  // HBox 6
  GtkWidget * label_6 = gtk_label_new("      Edge drawing type: ");
  gtk_grid_attach(GTK_GRID(grid), label_6, 0, 5, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), I->combobox_et, 1, 5, 1, 1);

  // HBox 7
  GtkWidget * label_7 = gtk_label_new("Affix btw edges & nodes: ");
  gtk_grid_attach(GTK_GRID(grid), label_7, 0, 6, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), I->togg_eaffix, 1, 6, 1, 1);


  // Set attribute values
  dv_view_interface_set_values(V, I);

  return I;
}

void dv_view_interface_destroy(dv_view_interface_t *I) {
  if (GTK_IS_WIDGET(I->togg_focused))
    gtk_widget_destroy(I->togg_focused);
  if (GTK_IS_WIDGET(I->combobox_lt))
    gtk_widget_destroy(I->combobox_lt);
  if (GTK_IS_WIDGET(I->entry_radix))
    gtk_widget_destroy(I->entry_radix);
  if (GTK_IS_WIDGET(I->toolbar))
    gtk_widget_destroy(I->toolbar);
  dv_free(I, sizeof(dv_view_interface_t));
}

void
dv_view_change_mainvp(dv_view_t * V, dv_viewport_t * VP) {
  if (V->mainVP == VP)
    return;
  V->mainVP = VP;
  if (!VP)
    return;
  int i = VP - CS->VP;
  if (!V->I[i])
    return;
  V->S->do_zoomfit = 1;
} 

void
dv_view_add_viewport(dv_view_t * V, dv_viewport_t * VP) {
  int idx = VP - CS->VP;
  if (V->I[idx] && V->I[idx]->VP == VP)
    return;
  dv_view_interface_t * itf = dv_view_interface_create_new(V, VP);
  V->I[idx] = itf;
  dv_viewport_add_interface(VP, itf);
  if (!V->mainVP)
    dv_view_change_mainvp(V, VP);
}

void
dv_view_remove_viewport(dv_view_t * V, dv_viewport_t * VP) {
  int idx = VP - CS->VP;
  if (!V->I[idx])
    return;
  dv_view_interface_t * itf = V->I[idx];
  V->I[idx] = NULL;
  dv_viewport_remove_interface(VP, itf);
  dv_view_interface_destroy(itf);
  if (V->mainVP == VP) {
    dv_viewport_t * new_vp = NULL;
    int i;
    for (i=0; i<CS->nVP; i++)
      if (V->I[i]) {
        new_vp = V->I[i]->VP;
        break;
      }
    dv_view_change_mainvp(V, new_vp);
  }
}

void
dv_view_switch_viewport(dv_view_t * V, dv_viewport_t * VP1, dv_viewport_t * VP2) {
  int sw = 0;
  fprintf(stderr, "mainvp:%ld, vp1:%ld\n", V->mainVP - CS->VP, VP1 - CS->VP);
  if (V->mainVP == VP1)
    sw = 1;
  dv_view_remove_viewport(V, VP1);
  dv_view_add_viewport(V, VP2);
  if (sw)
    dv_view_change_mainvp(V, VP2);
}

dv_view_interface_t * dv_view_get_interface_to_viewport(dv_view_t *V, dv_viewport_t *VP) {
  int idx = VP - CS->VP;
  dv_view_interface_t *I = V->I[idx];
  return I;
}

/*-----------------end of VIEW's functions-----------------*/



/*-----------------VIEWPORT's functions-----------------*/

dv_viewport_t *
dv_viewport_create_new() {
  if (CS->nVP >= DV_MAX_VIEWPORT)
    return NULL;
  dv_viewport_t * ret = &CS->VP[CS->nVP];
  CS->nVP++;
  return ret;
}

void
dv_viewport_init(dv_viewport_t * VP) {
  VP->split = 0;
  // Frame
  //char s[20];
  //sprintf(s, "Viewport %ld", VP - CS->VP);
  VP->frame = gtk_frame_new(NULL);
  g_object_ref(G_OBJECT(VP->frame));
  // Paned
  VP->paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  g_object_ref(G_OBJECT(VP->paned));
  VP->orientation = GTK_ORIENTATION_HORIZONTAL;
  VP->vp1 = VP->vp2 = NULL;
  // White color
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");
  // Box
  VP->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_object_ref(G_OBJECT(VP->box));
  gtk_container_add(GTK_CONTAINER(VP->frame), GTK_WIDGET(VP->box));
  // Drawing Area
  VP->darea = gtk_drawing_area_new();
  g_object_ref(G_OBJECT(VP->darea));
  gtk_box_pack_end(GTK_BOX(VP->box), VP->darea, TRUE, TRUE, 0);
  GtkWidget * darea = VP->darea;
  gtk_widget_override_background_color(GTK_WIDGET(darea), GTK_STATE_FLAG_NORMAL, white);
  g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), (void *) VP);
  gtk_widget_add_events(GTK_WIDGET(darea), GDK_SCROLL_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
  g_signal_connect(G_OBJECT(darea), "scroll-event", G_CALLBACK(on_scroll_event), (void *) VP);
  g_signal_connect(G_OBJECT(darea), "button-press-event", G_CALLBACK(on_button_event), (void *) VP);
  g_signal_connect(G_OBJECT(darea), "button-release-event", G_CALLBACK(on_button_event), (void *) VP);
  g_signal_connect(G_OBJECT(darea), "motion-notify-event", G_CALLBACK(on_motion_event), (void *) VP);
  g_signal_connect(G_OBJECT(darea), "configure-event", G_CALLBACK(on_darea_configure_event), (void *) VP);
  // I
  int i;
  for (i=0; i<DV_MAX_VIEW; i++)
    VP->I[i] = NULL;
  // vpw, vph
  VP->vpw = VP->vph = 0.0;
}

void dv_viewport_show(dv_viewport_t * vp);

void
dv_viewport_show_children(dv_viewport_t * vp) {
  dv_check(vp->split);
  dv_check(vp->paned);
  // Child 1
  if (vp->vp1) {
    dv_viewport_t * vp1 = vp->vp1;
    gtk_paned_pack1(GTK_PANED(vp->paned), vp1->frame, TRUE, TRUE);
    dv_viewport_show(vp1);
  }
  // Child 2
  if (vp->vp2) {
    dv_viewport_t * vp2 = vp->vp2;
    gtk_paned_pack2(GTK_PANED(vp->paned), vp2->frame, TRUE, TRUE);
    dv_viewport_show(vp2);
  }
}

void
dv_viewport_show(dv_viewport_t * VP) {
  GtkWidget * child = gtk_bin_get_child(GTK_BIN(VP->frame));
  if (VP->split) {
    if (child != VP->paned) {
      if (child) gtk_container_remove(GTK_CONTAINER(VP->frame), child);
      dv_check(VP->paned);
      gtk_container_add(GTK_CONTAINER(VP->frame), VP->paned);
      dv_viewport_show_children(VP);
    }
  } else {
    if (child != VP->box) {
      if (child) gtk_container_remove(GTK_CONTAINER(VP->frame), child);
      gtk_container_add(GTK_CONTAINER(VP->frame), VP->box);
    }
  }
}

void
dv_viewport_change_split(dv_viewport_t * VP, int new_split) {
  int old_split = VP->split;
  if (new_split == old_split) return;
  if (new_split) {
    // Check children's existence
    if (!VP->vp1) {
      VP->vp1 = dv_viewport_create_new();
      if (VP->vp1)
        dv_viewport_init(VP->vp1);
    }
    int i;
    for (i=0; i<CS->nV; i++)
      if (VP->I[i]) {
        dv_view_switch_viewport(&CS->V[i], VP, VP->vp1);
      }
    if (!VP->vp2) {
      VP->vp2 = dv_viewport_create_new();
      if (VP->vp2)
        dv_viewport_init(VP->vp2);
    }        
  } else {
    int i;
    for (i=0; i<CS->nV; i++)
      if (VP->vp1->I[i]) {
        dv_view_switch_viewport(&CS->V[i], VP->vp1, VP);
      }
  }
  VP->split = new_split;
  dv_viewport_show(VP);
}

void
dv_viewport_change_orientation(dv_viewport_t * vp, GtkOrientation o) {
  GtkWidget * child1 = NULL;
  GtkWidget * child2 = NULL;
  if (vp->paned) {
    child1 = gtk_paned_get_child1(GTK_PANED(vp->paned));
    if (child1)
      gtk_container_remove(GTK_CONTAINER(vp->paned), child1);
    child2 = gtk_paned_get_child2(GTK_PANED(vp->paned));
    if (child2)
      gtk_container_remove(GTK_CONTAINER(vp->paned), child2);
    gtk_widget_destroy(GTK_WIDGET(vp->paned));
  }
  vp->paned = gtk_paned_new(o);
  g_object_ref(vp->paned);
  vp->orientation = o;
  dv_viewport_show(vp);
}

void
dv_viewport_add_interface(dv_viewport_t * VP, dv_view_interface_t * I) {
  int idx = I->V - CS->V;
  dv_check(!VP->I[idx]);
  VP->I[idx] = I;
  // Reset box
  gtk_box_pack_start(GTK_BOX(VP->box), GTK_WIDGET(I->toolbar), FALSE, FALSE, 0);
}

void
dv_viewport_remove_interface(dv_viewport_t * VP, dv_view_interface_t * I) {
  int idx = I->V - CS->V;
  dv_check(VP->I[idx]);
  VP->I[idx] = NULL;
  // Reset box
  gtk_container_remove(GTK_CONTAINER(VP->box), GTK_WIDGET(I->toolbar));
}

dv_view_interface_t *
dv_viewport_get_interface_to_view(dv_viewport_t * VP, dv_view_t * v) {
  dv_check(VP);
  if (!v)
    return NULL;
  int idx = v - CS->V;
  dv_view_interface_t * i = VP->I[idx];
  return i;
}

static GtkWidget *
dv_viewport_create_frame(dv_viewport_t * VP) {
  char s[10];
  sprintf(s, "Viewport %ld", VP - CS->VP);
  GtkWidget * frame = gtk_frame_new(s);
  if (VP->split) {
    GtkWidget * paned = gtk_paned_new(VP->orientation);
    gtk_container_add(GTK_CONTAINER(frame), paned);
    if (VP->vp1)
      gtk_paned_pack1(GTK_PANED(paned), dv_viewport_create_frame(VP->vp1), TRUE, TRUE);
    if (VP->vp2)
      gtk_paned_pack2(GTK_PANED(paned), dv_viewport_create_frame(VP->vp2), TRUE, TRUE);
  }
  return frame;
}

static void dv_viewport_redraw_configure_box();
static void dv_alternate_menubar();

static gboolean
on_viewport_options_split_changed(GtkWidget * widget, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  int active = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  if (active == 0) {
    // No-split
    if (VP->split != 0)
      dv_viewport_change_split(VP, 0);
  } else {
    // Split
    if (VP->split != 1)
      dv_viewport_change_split(VP, 1);
  }

  // Redraw viewports
  gtk_widget_show_all(GTK_WIDGET(VP->frame));
  gtk_widget_queue_draw(GTK_WIDGET(VP->frame));
  // Redraw viewport configure dialog
  dv_viewport_redraw_configure_box();
  // Redraw menubar
  dv_alternate_menubar();
  
  return TRUE;
}

static gboolean
on_viewport_options_orientation_changed(GtkWidget * widget, gpointer user_data) {
  dv_viewport_t * vp = (dv_viewport_t *) user_data;
  int active = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  if (active == 0) {
    // Horizontal
    if (vp->orientation != GTK_ORIENTATION_HORIZONTAL)
      dv_viewport_change_orientation(vp, GTK_ORIENTATION_HORIZONTAL);
  } else {
    // Vertical
    if (vp->orientation != GTK_ORIENTATION_VERTICAL)
      dv_viewport_change_orientation(vp, GTK_ORIENTATION_VERTICAL);
  }

  // Redraw viewports
  gtk_widget_show_all(GTK_WIDGET(vp->frame));
  gtk_widget_queue_draw(GTK_WIDGET(vp->frame));
  // Redraw viewport configure dialog
  dv_viewport_redraw_configure_box();
  
  return TRUE;
}

static void
dv_viewport_update_configure_box() {
  // Box
  GtkWidget * box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  GList * list = gtk_container_get_children(GTK_CONTAINER(CS->box_viewport_configure));
  GtkWidget * child = (GtkWidget *) g_list_nth_data(list, 0);
  if (child)
    gtk_container_remove(GTK_CONTAINER(CS->box_viewport_configure), child);
  gtk_box_pack_start(GTK_BOX(CS->box_viewport_configure), box, TRUE, TRUE, 3);

  // Left
  GtkWidget * left = dv_viewport_create_frame(CS->VP);
  gtk_box_pack_start(GTK_BOX(box), left, TRUE, TRUE, 3);

  // Separator
  gtk_box_pack_start(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);

  // Right
  GtkWidget * right = gtk_scrolled_window_new(NULL, NULL);
  gtk_box_pack_start(GTK_BOX(box), right, FALSE, FALSE, 3);
  gtk_widget_set_size_request(GTK_WIDGET(right), 315, 0);
  GtkWidget * right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(right), right_box);
  int i;
  for (i=0; i<CS->nVP; i++) {
    char s[30];
    sprintf(s, "Viewport %d: ", i);
    GtkWidget * vp_frame = gtk_frame_new(NULL);
    gtk_box_pack_start(GTK_BOX(right_box), vp_frame, FALSE, FALSE, 3);
    GtkWidget * vp_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_container_add(GTK_CONTAINER(vp_frame), vp_hbox);

    // Label
    GtkWidget * label = gtk_label_new(s);
    gtk_box_pack_start(GTK_BOX(vp_hbox), label, FALSE, FALSE, 3);

    // Split
    GtkWidget * split_combobox = gtk_combo_box_text_new();
    gtk_box_pack_start(GTK_BOX(vp_hbox), split_combobox, TRUE, FALSE, 0);
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(split_combobox), "nosplit", "No split");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(split_combobox), "split", "Split");
    gtk_combo_box_set_active(GTK_COMBO_BOX(split_combobox), CS->VP[i].split);
    g_signal_connect(G_OBJECT(split_combobox), "changed", G_CALLBACK(on_viewport_options_split_changed), (void *) &CS->VP[i]);

    // Orientation
    GtkWidget * orient_combobox = gtk_combo_box_text_new();
    gtk_box_pack_start(GTK_BOX(vp_hbox), orient_combobox, TRUE, FALSE, 0);
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(orient_combobox), "horizontal", "Horizontally");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(orient_combobox), "vertical", "Vertically");
    gtk_combo_box_set_active(GTK_COMBO_BOX(orient_combobox), CS->VP[i].orientation);
    g_signal_connect(G_OBJECT(orient_combobox), "changed", G_CALLBACK(on_viewport_options_orientation_changed), (void *) &CS->VP[i]);

  }
}

static void
dv_viewport_redraw_configure_box() {
  dv_viewport_update_configure_box();
  gtk_widget_show_all(CS->box_viewport_configure);
  gtk_widget_queue_draw(GTK_WIDGET(CS->box_viewport_configure));
}

/*-----------------end of VIEWPORT's functions-----------------*/



/*-----------------Menubar functions-----------------*/

static GtkWidget * dv_create_menubar();

static void
dv_alternate_menubar() {
  gtk_container_remove(GTK_CONTAINER(CS->vbox0), CS->menubar);
  CS->menubar = dv_create_menubar();
  gtk_box_pack_start(GTK_BOX(CS->vbox0), CS->menubar, FALSE, FALSE, 0);
  gtk_widget_show_all(CS->menubar);
  gtk_widget_queue_draw(GTK_WIDGET(CS->menubar));
}

static void
on_view_add_new(GtkMenuItem * menuitem, gpointer user_data) {
  // Create new view
  dv_dag_t * D = (dv_dag_t *) user_data;
  dv_view_t * V = dv_view_create_new_with_dag(D);
  if (V) {
    //dv_do_expanding_one(V);
    dv_view_layout(V);
    // Alternate menubar
    dv_alternate_menubar();
  }
}

static void
on_dag_add_new(GtkMenuItem *menuitem, gpointer user_data) {
  // Create new view
  dv_pidag_t * P = (dv_pidag_t *) user_data;
  dv_dag_t * D = dv_dag_create_new_with_pidag(P);
  if (D) {
    // Alternate menubar
    dv_alternate_menubar();
  }
}

static void on_menu_item_view_samples_clicked(GtkToolButton *toolbtn, gpointer user_data) {
  dv_pidag_t * P = CS->activeV->D->P;
  CS->btviewer->P = P;

  // Build dialog
  GtkWidget * dialog = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog), "View Backtrace Samples");
  gtk_window_set_default_size(GTK_WINDOW(dialog), 800, 700);
  GtkWidget * dialog_vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_box_set_spacing(GTK_BOX(dialog_vbox), 5);

  // HBox 1: dag file
  GtkWidget * hbox1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox1, FALSE, FALSE, 0);
  GtkWidget * hbox1_label = gtk_label_new("DAG File:");
  gtk_box_pack_start(GTK_BOX(hbox1), hbox1_label, FALSE, FALSE, 0);
  GtkWidget * hbox1_filename = CS->btviewer->label_dag_file_name;
  gtk_box_pack_start(GTK_BOX(hbox1), hbox1_filename, TRUE, TRUE, 0);
  gtk_label_set_text(GTK_LABEL(hbox1_filename), P->fn);
  
  // HBox 2: bt file
  GtkWidget * hbox2 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox2, FALSE, FALSE, 0);
  GtkWidget * hbox2_label = gtk_label_new("        BT File:");
  gtk_box_pack_start(GTK_BOX(hbox2), hbox2_label, FALSE, FALSE, 0);
  GtkWidget * hbox2_filename = CS->btviewer->entry_bt_file_name;
  gtk_box_pack_start(GTK_BOX(hbox2), hbox2_filename, TRUE, TRUE, 0);
  GtkWidget * hbox2_btn = gtk_button_new_with_label("Choose file");
  gtk_box_pack_start(GTK_BOX(hbox2), hbox2_btn, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(hbox2_btn), "clicked", G_CALLBACK(on_btn_choose_bt_file_clicked), (void *) CS->btviewer);
  
  // HBox 3: binary file
  GtkWidget * hbox3 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox3, FALSE, FALSE, 0);
  GtkWidget * hbox3_label = gtk_label_new("Binary File:");
  gtk_box_pack_start(GTK_BOX(hbox3), hbox3_label, FALSE, FALSE, 0);
  GtkWidget * hbox3_filename = CS->btviewer->entry_binary_file_name;
  gtk_box_pack_start(GTK_BOX(hbox3), hbox3_filename, TRUE, TRUE, 0);
  GtkWidget * hbox3_btn = gtk_button_new_with_label("Choose file");
  gtk_box_pack_start(GTK_BOX(hbox3), hbox3_btn, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(hbox3_btn), "clicked", G_CALLBACK(on_btn_choose_binary_file_clicked), (void *) CS->btviewer);

  // HBox 4: node ID
  GtkWidget * hbox4 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox4, FALSE, FALSE, 0);
  GtkWidget * hbox4_label = gtk_label_new("Node ID:");
  gtk_box_pack_start(GTK_BOX(hbox4), hbox4_label, FALSE, FALSE, 0);
  GtkWidget * hbox4_nodeid = CS->btviewer->entry_node_id;
  gtk_box_pack_start(GTK_BOX(hbox4), hbox4_nodeid, FALSE, FALSE, 0);
  gtk_entry_set_max_length(GTK_ENTRY(hbox4_nodeid), 10);
  GtkWidget * hbox4_btn = gtk_button_new_with_label("Find node");
  gtk_box_pack_start(GTK_BOX(hbox4), hbox4_btn, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(hbox4_btn), "clicked", G_CALLBACK(on_btn_find_node_clicked), (void *) CS->btviewer);

  // HBox 5: worker & time interval
  GtkWidget * hbox5 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox5, FALSE, FALSE, 0);
  GtkWidget * hbox5_label = gtk_label_new("On worker");
  gtk_box_pack_start(GTK_BOX(hbox5), hbox5_label, FALSE, FALSE, 0);
  GtkWidget * hbox5_combo = CS->btviewer->combobox_worker;
  gtk_box_pack_start(GTK_BOX(hbox5), hbox5_combo, FALSE, FALSE, 0);
  gtk_combo_box_text_remove_all(GTK_COMBO_BOX_TEXT(hbox5_combo));
  int i;
  char str[10];
  for (i=0; i<P->num_workers; i++) {
    sprintf(str, "%d", i);
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(hbox5_combo), str, str);
  }
  GtkWidget * hbox5_label2 = gtk_label_new("from clock");
  gtk_box_pack_start(GTK_BOX(hbox5), hbox5_label2, FALSE, FALSE, 0);
  GtkWidget * hbox5_from = CS->btviewer->entry_time_from;
  gtk_box_pack_start(GTK_BOX(hbox5), hbox5_from, TRUE, TRUE, 0);
  GtkWidget * hbox5_label3 = gtk_label_new("to clock");
  gtk_box_pack_start(GTK_BOX(hbox5), hbox5_label3, FALSE, FALSE, 0);
  GtkWidget * hbox5_to = CS->btviewer->entry_time_to;
  gtk_box_pack_start(GTK_BOX(hbox5), hbox5_to, TRUE, TRUE, 0);

  // HBox 6: run button
  GtkWidget * hbox6 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(dialog_vbox), hbox6, FALSE, FALSE, 0);
  GtkWidget * hbox6_btn = gtk_button_new_with_label("Run");
  gtk_box_pack_end(GTK_BOX(hbox6), hbox6_btn, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(hbox6_btn), "clicked", G_CALLBACK(on_btn_run_view_bt_samples_clicked), (void *) CS->btviewer);

  // Text view
  GtkWidget *scrolledwindow = gtk_scrolled_window_new(NULL, NULL);
  gtk_box_pack_end(GTK_BOX(dialog_vbox), scrolledwindow, TRUE, TRUE, 0);
  gtk_container_add(GTK_CONTAINER(scrolledwindow), CS->btviewer->text_view);

  // Run
  gtk_widget_show_all(dialog_vbox);
  gtk_dialog_run(GTK_DIALOG(dialog));

  // Destroy
  gtk_container_remove(GTK_CONTAINER(hbox1), CS->btviewer->label_dag_file_name);
  gtk_container_remove(GTK_CONTAINER(hbox2), CS->btviewer->entry_bt_file_name);
  gtk_container_remove(GTK_CONTAINER(hbox3), CS->btviewer->entry_binary_file_name);
  gtk_container_remove(GTK_CONTAINER(hbox4), CS->btviewer->entry_node_id);
  gtk_container_remove(GTK_CONTAINER(hbox5), CS->btviewer->combobox_worker);
  gtk_container_remove(GTK_CONTAINER(hbox5), CS->btviewer->entry_time_from);
  gtk_container_remove(GTK_CONTAINER(hbox5), CS->btviewer->entry_time_to);
  gtk_container_remove(GTK_CONTAINER(scrolledwindow), CS->btviewer->text_view);
  gtk_widget_destroy(dialog);
}

static void
on_help_hotkeys_clicked(GtkToolButton *toolbtn, gpointer user_data) {
  // Build dialog
  GtkWidget * dialog = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog), "Hotkeys");
  gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 170);
  GtkWidget * dialog_vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  // Label
  GtkWidget * label = gtk_label_new("\n"
                                    "tab : switch control between VIEWs\n"
                                    "Ctrl + 1 : dag view\n"
                                    "Ctrl + 2 : dagbox view\n"
                                    "Ctrl + 3 : timeline view\n"
                                    "Ctrl + 4 : timeline2 (horizontal) view\n"
                                    "x : expand\n"
                                    "c : collapse\n"
                                    "h : horizontal fit\n"
                                    "v : vertical fit\n"
                                    "Alt + key : access menu\n"
                                    "Arrow keys : move around\n");
  gtk_box_pack_start(GTK_BOX(dialog_vbox), label, TRUE, FALSE, 0);

  // Run
  gtk_widget_show_all(dialog_vbox);
  gtk_dialog_run(GTK_DIALOG(dialog));

  // Destroy
  gtk_widget_destroy(dialog);  
}

static void
dv_viewport_export_to_png(dv_viewport_t * VP) {
  cairo_surface_t * surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, VP->vpw, VP->vph);
  cairo_t * cr = cairo_create(surface);
  // Whiten background
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");
  cairo_set_source_rgba(cr, white->red, white->green, white->blue, white->alpha);
  cairo_paint(cr);
  // Draw viewport
  dv_viewport_draw(VP, cr);
  // Write to file
  cairo_surface_write_to_png(surface, "00dv.png");
  fprintf(stdout, "Exported viewport %ld to 00dv.png\n", VP - CS->VP);
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
}

static void
dv_viewport_export_to_eps(dv_viewport_t * VP) {
  cairo_surface_t * surface = cairo_ps_surface_create("00dv.eps", VP->vpw, VP->vph);
  cairo_ps_surface_set_eps(surface, TRUE);
  cairo_t * cr = cairo_create(surface);
  // Whiten background
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");
  cairo_set_source_rgba(cr, white->red, white->green, white->blue, white->alpha);
  cairo_paint(cr);
  // Draw viewport
  dv_viewport_draw(VP, cr);
  // Finish
  fprintf(stdout, "Exported viewport %ld to 00dv.eps\n", VP - CS->VP);
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
}

static void
on_help_export_clicked(GtkToolButton *toolbtn, gpointer user_data) {
  dv_view_t * V = CS->activeV;
  if (!V) {
    fprintf(stderr, "Warning: there is no active V to export.\n");
    return;
  }
  dv_viewport_export_to_png(V->mainVP);
  dv_viewport_export_to_eps(V->mainVP);
  return;  
}

static void
on_viewport_select_view(GtkCheckMenuItem * checkmenuitem, gpointer user_data) {
  dv_viewport_t * vp = (dv_viewport_t *) user_data;
  const gchar * label = gtk_menu_item_get_label(GTK_MENU_ITEM(checkmenuitem));
  gboolean active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(checkmenuitem));
  // Find V
  char s[10];
  int i;
  for (i=0; i<CS->nV; i++) {
    sprintf(s, "VIEW _%d", i);
    if (strcmp(s, label) == 0)
      break;
  }
  if (i >= CS->nV) {
    fprintf(stderr, "on_viewport_select_view: could not find view (%s)\n", label);
    return;
  }
  dv_view_t * v = &CS->V[i];
  // Actions
  if (active) {
    dv_view_add_viewport(v, vp);
  } else {
    dv_view_remove_viewport(v, vp);
  }
  gtk_widget_show_all(GTK_WIDGET(vp->frame));
  gtk_widget_queue_draw(GTK_WIDGET(vp->frame));
}

static void
on_viewport_configure_clicked(GtkMenuItem * menuitem, gpointer user_data) {
  GtkWidget * dialog = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog), "Configure Viewports");
  gtk_window_set_default_size(GTK_WINDOW(dialog), 800, 400);
  
  GtkWidget * box = CS->box_viewport_configure;
  if (!box) {
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    g_object_ref(box);
    CS->box_viewport_configure = box;
  }

  // Update dialog's content
  dv_viewport_update_configure_box();

  // GUI
  GtkWidget * vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_box_pack_start(GTK_BOX(vbox), box, TRUE, TRUE, 0);

  // Run
  gtk_widget_show_all(dialog);
  gtk_dialog_run(GTK_DIALOG(dialog));

  // Destroy
  gtk_container_remove(GTK_CONTAINER(vbox), box);
  gtk_widget_destroy(dialog);
}
/*-----------------end of Menubar functions-----------------*/



/*-----------------Main begins-----------------*/

static GtkWidget *
dv_create_menubar() {
  // Menu Bar
  GtkWidget * menubar = gtk_menu_bar_new();
  
  // submenu viewports
  GtkWidget * viewports = gtk_menu_item_new_with_mnemonic("Viewp_orts");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), viewports);
  GtkWidget * viewports_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(viewports), viewports_menu);
  GtkWidget * viewport, * viewport_menu;
  viewport = gtk_menu_item_new_with_mnemonic("_Configure");
  gtk_menu_shell_append(GTK_MENU_SHELL(viewports_menu), viewport);
  g_signal_connect(G_OBJECT(viewport), "activate", G_CALLBACK(on_viewport_configure_clicked), NULL);
  GSList * group;
  GtkWidget * item;
  char s[100];
  int i, j;
  for (i=0; i<CS->nVP; i++) {
    if (!CS->VP[i].split) {
      sprintf(s, "Viewport _%d", i);
      viewport = gtk_menu_item_new_with_mnemonic(s);
      gtk_menu_shell_append(GTK_MENU_SHELL(viewports_menu), viewport);
      viewport_menu = gtk_menu_new();
      gtk_menu_item_set_submenu(GTK_MENU_ITEM(viewport), viewport_menu);
      for (j=0; j<CS->nV; j++) {
        sprintf(s, "VIEW _%d", j);
        item = gtk_check_menu_item_new_with_mnemonic(s);
        gtk_menu_shell_append(GTK_MENU_SHELL(viewport_menu), item);
        g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(on_viewport_select_view), (void *) &CS->VP[i]);
        if (CS->VP[i].I[j])
          gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
      }
    }
  }
  
  // submenu views
  GtkWidget * views = gtk_menu_item_new_with_mnemonic("_VIEWs");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), views);
  GtkWidget * views_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(views), views_menu);
  GtkWidget * view, * view_menu;
  view = gtk_menu_item_new_with_label("Add new VIEW");
  gtk_menu_shell_append(GTK_MENU_SHELL(views_menu), view);
  view_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(view), view_menu);
  for (j=0; j<CS->nD; j++) {
    sprintf(s, "for DAG %d", j);
    item = gtk_menu_item_new_with_label(s);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), item);
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_view_add_new), (void *) &CS->D[j]);
  }    
  for (i=0; i<CS->nV; i++) {
    group = NULL;
    sprintf(s, "VIEW %d", i);
    view = gtk_menu_item_new_with_label(s);
    gtk_menu_shell_append(GTK_MENU_SHELL(views_menu), view);
    view_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(view), view_menu);
    for (j=0; j<CS->nD; j++) {
      sprintf(s, "DAG %d", j);
      item = gtk_radio_menu_item_new_with_label(group, s);
      gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), item);
      group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
      if (j == (CS->V[i].D - CS->D))
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
    }    
  }
  // submenu DAGs
  GtkWidget * dags = gtk_menu_item_new_with_mnemonic("_DAGs");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), dags);
  GtkWidget * dags_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(dags), dags_menu);
  GtkWidget * dag, * dag_menu;
  dag = gtk_menu_item_new_with_label("Add new DAG");
  gtk_menu_shell_append(GTK_MENU_SHELL(dags_menu), dag);
  dag_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(dag), dag_menu);
  for (j=0; j<CS->nP; j++) {
    sprintf(s, "for PIDAG %d", j);
    item = gtk_menu_item_new_with_label(s);
    gtk_menu_shell_append(GTK_MENU_SHELL(dag_menu), item);
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_dag_add_new), (void *) &CS->P[j]);
  }
  for (i=0; i<CS->nD; i++) {
    group = NULL;
    sprintf(s, "DAG %d", i);
    dag = gtk_menu_item_new_with_label(s);
    gtk_menu_shell_append(GTK_MENU_SHELL(dags_menu), dag);
    dag_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(dag), dag_menu);
    for (j=0; j<CS->nP; j++) {
      sprintf(s, "PIDAG %d: [%0.0lfMB,%ld] %s", j, ((double) CS->P[j].stat->st_size) / (1024.0 * 1024.0), CS->P[j].n, CS->P[j].fn);
      item = gtk_radio_menu_item_new_with_label(group, s);
      gtk_menu_shell_append(GTK_MENU_SHELL(dag_menu), item);
      group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
      if (j == (CS->D[i].P - CS->P))
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
    }    
  }
  // submenu PIDAGs
  GtkWidget * pidags = gtk_menu_item_new_with_mnemonic("_PIDAGs");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), pidags);
  GtkWidget * pidags_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(pidags), pidags_menu);
  GtkWidget * pidag;
  //pidag = gtk_menu_item_new_with_label("Add new dag file");
  //gtk_menu_shell_append(GTK_MENU_SHELL(pidags_menu), pidag);
  for (i=0; i<CS->nP; i++) {
    sprintf(s, "PIDAG %d: [%0.0lfMB,%ld] %s", i, ((double) CS->P[i].stat->st_size) / (1024.0 * 1024.0), CS->P[i].n, CS->P[i].fn);
    pidag = gtk_menu_item_new_with_label(s);
    gtk_menu_shell_append(GTK_MENU_SHELL(pidags_menu), pidag);
  }
  
  // submenu Sample Backtrace
  GtkWidget * samplebt = gtk_menu_item_new_with_mnemonic("_Samples");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), samplebt);
  GtkWidget * samplebt_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(samplebt), samplebt_menu);
  GtkWidget * samplebt_open = gtk_menu_item_new_with_mnemonic("_View Backtrace Samples");
  gtk_menu_shell_append(GTK_MENU_SHELL(samplebt_menu), samplebt_open);
  g_signal_connect(G_OBJECT(samplebt_open), "activate", G_CALLBACK(on_menu_item_view_samples_clicked), (void *) 0);

  // submenu help
  GtkWidget * help = gtk_menu_item_new_with_mnemonic("_Help");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), help);
  GtkWidget * help_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(help), help_menu);  
  GtkWidget * hotkeys = gtk_menu_item_new_with_mnemonic("Hot_keys");
  gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), hotkeys);
  g_signal_connect(G_OBJECT(hotkeys), "activate", G_CALLBACK(on_help_hotkeys_clicked), NULL);
  GtkWidget * export = gtk_menu_item_new_with_mnemonic("E_xport");
  gtk_menu_shell_append(GTK_MENU_SHELL(help_menu), export);
  g_signal_connect(G_OBJECT(export), "activate", G_CALLBACK(on_help_export_clicked), NULL);
  
  return menubar;
}

static int
open_gui(int argc, char * argv[]) {
  // Initialize
  CS->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  CS->vbox0 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  CS->menubar = dv_create_menubar();

  // Window
  GtkWidget * window = CS->window;
  //gtk_window_fullscreen(GTK_WINDOW(window));
  //gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
  gtk_window_maximize(GTK_WINDOW(window));
  //gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_title(GTK_WINDOW(window), "DAG Visualizer");
  g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(G_OBJECT(CS->window), "key-press-event", G_CALLBACK(on_window_key_event), NULL);

  // Set icon
  /*
  char * icon_filename = "smile_icon.png";
  GError * error = 0; 
  GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(icon_filename, &error);
  if (!pixbuf) {
    fprintf(stderr, "Cannot set icon %s: %s\n", icon_filename, error->message);
  } else {
    gtk_window_set_icon(GTK_WINDOW(window), pixbuf);
  }
  */

  // vbox0
  GtkWidget * vbox0 = CS->vbox0;
  gtk_container_add(GTK_CONTAINER(window), vbox0);

  // menubar
  GtkWidget * menubar = CS->menubar;
  gtk_box_pack_start(GTK_BOX(vbox0), menubar, FALSE, FALSE, 0);

  // viewport
  dv_viewport_t * vp0 = CS->VP;
  gtk_box_pack_end(GTK_BOX(vbox0), vp0->frame, TRUE, TRUE, 0);
  dv_viewport_show(vp0);
  //GtkWidget *hbox = CS->hbox;
  //gtk_box_pack_end(GTK_BOX(vbox0), hbox, TRUE, TRUE, 0);
  //gtk_box_set_homogeneous(GTK_BOX(hbox), TRUE);
  /*
  int i;
  for (i=0; i<CS->nVP; i++) {
    dv_viewport_t *VP = &CS->VP[i];
    if (!VP->hide)
      gtk_container_add(GTK_CONTAINER(hbox), VP->box);
  }
  */

  // Run main loop
  gtk_widget_show_all(window);
  gtk_main();

  return 1;
}

static void dv_alarm_set() {
  alarm(3);
}

void dv_signal_handler(int signo) {
  if (signo == SIGALRM) {
    fprintf(stderr, "received SIGALRM\n");
    dv_get_callpath_by_libunwind();
    dv_get_callpath_by_backtrace();
  } else
    fprintf(stderr, "received unknown signal\n");
  dv_alarm_set();
}

static void dv_alarm_init() {
  if (signal(SIGALRM, dv_signal_handler) == SIG_ERR)
    fprintf(stderr, "cannot catch SIGALRM\n");
  else
    dv_alarm_set();
}

int
main(int argc, char * argv[]) {
  /* General initialization */
  gtk_init(&argc, &argv);
  dv_global_state_init(CS);
  //dv_get_env();
  //if (argc > 1)  print_dag_file(argv[1]);
  //dv_alarm_init();
  
  /* PIDAG initialization */
  int i;
  if (argc > 1) {
    for (i=1; i<argc; i++)
      dv_pidag_read_new_file(argv[i]);
  }
  
  /* Viewport initialization */
  dv_viewport_t * VP = dv_viewport_create_new();
  dv_viewport_init(VP);

  /* DAG -> VIEW <- Viewport initialization */
  dv_dag_t * D;
  dv_view_t * V;
  for (i=0; i<CS->nP; i++) {
    D = dv_dag_create_new_with_pidag(&CS->P[i]);
    //print_dvdag(D);
    V = dv_view_create_new_with_dag(D);
  }
  V = CS->V;
  dv_view_add_viewport(V, VP);
  //dv_view_change_lt(V, 4);
  /*
  for (i=0; i<1; i++)
    dv_do_expanding_one(V);
  */
  dv_do_set_focused_view(CS->V, 1);
  
  /* Open GUI */
  return open_gui(argc, argv);
}

/*-----------------Main ends-------------------*/
