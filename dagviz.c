#include "dagviz.h"

dv_global_state_t  CS[1];

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
  int i;
  for (i=0; i<DV_MAX_DAG_FILE; i++)
    CS->Pfn[i] = NULL;
  CS->nP = 0;
  CS->nD = 0;
  CS->nV = 0;
  CS->FL = NULL;
  CS->window = NULL;
  CS->activeV = NULL;
  CS->err = DV_OK;
  for (i=0; i<DV_NUM_COLOR_POOLS; i++)
    CS->CP_sizes[i] = 0;
}

void dv_global_state_set_active_view(dv_view_t *V) {
  CS->activeV = V;
}

/*-----------------end of Global State-----------------*/



/*--------Interactive processing functions------------*/

void dv_queue_draw(dv_view_t *V) {
  gtk_widget_queue_draw(V->darea);
}

static void dv_do_zooming(dv_view_t *V, double zoom_ratio, double posx, double posy)
{
  dv_dag_t *D = V->D;
  posx -= D->basex + D->x;
  posy -= D->basey + D->y;
  double deltax = posx / D->zoom_ratio * zoom_ratio - posx;
  double deltay = posy / D->zoom_ratio * zoom_ratio - posy;
  D->x -= deltax;
  D->y -= deltay;
  D->zoom_ratio = zoom_ratio;
  dv_queue_draw(V);
}

static void dv_get_zoomfit_hoz_ratio(dv_view_t *V, double w, double h, double *pz, double *px, double *py) {
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  double zoom_ratio = 1.0;
  double x = 0.0;
  double y = 0.0;
  double d1, d2;
  double lw, rw;
  if (S->lt == 0) {
    // Grid-based
    lw = D->rt->vl->lc * DV_HDIS;
    rw = D->rt->vl->rc * DV_HDIS;
    d1 = lw + rw;
    d2 = w - 2 * (DV_ZOOM_TO_FIT_MARGIN + DV_RADIUS);
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    x -= (rw - lw) * 0.5 * zoom_ratio;
  } else if (S->lt == 1) {
    // Bounding box
    d1 = D->rt->lw + D->rt->rw;
    d2 = w - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    x -= (D->rt->rw - D->rt->lw) * 0.5 * zoom_ratio;
  } else if (S->lt == 2) {
    // Timeline
    d1 = 2*DV_RADIUS + (D->P->num_workers - 1) * (2*DV_RADIUS + DV_HDIS);
    d2 = w - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
  } else
    dv_check(0);
  *pz = zoom_ratio;
  *px = x;
  *py = y;
}

static void dv_get_zoomfit_ver_ratio(dv_view_t *V, double w, double h, double *pz, double *px, double *py) {
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  double zoom_ratio = 1.0;
  double x = 0.0;
  double y = 0.0;
  double d1, d2;
  double lw, rw;
  if (S->lt == 0) {
    // Grid-based
    d1 = D->rt->dc * DV_VDIS;
    d2 = h - 2 * (DV_ZOOM_TO_FIT_MARGIN + DV_RADIUS);
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    lw = D->rt->vl->lc * DV_HDIS;
    rw = D->rt->vl->rc * DV_HDIS;
    x -= (rw - lw) * 0.5 * zoom_ratio;
  } else if (S->lt == 1) {
    // Bounding box
    d1 = D->rt->dw;
    d2 = h - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;    
    x -= (D->rt->rw - D->rt->lw) * 0.5 * zoom_ratio;
  } else if (S->lt == 2) {
    // Timeline
    d1 = 10 + D->rt->dw;
    d2 = h - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    double lrw = 2*DV_RADIUS + (D->P->num_workers - 1) * (2*DV_RADIUS + DV_HDIS);
    x += (w - lrw * zoom_ratio) * 0.5;
  } else
    dv_check(0);
  *pz = zoom_ratio;
  *px = x;
  *py = y;
}

static void dv_do_zoomfit_hoz(dv_view_t *V) {
  //dv_dag_t *D = V->D;
  //dv_view_status_t *S = V->S;
  V->D->x = V->D->y = 0.0;
  dv_get_zoomfit_hoz_ratio(V, V->S->vpw, V->S->vph, &V->D->zoom_ratio, &V->D->x, &V->D->y);
  dv_queue_draw(V);
}

static void dv_do_zoomfit_ver(dv_view_t *V) {
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  dv_get_zoomfit_ver_ratio(V, S->vpw, S->vph, &D->zoom_ratio, &D->x, &D->y);
  dv_queue_draw(V);
}

static void dv_do_changing_lt(dv_view_t *V, int new_lt) {
  dv_view_status_t *S = V->S;
  int old_lt = S->lt;
  S->lt = new_lt;
  gtk_combo_box_set_active(GTK_COMBO_BOX(V->combobox_lt), new_lt);
  dv_view_layout(V);
  if (S->lt != old_lt)
    dv_do_zoomfit_ver(V);
}

static void dv_do_drawing(dv_view_t *V, cairo_t *cr)
{
  // First time only
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  if (D->init) {
    dv_get_zoomfit_hoz_ratio(V, S->vpw, S->vph, &D->zoom_ratio, &D->x, &D->y);
    D->init = 0;
  }
  // Draw graph
  cairo_save(cr);
  cairo_translate(cr, D->basex + D->x, D->basey + D->y);
  cairo_scale(cr, D->zoom_ratio, D->zoom_ratio);
  dv_view_draw(V, cr);
  cairo_restore(cr);

  // Draw status line
  dv_view_draw_status(V, cr);
}


static void dv_do_expanding_one_1(dv_view_t *V, dv_dag_node_t *node) {
  if (!dv_is_inner_loaded(node))
    if (dv_dag_build_node_inner(V->D, node) != DV_OK) return;
  dv_view_status_t *S = V->S;
  switch (S->lt) {
  case 0:
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
  case 1:
  case 2:
    // set expanded
    dv_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKED);
    break;
  default:
    dv_check(0);
  }
}

static void dv_do_expanding_one_r(dv_view_t *V, dv_dag_node_t *node) {
  if (!dv_is_set(node))
    dv_dag_node_set(V->D, node);
  if (dv_is_union(node)) {
    if ((!dv_is_inner_loaded(node) || dv_is_shrinked(node) || dv_is_shrinking(node))
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
  dv_llist_iterate_init(node->links);
  dv_dag_node_t *u;
  while (u = (dv_dag_node_t *) dv_llist_iterate_next(node->links)) {
    dv_do_expanding_one_r(V, u);
  }
}

static void dv_do_expanding_one(dv_view_t *V) {
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  dv_do_expanding_one_r(V, D->rt);
  if (!S->a->on) {
    dv_view_layout(V);
    dv_queue_draw(V);
  }
}


static void dv_do_collapsing_one_1(dv_view_t *V, dv_dag_node_t *node) {
  dv_view_status_t *S = V->S;
  switch (S->lt) {
  case 0:
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
  case 1:
  case 2:
    // set shrinked
    dv_node_flag_set(node->f, DV_NODE_FLAG_SHRINKED);
    break;
  default:
    dv_check(0);
  }
}

static void dv_do_collapsing_one_r(dv_view_t *V, dv_dag_node_t *node) {
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
      dv_llist_iterate_init(x->links);
      dv_dag_node_t *xx;
      while (xx = (dv_dag_node_t *) dv_llist_iterate_next(x->links)) {
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
  dv_llist_iterate_init(node->links);
  dv_dag_node_t *u;
  while (u = (dv_dag_node_t *) dv_llist_iterate_next(node->links)) {
    dv_do_collapsing_one_r(V, u);
  }
}

static void dv_do_collapsing_one(dv_view_t *V) {
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  dv_do_collapsing_one_r(V, D->rt);
  if (!S->a->on) {
    dv_view_layout(V);
    dv_queue_draw(V);
  }
}

static dv_dag_node_t *dv_do_finding_clicked_node_1(dv_view_t *V, double x, double y, dv_dag_node_t *node) {
  dv_dag_node_t * ret = NULL;
  double vc, hc;
  switch (V->S->lt) {
  case 0:
    // grid-like layout
    vc = node->vl->c;
    hc = node->c;
    if (vc - DV_RADIUS < x && x < vc + DV_RADIUS
        && hc - DV_RADIUS < y && y < hc + DV_RADIUS) {
      ret = node;
    }
    break;
  case 1:
  case 2:
    // bbox/timeline layouts
    if (node->x - node->lw < x && x < node->x + node->rw
        && node->y < y && y < node->y + node->dw) {
      ret = node;
    }
    break;
  default:
    dv_check(0);
  }
  return ret;
}

static dv_dag_node_t *dv_do_finding_clicked_node_r(dv_view_t *V, double x, double y, dv_dag_node_t *node) {
  dv_dag_node_t *ret;
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
  dv_llist_iterate_init(node->links);
  dv_dag_node_t *u;
  while (u = (dv_dag_node_t *) dv_llist_iterate_next(node->links)) {
    ret = dv_do_finding_clicked_node_r(V, x, y, u);
    if (ret)
      return ret;
  }
  return 0;
}

static dv_dag_node_t *dv_do_finding_clicked_node(dv_view_t *V, double x, double y) {
  return dv_do_finding_clicked_node_r(V, x, y, V->D->rt);
}

static void dv_set_entry_radix_text(dv_view_t *V) {
  dv_view_status_t *S = V->S;
  char str[DV_ENTRY_RADIX_MAX_LENGTH];
  double radix;
  if (S->sdt == 0)
    radix = S->log_radix;
  else if (S->sdt == 1)
    radix = S->power_radix;
  else if (S->sdt == 2)
    radix = S->linear_radix;
  else
    dv_check(0);
  sprintf(str, "%0.3lf", radix);
  gtk_entry_set_width_chars(GTK_ENTRY(V->entry_radix), strlen(str));
  gtk_entry_set_text(GTK_ENTRY(V->entry_radix), str);
}

static void dv_get_entry_radix_text(dv_view_t *V) {
  dv_view_status_t *S = V->S;
  double radix = atof(gtk_entry_get_text(GTK_ENTRY(V->entry_radix)));
  if (S->sdt == 0)
    S->log_radix = radix;
  else if (S->sdt == 1)
    S->power_radix = radix;
  else if (S->sdt == 2)
    S->linear_radix = radix;
  else
    dv_check(0);
  dv_view_layout(V);
  dv_queue_draw(V);
}

/*--------end of Interactive processing functions------------*/


/*-----------------VIEW's functions-----------------*/

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  dv_view_t *V = (dv_view_t *) user_data;
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  if (S->lt == 0) {
    D->basex = 0.5 * S->vpw;
    D->basey = DV_ZOOM_TO_FIT_MARGIN + DV_RADIUS;
  } else if (S->lt == 1) {
    //G->basex = 0.5 * S->vpw - 0.5 * (G->rt->rw - G->rt->lw);
    D->basex = 0.5 * S->vpw;
    D->basey = DV_ZOOM_TO_FIT_MARGIN;
  } else if (S->lt == 2) {
    D->basex = DV_ZOOM_TO_FIT_MARGIN;
    D->basey = DV_ZOOM_TO_FIT_MARGIN;
  } else
    dv_check(0);
  // Draw
  dv_do_drawing(V, cr);
  return FALSE;
}

static void on_btn_zoomfit_hoz_clicked(GtkToolButton *toolbtn, gpointer user_data)
{
  dv_view_t *V = (dv_view_t *) user_data;
  dv_do_zoomfit_hoz(V);
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

static gboolean on_scroll_event(GtkWidget *widget, GdkEventScroll *event, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  dv_dag_t *D = V->D;
  if (event->direction == GDK_SCROLL_UP) {
    dv_do_zooming(V, D->zoom_ratio * DV_ZOOM_INCREMENT, event->x, event->y);
  } else if (event->direction == GDK_SCROLL_DOWN) {
    dv_do_zooming(V, D->zoom_ratio / DV_ZOOM_INCREMENT, event->x, event->y);
  }
  return TRUE;
}

static gboolean on_button_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
  dv_view_t *V = (dv_view_t *) user_data;
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
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
      double ox = (event->x - D->basex - D->x) / D->zoom_ratio;
      double oy = (event->y - D->basey - D->y) / D->zoom_ratio;
      dv_dag_node_t *node = dv_do_finding_clicked_node(V, ox, oy);
      if (node) {
        switch (S->cm) {
        case 0:
          // Info tag        
          if (!dv_llist_remove(D->itl, node)) {
            dv_llist_add(D->itl, node);
          }
          dv_queue_draw(V);
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
  return TRUE;
}

static gboolean on_motion_event(GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
  dv_view_t *V = (dv_view_t *) user_data;
  dv_dag_t *D = V->D;
  dv_view_status_t *S = V->S;
  if (S->drag_on) {
    // Drag
    double deltax = event->x - S->pressx;
    double deltay = event->y - S->pressy;
    D->x += deltax;
    D->y += deltay;
    S->accdisx += deltax;
    S->accdisy += deltay;
    S->pressx = event->x;
    S->pressy = event->y;
    dv_queue_draw(V);
  }
  return TRUE;
}

static gboolean on_combobox_lt_changed(GtkComboBox *widget, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  int new_lt = gtk_combo_box_get_active(widget);
  dv_do_changing_lt(V, new_lt);
  return TRUE;  
}

static gboolean on_combobox_nc_changed(GtkComboBox *widget, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  V->S->nc = gtk_combo_box_get_active(widget);
  dv_queue_draw(V);
  return TRUE;
}

static gboolean on_combobox_sdt_changed(GtkComboBox *widget, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  V->S->sdt = gtk_combo_box_get_active(widget);
  dv_set_entry_radix_text(V);
  dv_view_layout(V);
  dv_queue_draw(V);
  return TRUE;
}

static gboolean on_entry_radix_activate(GtkEntry *entry, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  dv_get_entry_radix_text(V);
  return TRUE;
}

static gboolean on_combobox_frombt_changed(GtkComboBox *widget, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  V->S->frombt = gtk_combo_box_get_active(widget);
  dv_view_layout(V);
  dv_queue_draw(V);
  return TRUE;
}

static gboolean on_combobox_et_changed(GtkComboBox *widget, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  V->S->et = gtk_combo_box_get_active(widget);
  dv_queue_draw(V);
  return TRUE;
}

static void on_togg_eaffix_toggled(GtkWidget *widget, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    V->S->edge_affix = DV_EDGE_AFFIX_LENGTH;    
  } else {
    V->S->edge_affix = 0;
  }
  dv_queue_draw(V);
}

static gboolean on_combobox_cm_changed(GtkComboBox *widget, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  V->S->cm = gtk_combo_box_get_active(widget);
  return TRUE;
}

static gboolean on_darea_configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer user_data) {
  dv_view_t *V = (dv_view_t *) user_data;
  dv_view_status_t *S = V->S;
  S->vpw = event->width;
  S->vph = event->height;
  return TRUE;
}

static gboolean on_window_key_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  dv_check(CS->activeV);
  GdkEventKey *e = (GdkEventKey *) event;
  //printf("key: %d\n", e->keyval);
  switch (e->keyval) {
  case 120: /* x */
    dv_do_expanding_one(CS->activeV);    
    break;
  case 99: /* c */
    dv_do_collapsing_one(CS->activeV);
    break;
  case 104: /* h */
    dv_do_zoomfit_hoz(CS->activeV);
    break;
  case 118: /* v */
    dv_do_zoomfit_ver(CS->activeV);
    break;
  case 49: /* 1 */
    dv_do_changing_lt(CS->activeV, 0);
    break;
  case 50: /* 2 */
    dv_do_changing_lt(CS->activeV, 1);
    break;
  case 51: /* 3 */
    dv_do_changing_lt(CS->activeV, 2);
    break;
  }
  return TRUE;
}

static void dv_view_status_init(dv_view_t *V, dv_view_status_t *S) {
  S->drag_on = 0;
  S->pressx = S->pressy = 0.0;
  S->accdisx = S->accdisy = 0.0;
  S->nc = DV_NODE_COLOR_INIT;
  S->vpw = S->vph = 0.0;
  dv_animation_init(V, S->a);
  S->nd = 0;
  S->lt = DV_LAYOUT_TYPE_INIT;
  S->sdt = DV_SCALE_TYPE_INIT;
  S->log_radix = DV_RADIX_LOG;
  S->power_radix = DV_RADIX_POWER;
  S->linear_radix = DV_RADIX_LINEAR;
  S->frombt = DV_FROMBT_INIT;
  S->et = DV_EDGE_TYPE_INIT;
  S->edge_affix = DV_EDGE_AFFIX_LENGTH;
  S->cm = DV_CLICK_MODE_INIT;
}

static void dv_view_init(dv_view_t *V) {
  V->D = NULL;
  dv_view_status_init(V, V->S);
  V->toolbar = NULL;
  V->entry_radix = NULL;
  V->combobox_lt = NULL;
  V->darea = NULL;
}

dv_view_t * dv_view_create_new_with_dag(dv_dag_t *D) {
  /* Get new VIEW */
  dv_check(CS->nV < DV_MAX_VIEW);
  dv_view_t * V = &CS->V[CS->nV];
  CS->nV++;
  dv_view_init(V);

  // Set values
  V->D = D;

  // White color
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");

  // Toolbar
  V->toolbar = gtk_toolbar_new();
  GtkWidget *toolbar = V->toolbar;
  //gtk_widget_override_background_color(GTK_WIDGET(toolbar), GTK_STATE_FLAG_NORMAL, white);

  // Layout type combobox
  GtkToolItem *btn_combo_lt = gtk_tool_item_new();
  V->combobox_lt = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(V->combobox_lt), "grid", "Grid like");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(V->combobox_lt), "bounding", "Bounding box");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(V->combobox_lt), "timeline", "Timeline");
  gtk_combo_box_set_active(GTK_COMBO_BOX(V->combobox_lt), DV_LAYOUT_TYPE_INIT);
  g_signal_connect(G_OBJECT(V->combobox_lt), "changed", G_CALLBACK(on_combobox_lt_changed), (void *) V);
  gtk_container_add(GTK_CONTAINER(btn_combo_lt), V->combobox_lt);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo_lt, -1);

  // Node color combobox
  GtkToolItem *btn_combo_nc = gtk_tool_item_new();
  GtkWidget *combobox_nc = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "worker", "Worker");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "cpu", "CPU");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "kind", "Node kind");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "code_start", "Code start");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "code_end", "Code end");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "code_start_end", "Code start-end");
  gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_nc), DV_NODE_COLOR_INIT);
  g_signal_connect(G_OBJECT(combobox_nc), "changed", G_CALLBACK(on_combobox_nc_changed), (void *) V);
  gtk_container_add(GTK_CONTAINER(btn_combo_nc), combobox_nc);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo_nc, -1);

  // Scale-down type combobox
  GtkToolItem *btn_combo_sdt = gtk_tool_item_new();
  GtkWidget *combobox_sdt = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_sdt), "log", "Log");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_sdt), "power", "Power");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_sdt), "linear", "Linear");
  gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_sdt), DV_SCALE_TYPE_INIT);
  g_signal_connect(G_OBJECT(combobox_sdt), "changed", G_CALLBACK(on_combobox_sdt_changed), (void *) V);
  gtk_container_add(GTK_CONTAINER(btn_combo_sdt), combobox_sdt);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo_sdt, -1);

  /*
  // Radix value input
  GtkToolItem *btn_entry_radix = gtk_tool_item_new();
  V->entry_radix = gtk_entry_new();
  //gtk_entry_set_max_length(GTK_ENTRY(S->entry_radix), 10);
  dv_set_entry_radix_text(V);
  g_signal_connect(G_OBJECT(V->entry_radix), "activate", G_CALLBACK(on_entry_radix_activate), (void *) V);
  gtk_container_add(GTK_CONTAINER(btn_entry_radix), V->entry_radix);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_entry_radix, -1);

  // Frombt combobox
  GtkToolItem *btn_combo_frombt = gtk_tool_item_new();
  GtkWidget *combobox_frombt = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_frombt), "not", "Not frombt");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_frombt), "frombt", "From BT");
  gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_frombt), DV_FROMBT_INIT);
  g_signal_connect(G_OBJECT(combobox_frombt), "changed", G_CALLBACK(on_combobox_frombt_changed), (void *) V);
  gtk_container_add(GTK_CONTAINER(btn_combo_frombt), combobox_frombt);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo_frombt, -1);

  // Edge type combobox
  GtkToolItem *btn_combo_et = gtk_tool_item_new();
  GtkWidget *combobox_et = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_et), "none", "None");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_et), "straight", "Straight");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_et), "down", "Down");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_et), "winding", "Winding");
  gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_et), DV_EDGE_TYPE_INIT);
  g_signal_connect(G_OBJECT(combobox_et), "changed", G_CALLBACK(on_combobox_et_changed), (void *) V);
  gtk_container_add(GTK_CONTAINER(btn_combo_et), combobox_et);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo_et, -1);
  */

  // Edge affix toggle
  GtkToolItem *btn_togg_eaffix = gtk_tool_item_new();
  GtkWidget *togg_eaffix = gtk_toggle_button_new_with_label("Edge Affix");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(togg_eaffix), TRUE);
  g_signal_connect(G_OBJECT(togg_eaffix), "toggled", G_CALLBACK(on_togg_eaffix_toggled), (void *) V);
  gtk_container_add(GTK_CONTAINER(btn_togg_eaffix), togg_eaffix);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_togg_eaffix, -1);

  // Click mode combobox
  GtkToolItem *btn_combo_cm = gtk_tool_item_new();
  GtkWidget *combobox_cm = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_cm), "info", "Infotag");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_cm), "expand", "Expand");
  gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_cm), DV_CLICK_MODE_INIT);
  g_signal_connect(G_OBJECT(combobox_cm), "changed", G_CALLBACK(on_combobox_cm_changed), (void *) V);
  gtk_container_add(GTK_CONTAINER(btn_combo_cm), combobox_cm);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo_cm, -1);

  // Zoomfit-horizontally button
  GtkToolItem *btn_zoomfit_hoz = gtk_tool_button_new(NULL, "Best _Fit");
  gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_zoomfit_hoz), "zoom-fit-best");
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoomfit_hoz, -1);
  g_signal_connect(G_OBJECT(btn_zoomfit_hoz), "clicked", G_CALLBACK(on_btn_zoomfit_hoz_clicked), (void *) V);

  // Zoomfit-vertically button
  GtkToolItem *btn_zoomfit_ver = gtk_tool_button_new(NULL, "Best _Fit");
  gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_zoomfit_ver), "zoom-fit-best");
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoomfit_ver, -1);
  g_signal_connect(G_OBJECT(btn_zoomfit_ver), "clicked", G_CALLBACK(on_btn_zoomfit_ver_clicked), (void *) V);

  // Shrink/Expand buttons
  GtkToolItem *btn_shrink = gtk_tool_button_new(NULL, "Zoom _Out");
  gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_shrink), "zoom-out");
  GtkToolItem *btn_expand = gtk_tool_button_new(NULL, "Zoom _In");
  gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_expand), "zoom-in");
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_shrink, -1);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_expand, -1);
  g_signal_connect(G_OBJECT(btn_shrink), "clicked", G_CALLBACK(on_btn_shrink_clicked), (void *) V);
  g_signal_connect(G_OBJECT(btn_expand), "clicked", G_CALLBACK(on_btn_expand_clicked), (void *) V);

  // Drawing Area
  V->darea = gtk_drawing_area_new();
  GtkWidget * darea = V->darea;
  gtk_widget_override_background_color(GTK_WIDGET(darea), GTK_STATE_FLAG_NORMAL, white);
  g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), (void *) V);
  gtk_widget_add_events(GTK_WIDGET(darea), GDK_SCROLL_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
  g_signal_connect(G_OBJECT(darea), "scroll-event", G_CALLBACK(on_scroll_event), (void *) V);
  g_signal_connect(G_OBJECT(darea), "button-press-event", G_CALLBACK(on_button_event), (void *) V);
  g_signal_connect(G_OBJECT(darea), "button-release-event", G_CALLBACK(on_button_event), (void *) V);
  g_signal_connect(G_OBJECT(darea), "motion-notify-event", G_CALLBACK(on_motion_event), (void *) V);
  g_signal_connect(G_OBJECT(darea), "configure-event", G_CALLBACK(on_darea_configure_event), (void *) V);

  return V;  
}

/*-----------------end of VIEW's functions-----------------*/



/*-----------------Main begins-----------------*/

static int open_gui(int argc, char *argv[])
{
  // Initialize window
  CS->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  GtkWidget *window = CS->window;
  //gtk_window_fullscreen(GTK_WINDOW(window));
  //gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
  gtk_window_maximize(GTK_WINDOW(window));
  gtk_window_set_title(GTK_WINDOW(window), "DAG Visualizer");
  g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(G_OBJECT(CS->window), "key-press-event", G_CALLBACK(on_window_key_event), NULL);

  // vbox0
  GtkWidget *vbox0 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(window), vbox0);

  // Menu Bar
  GtkWidget *menubar = gtk_menu_bar_new();
  gtk_box_pack_start(GTK_BOX(vbox0), menubar, FALSE, FALSE, 0);
  // submenu screens
  GtkWidget *screens = gtk_menu_item_new_with_label("Screens");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), screens);
  GtkWidget *screens_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(screens), screens_menu);
  GtkWidget *screen, *screen_menu;
  GSList *group;
  GtkWidget *item;
  char s[100];
  int i, j;
  for (i=0; i<2; i++) {
    group = NULL;
    if (i == 0)
      sprintf(s, "Left");
    else
      sprintf(s, "Right");
    screen = gtk_menu_item_new_with_label(s);
    gtk_menu_shell_append(GTK_MENU_SHELL(screens_menu), screen);
    screen_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(screen), screen_menu);
    for (j=0; j<=CS->nV; j++) {
      if (j == 0)
        sprintf(s, "Hide");
      else
        sprintf(s, "VIEW %d", j-1);
      item = gtk_radio_menu_item_new_with_label(group, s);
      gtk_menu_shell_append(GTK_MENU_SHELL(screen_menu), item);
      group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
      if (j-1 == i)
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);      
    }
  }
  // submenu views
  GtkWidget *views = gtk_menu_item_new_with_label("VIEWs");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), views);
  GtkWidget *views_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(views), views_menu);
  GtkWidget *view, *view_menu;
  for (i=0; i<CS->nV; i++) {
    group = NULL;
    sprintf(s, "VIEW %d", i);
    view = gtk_menu_item_new_with_label(s);
    gtk_menu_shell_append(GTK_MENU_SHELL(views_menu), view);
    view_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(view), view_menu);
    for (j=0; j<CS->nD; j++) {
      sprintf(s, "DAG %d: %ld/%ld", j, CS->D[j].Tn, CS->D[j].Tsz);
      item = gtk_radio_menu_item_new_with_label(group, s);
      gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), item);
      group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
      if (j == (CS->V[i].D - CS->D))
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
    }    
  }
  // submenu DAGs
  GtkWidget *dags = gtk_menu_item_new_with_label("DAGs");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), dags);
  GtkWidget *dags_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(dags), dags_menu);
  GtkWidget *dag, *dag_menu;
  for (i=0; i<CS->nD; i++) {
    group = NULL;
    sprintf(s, "DAG %d: %ld/%ld", i, CS->D[i].Tn, CS->D[i].Tsz);
    dag = gtk_menu_item_new_with_label(s);
    gtk_menu_shell_append(GTK_MENU_SHELL(dags_menu), dag);
    dag_menu = gtk_menu_new();
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(dag), dag_menu);
    for (j=0; j<CS->nP; j++) {
      sprintf(s, "PIDAG %d: (%ld)%s", j, CS->P[j].n, CS->Pfn[j]);
      item = gtk_radio_menu_item_new_with_label(group, s);
      gtk_menu_shell_append(GTK_MENU_SHELL(dag_menu), item);
      group = gtk_radio_menu_item_get_group(GTK_RADIO_MENU_ITEM(item));
      if (j == (CS->D[i].P - CS->P))
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
    }    
  }
  // submenu PIDAGs
  GtkWidget *pidags = gtk_menu_item_new_with_label("PIDAGs");
  gtk_menu_shell_append(GTK_MENU_SHELL(menubar), pidags);
  GtkWidget *pidags_menu = gtk_menu_new();
  gtk_menu_item_set_submenu(GTK_MENU_ITEM(pidags), pidags_menu);
  GtkWidget *pidag;
  for (i=0; i<CS->nP; i++) {
    sprintf(s, "PIDAG %d: (%ld)%s", i, CS->P[i].n, CS->Pfn[i]);
    pidag = gtk_menu_item_new_with_label(s);
    gtk_menu_shell_append(GTK_MENU_SHELL(pidags_menu), pidag);
  }
  

  // hbox
  GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(vbox0), hbox, TRUE, TRUE, 0);
  gtk_box_set_homogeneous(GTK_BOX(hbox), TRUE);

  // vbox
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(hbox), vbox);
  // Pack activeV to vbox
  dv_view_t *V = CS->activeV;
  if (V && V->toolbar && V->darea) {
    gtk_box_pack_start(GTK_BOX(vbox), V->toolbar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), V->darea, TRUE, TRUE, 0);
  }

  // vbox2
  GtkWidget *vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(hbox), vbox2);
  // Pack V2 to vbox
  dv_view_t *V2 = V + 1;
  if (V2 && V2->toolbar && V2->darea) {
    gtk_box_pack_start(GTK_BOX(vbox2), V2->toolbar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox2), V2->darea, TRUE, TRUE, 0);
  }

  // Run main loop
  gtk_widget_show_all(window);
  gtk_main();

  return 1;
}

int main(int argc, char *argv[])
{
  dv_global_state_init(CS);
  gtk_init(&argc, &argv);
  //dv_get_env();
  //if (argc > 1)  print_dag_file(argv[1]);
  if (argc > 1) {
    int i;
    for (i=1; i<argc; i++)
      dv_pidag_read_new_file(argv[i]);
  }
  if (CS->nP >= 1) {
    dv_dag_t *D = dv_dag_create_new_with_pidag(CS->P);
    dv_dag_t *D2 = dv_dag_create_new_with_pidag(CS->P);
    //print_dvdag(D);
    dv_view_t *V = dv_view_create_new_with_dag(D);
    dv_view_t *V2 = dv_view_create_new_with_dag(D2);
    int i;
    for (i=0; i<2; i++)
      dv_do_expanding_one(V);
    dv_view_layout(V); // must be called before first call to view_draw() for rt->vl
    dv_do_expanding_one(V2);
    dv_view_layout(V2); // must be called before first call to view_draw() for rt->vl
    dv_global_state_set_active_view(V);
  }
  return open_gui(argc, argv);
}

/*-----------------Main ends-------------------*/
