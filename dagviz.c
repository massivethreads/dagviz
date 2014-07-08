#include "dagviz.h"

dv_status_t S[1];
dr_pi_dag P[1];
dv_dag_t G[1];

GtkWidget *window;
GtkWidget *darea;
GtkWidget *dv_entry_radix;
GtkWidget *dv_combobox_lt;

/*--------Interactive processing functions------------*/

static void dv_do_zooming(double zoom_ratio, double posx, double posy)
{
  posx -= G->basex + G->x;
  posy -= G->basey + G->y;
  double deltax = posx / G->zoom_ratio * zoom_ratio - posx;
  double deltay = posy / G->zoom_ratio * zoom_ratio - posy;
  G->x -= deltax;
  G->y -= deltay;
  G->zoom_ratio = zoom_ratio;
  gtk_widget_queue_draw(darea);
}

static void dv_get_zoomfit_hoz_ratio(double w, double h, double *pz, double *px, double *py) {
  double zoom_ratio = 1.0;
  double x = 0.0;
  double y = 0.0;
  double d1, d2;
  double lw, rw;
  if (S->lt == 0) {
    // Grid-based
    lw = G->rt->vl->lc * DV_HDIS;
    rw = G->rt->vl->rc * DV_HDIS;
    d1 = lw + rw;
    d2 = w - 2 * (DV_ZOOM_TO_FIT_MARGIN + DV_RADIUS);
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    x -= (rw - lw) * 0.5 * zoom_ratio;
  } else if (S->lt == 1) {
    // Bounding box
    d1 = G->rt->lw + G->rt->rw;
    d2 = w - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    x -= (G->rt->rw - G->rt->lw) * 0.5 * zoom_ratio;
  } else if (S->lt == 2) {
    // Timeline
    d1 = 2*DV_RADIUS + (G->nw - 1) * (2*DV_RADIUS + DV_HDIS);
    d2 = w - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
  } else
    dv_check(0);
  *pz = zoom_ratio;
  *px = x;
  *py = y;
}

static void dv_get_zoomfit_ver_ratio(double w, double h, double *pz, double *px, double *py) {
  double zoom_ratio = 1.0;
  double x = 0.0;
  double y = 0.0;
  double d1, d2;
  double lw, rw;
  if (S->lt == 0) {
    // Grid-based
    d1 = G->rt->dc * DV_VDIS;
    d2 = h - 2 * (DV_ZOOM_TO_FIT_MARGIN + DV_RADIUS);
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    lw = G->rt->vl->lc * DV_HDIS;
    rw = G->rt->vl->rc * DV_HDIS;
    x -= (rw - lw) * 0.5 * zoom_ratio;
  } else if (S->lt == 1) {
    // Bounding box
    d1 = G->rt->dw;
    d2 = h - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;    
    x -= (G->rt->rw - G->rt->lw) * 0.5 * zoom_ratio;
  } else if (S->lt == 2) {
    // Timeline
    d1 = 10 + G->rt->dw;
    d2 = h - 2 * DV_ZOOM_TO_FIT_MARGIN;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    double lrw = 2*DV_RADIUS + (G->nw - 1) * (2*DV_RADIUS + DV_HDIS);
    x += (w - lrw * zoom_ratio) * 0.5;
  } else
    dv_check(0);
  *pz = zoom_ratio;
  *px = x;
  *py = y;
}

static void dv_do_zoomfit_hoz() {
  G->x = G->y = 0.0;
  dv_get_zoomfit_hoz_ratio(S->vpw, S->vph, &G->zoom_ratio, &G->x, &G->y);
  gtk_widget_queue_draw(darea);
}

static void dv_do_zoomfit_ver() {
  dv_get_zoomfit_ver_ratio(S->vpw, S->vph, &G->zoom_ratio, &G->x, &G->y);
  gtk_widget_queue_draw(darea);
}

static void dv_do_changing_lt(int new_lt) {
  int old_lt = S->lt;
  S->lt = new_lt;
  gtk_combo_box_set_active(GTK_COMBO_BOX(dv_combobox_lt), new_lt);
  dv_layout_dvdag(G);
  if (S->lt != old_lt)
    dv_do_zoomfit_hoz();
}

static void dv_do_drawing(cairo_t *cr)
{
  // First time only
  if (G->init) {
    dv_get_zoomfit_hoz_ratio(S->vpw, S->vph, &G->zoom_ratio, &G->x, &G->y);
    G->init = 0;
  }
  // Draw graph
  cairo_save(cr);
  cairo_translate(cr, G->basex + G->x, G->basey + G->y);
  cairo_scale(cr, G->zoom_ratio, G->zoom_ratio);
  dv_draw_dvdag(cr, G);
  cairo_restore(cr);

  // Draw status line
  dv_draw_status(cr);
}


static void dv_do_expanding_one_1(dv_dag_node_t *node) {
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

static void dv_do_expanding_one_r(dv_dag_node_t *node) {
  if ((dv_is_shrinked(node) || dv_is_shrinking(node))
      && !dv_is_expanding(node)) {
    // expand node
    dv_do_expanding_one_1(node);
  } else if (dv_is_union(node)) {
    /* Call inward */
    dv_check(node->head);
    dv_do_expanding_one_r(node->head);
  }
  /* Call link-along */
  dv_llist_iterate_init(node->links);
  dv_dag_node_t *u;
  while (u = (dv_dag_node_t *) dv_llist_iterate_next(node->links)) {
    dv_do_expanding_one_r(u);
  }
}

static void dv_do_expanding_one() {
  dv_do_expanding_one_r(G->rt);
  if (!S->a->on) {
    dv_layout_dvdag(G);
    gtk_widget_queue_draw(darea);
  }
}


static void dv_do_collapsing_one_1(dv_dag_node_t *node) {
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

static void dv_do_collapsing_one_r(dv_dag_node_t *node) {
  if (dv_is_union(node) && !dv_is_shrinking(node)
      && (dv_is_expanded(node) || dv_is_expanding(node))) {
    // check if node has expanded node, excluding shrinking nodes
    int has_expanded_node = 0;
    dv_stack_t s[1];
    dv_stack_init(s);
    dv_check(node->head);
    dv_stack_push(s, (void *) node->head);
    while (s->top) {
      dv_dag_node_t * x = (dv_dag_node_t *) dv_stack_pop(s);
      if (dv_is_union(x) && (dv_is_expanded(x) || dv_is_expanding(x))
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
      dv_do_collapsing_one_1(node);
    } else {
      /* Call inward */
      dv_do_collapsing_one_r(node->head);
    }
  } 
  /* Call link-along */
  dv_llist_iterate_init(node->links);
  dv_dag_node_t *u;
  while (u = (dv_dag_node_t *) dv_llist_iterate_next(node->links)) {
    dv_do_collapsing_one_r(u);
  }
}

static void dv_do_collapsing_one() {
  dv_do_collapsing_one_r(G->rt);
  if (!S->a->on) {
    dv_layout_dvdag(G);
    gtk_widget_queue_draw(darea);
  }
}

static dv_dag_node_t *dv_do_finding_clicked_node_1(double x, double y, dv_dag_node_t *node) {
  dv_dag_node_t * ret = NULL;
  double vc, hc;
  switch (S->lt) {
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

static dv_dag_node_t *dv_do_finding_clicked_node_r(double x, double y, dv_dag_node_t *node) {
  /* Call inward */
  if (dv_is_union(node)
      && !dv_is_shrinking(node)
      && (dv_is_expanded(node) || dv_is_expanding(node))) {
    dv_do_finding_clicked_node_r(x, y, node->head);
  } else if (dv_do_finding_clicked_node_1(x, y, node)) {
      return node;
  }
  /* Call link-along */
  dv_llist_iterate_init(node->links);
  dv_dag_node_t *u;
  while (u = (dv_dag_node_t *) dv_llist_iterate_next(node->links)) {
    if (dv_do_finding_clicked_node_r(x, y, u))
      return u;
  }
  return 0;
}

static dv_dag_node_t *dv_do_finding_clicked_node(double x, double y) {
  return dv_do_finding_clicked_node_r(x, y, G->rt);
}

static void dv_set_entry_radix_text() {
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
  gtk_entry_set_width_chars(GTK_ENTRY(dv_entry_radix), strlen(str));
  gtk_entry_set_text(GTK_ENTRY(dv_entry_radix), str);
}

static void dv_get_entry_radix_text() {
  double radix = atof(gtk_entry_get_text(GTK_ENTRY(dv_entry_radix)));
  if (S->sdt == 0)
    S->log_radix = radix;
  else if (S->sdt == 1)
    S->power_radix = radix;
  else if (S->sdt == 2)
    S->linear_radix = radix;
  else
    dv_check(0);
  dv_layout_dvdag(G);
  gtk_widget_queue_draw(darea);
}

/*--------end of Interactive processing functions------------*/


/*-----------------DV Visualizer GUI-------------------*/

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  if (S->lt == 0) {
    G->basex = 0.5 * S->vpw;
    G->basey = DV_ZOOM_TO_FIT_MARGIN + DV_RADIUS;
  } else if (S->lt == 1) {
    //G->basex = 0.5 * S->vpw - 0.5 * (G->rt->rw - G->rt->lw);
    G->basex = 0.5 * S->vpw;
    G->basey = DV_ZOOM_TO_FIT_MARGIN;
  } else if (S->lt == 2) {
    G->basex = DV_ZOOM_TO_FIT_MARGIN;
    G->basey = DV_ZOOM_TO_FIT_MARGIN;
  } else
    dv_check(0);
  // Draw
  dv_do_drawing(cr);
  return FALSE;
}

static gboolean on_btn_zoomfit_hoz_clicked(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  dv_do_zoomfit_hoz();
  return TRUE;
}

static gboolean on_btn_zoomfit_ver_clicked(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  dv_do_zoomfit_ver();
  return TRUE;
}

static gboolean on_btn_shrink_clicked(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  dv_do_collapsing_one();
  return TRUE;
}

static gboolean on_btn_expand_clicked(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  dv_do_expanding_one();
  return TRUE;
}

static gboolean on_scroll_event(GtkWidget *widget, GdkEventScroll *event, gpointer user_data) {
  if (event->direction == GDK_SCROLL_UP) {
    dv_do_zooming(G->zoom_ratio * DV_ZOOM_INCREMENT, event->x, event->y);
  } else if (event->direction == GDK_SCROLL_DOWN) {
    dv_do_zooming(G->zoom_ratio / DV_ZOOM_INCREMENT, event->x, event->y);
  }
  return TRUE;
}

static gboolean on_button_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
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
      double ox = (event->x - G->basex - G->x) / G->zoom_ratio;
      double oy = (event->y - G->basey - G->y) / G->zoom_ratio;
      dv_dag_node_t *node = dv_do_finding_clicked_node(ox, oy);
      if (node) {
        switch (S->cm) {
        case 0:
          // Info tag        
          if (!dv_llist_remove(G->itl, node)) {
            dv_llist_add(G->itl, node);
          }
          gtk_widget_queue_draw(darea);
          break;
        case 1:
          // Expand/Collapse
          if (dv_is_union(node)) {
            if ((dv_is_shrinked(node) || dv_is_shrinking(node))
                && !dv_is_expanding(node))
              dv_do_expanding_one_1(node);
          } else {
            dv_do_collapsing_one_r(node->parent);
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
  if (S->drag_on) {
    // Drag
    double deltax = event->x - S->pressx;
    double deltay = event->y - S->pressy;
    G->x += deltax;
    G->y += deltay;
    S->accdisx += deltax;
    S->accdisy += deltay;
    S->pressx = event->x;
    S->pressy = event->y;
    gtk_widget_queue_draw(darea);
  }
  return TRUE;
}

static gboolean on_combobox_lt_changed(GtkComboBox *widget, gpointer user_data) {
  int new_lt = gtk_combo_box_get_active(widget);
  dv_do_changing_lt(new_lt);
  return TRUE;  
}

static gboolean on_combobox_nc_changed(GtkComboBox *widget, gpointer user_data) {
  S->nc = gtk_combo_box_get_active(widget);
  gtk_widget_queue_draw(darea);
  return TRUE;
}

static gboolean on_combobox_sdt_changed(GtkComboBox *widget, gpointer user_data) {
  S->sdt = gtk_combo_box_get_active(widget);
  dv_set_entry_radix_text();
  dv_layout_dvdag(G);
  gtk_widget_queue_draw(darea);
  return TRUE;
}

static gboolean on_combobox_frombt_changed(GtkComboBox *widget, gpointer user_data) {
  S->frombt = gtk_combo_box_get_active(widget);
  dv_layout_dvdag(G);
  gtk_widget_queue_draw(darea);
  return TRUE;
}

static gboolean on_combobox_et_changed(GtkComboBox *widget, gpointer user_data) {
  S->et = gtk_combo_box_get_active(widget);
  gtk_widget_queue_draw(darea);
  return TRUE;
}

static void on_togg_eaffix_toggled(GtkWidget *widget, gpointer user_data) {
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    S->edge_affix = DV_EDGE_AFFIX_LENGTH;    
  } else {
    S->edge_affix = 0;
  }
  gtk_widget_queue_draw(darea);
}

static gboolean on_combobox_cm_changed(GtkComboBox *widget, gpointer user_data) {
  S->cm = gtk_combo_box_get_active(widget);
  return TRUE;
}

static gboolean on_entry_radix_activate(GtkEntry *entry, gpointer user_data) {
  dv_get_entry_radix_text();
  return TRUE;
}

static gboolean on_darea_configure_event(GtkWidget *widget, GdkEventConfigure *event, gpointer user_data) {
  S->vpw = event->width;
  S->vph = event->height;
  return TRUE;
}

static gboolean on_window_key_event(GtkWidget *widget, GdkEvent *event, gpointer user_data) {
  GdkEventKey *e = (GdkEventKey *) event;
  //printf("key: %d\n", e->keyval);
  switch (e->keyval) {
  case 120: /* x */
    dv_do_expanding_one();    
    break;
  case 99: /* c */
    dv_do_collapsing_one();
    break;
  case 104: /* h */
    dv_do_zoomfit_hoz();
    break;
  case 118: /* v */
    dv_do_zoomfit_ver();
    break;
  case 49: /* 1 */
    dv_do_changing_lt(0);
    break;
  case 50: /* 2 */
    dv_do_changing_lt(1);
    break;
  case 51: /* 3 */
    dv_do_changing_lt(2);
    break;
  }
  return TRUE;
}

int open_gui(int argc, char *argv[])
{
  gtk_init(&argc, &argv);
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");

  // Main window
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  //gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
  //gtk_window_fullscreen(GTK_WINDOW(window));
  gtk_window_maximize(GTK_WINDOW(window));
  gtk_window_set_title(GTK_WINDOW(window), "DAG Visualizer");
  g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(G_OBJECT(window), "key-press-event", G_CALLBACK(on_window_key_event), NULL);
  GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(window), vbox);  

  // Toolbar
  GtkWidget *toolbar = gtk_toolbar_new();
  //gtk_widget_override_background_color(GTK_WIDGET(toolbar), GTK_STATE_FLAG_NORMAL, white);

  // Layout type combobox
  GtkToolItem *btn_combo_lt = gtk_tool_item_new();
  dv_combobox_lt = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(dv_combobox_lt), "grid", "Grid like");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(dv_combobox_lt), "bounding", "Bounding box");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(dv_combobox_lt), "timeline", "Timeline");
  gtk_combo_box_set_active(GTK_COMBO_BOX(dv_combobox_lt), DV_LAYOUT_TYPE_INIT);
  g_signal_connect(G_OBJECT(dv_combobox_lt), "changed", G_CALLBACK(on_combobox_lt_changed), NULL);
  gtk_container_add(GTK_CONTAINER(btn_combo_lt), dv_combobox_lt);
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
  g_signal_connect(G_OBJECT(combobox_nc), "changed", G_CALLBACK(on_combobox_nc_changed), NULL);
  gtk_container_add(GTK_CONTAINER(btn_combo_nc), combobox_nc);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo_nc, -1);

  // Scale-down type combobox
  GtkToolItem *btn_combo_sdt = gtk_tool_item_new();
  GtkWidget *combobox_sdt = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_sdt), "log", "Log");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_sdt), "power", "Power");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_sdt), "linear", "Linear");
  gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_sdt), DV_SCALE_TYPE_INIT);
  g_signal_connect(G_OBJECT(combobox_sdt), "changed", G_CALLBACK(on_combobox_sdt_changed), NULL);
  gtk_container_add(GTK_CONTAINER(btn_combo_sdt), combobox_sdt);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo_sdt, -1);

  // Radix value input
  GtkToolItem *btn_entry_radix = gtk_tool_item_new();
  dv_entry_radix = gtk_entry_new();
  //gtk_entry_set_max_length(GTK_ENTRY(dv_entry_radix), 10);
  dv_set_entry_radix_text();
  g_signal_connect(G_OBJECT(dv_entry_radix), "activate", G_CALLBACK(on_entry_radix_activate), NULL);
  gtk_container_add(GTK_CONTAINER(btn_entry_radix), dv_entry_radix);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_entry_radix, -1);

  // Frombt combobox
  GtkToolItem *btn_combo_frombt = gtk_tool_item_new();
  GtkWidget *combobox_frombt = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_frombt), "not", "Not frombt");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_frombt), "frombt", "From BT");
  gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_frombt), DV_FROMBT_INIT);
  g_signal_connect(G_OBJECT(combobox_frombt), "changed", G_CALLBACK(on_combobox_frombt_changed), NULL);
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
  g_signal_connect(G_OBJECT(combobox_et), "changed", G_CALLBACK(on_combobox_et_changed), NULL);
  gtk_container_add(GTK_CONTAINER(btn_combo_et), combobox_et);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo_et, -1);

  // Edge affix toggle
  GtkToolItem * btn_togg_eaffix = gtk_tool_item_new();
  GtkWidget * togg_eaffix = gtk_toggle_button_new_with_label("Edge Affix");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(togg_eaffix), TRUE);
  g_signal_connect(G_OBJECT(togg_eaffix), "toggled", G_CALLBACK(on_togg_eaffix_toggled), NULL);
  gtk_container_add(GTK_CONTAINER(btn_togg_eaffix), togg_eaffix);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_togg_eaffix, -1);
  
  // Click mode combobox
  GtkToolItem *btn_combo_cm = gtk_tool_item_new();
  GtkWidget *combobox_cm = gtk_combo_box_text_new();
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_cm), "info", "Infotag");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_cm), "expand", "Expand");
  gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_cm), DV_CLICK_MODE_INIT);
  g_signal_connect(G_OBJECT(combobox_cm), "changed", G_CALLBACK(on_combobox_cm_changed), NULL);
  gtk_container_add(GTK_CONTAINER(btn_combo_cm), combobox_cm);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo_cm, -1);

  // Zoomfit-horizontally button
  GtkToolItem *btn_zoomfit_hoz = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_FIT);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoomfit_hoz, -1);
  g_signal_connect(G_OBJECT(btn_zoomfit_hoz), "clicked", G_CALLBACK(on_btn_zoomfit_hoz_clicked), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

  // Zoomfit-vertically button
  GtkToolItem *btn_zoomfit_ver = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_FIT);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoomfit_ver, -1);
  g_signal_connect(G_OBJECT(btn_zoomfit_ver), "clicked", G_CALLBACK(on_btn_zoomfit_ver_clicked), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);

  // Shrink/Expand buttons
  GtkToolItem *btn_shrink = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_OUT);
  GtkToolItem *btn_expand = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_IN);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_shrink, -1);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_expand, -1);
  g_signal_connect(G_OBJECT(btn_shrink), "clicked", G_CALLBACK(on_btn_shrink_clicked), NULL);
  g_signal_connect(G_OBJECT(btn_expand), "clicked", G_CALLBACK(on_btn_expand_clicked), NULL);

  // Drawing Area
  darea = gtk_drawing_area_new();
  gtk_widget_override_background_color(GTK_WIDGET(darea), GTK_STATE_FLAG_NORMAL, white);
  g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), NULL);
  gtk_widget_add_events(GTK_WIDGET(darea), GDK_SCROLL_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
  g_signal_connect(G_OBJECT(darea), "scroll-event", G_CALLBACK(on_scroll_event), NULL);
  g_signal_connect(G_OBJECT(darea), "button-press-event", G_CALLBACK(on_button_event), NULL);
  g_signal_connect(G_OBJECT(darea), "button-release-event", G_CALLBACK(on_button_event), NULL);
  g_signal_connect(G_OBJECT(darea), "motion-notify-event", G_CALLBACK(on_motion_event), NULL);
  g_signal_connect(G_OBJECT(darea), "configure-event", G_CALLBACK(on_darea_configure_event), NULL);
  gtk_box_pack_start(GTK_BOX(vbox), darea, TRUE, TRUE, 0);

  // Run main loop
  gtk_widget_show_all(window);
  gtk_main();

  return 1;
}
/*-----------------end of DV Visualizer GUI-------------------*/


/*---------------Initialization Functions------*/

static void dv_status_init() {
  S->drag_on = 0;
  S->pressx = S->pressy = 0.0;
  S->accdisx = S->accdisy = 0.0;
  S->nc = DV_NODE_COLOR_INIT;
  S->vpw = S->vph = 0.0;
  dv_animation_init(S->a);
  S->nd = 0;
  S->lt = DV_LAYOUT_TYPE_INIT;
  S->sdt = DV_SCALE_TYPE_INIT;
  S->log_radix = DV_RADIX_LOG;
  S->power_radix = DV_RADIX_POWER;
  S->linear_radix = DV_RADIX_LINEAR;
  S->frombt = DV_FROMBT_INIT;
  int i;
  for (i=0; i<DV_NUM_COLOR_POOLS; i++)
    S->CP_sizes[i] = 0;
  S->et = DV_EDGE_TYPE_INIT;
  S->edge_affix = DV_EDGE_AFFIX_LENGTH;
  S->cm = DV_CLICK_MODE_INIT;
}

/*---------------end of Initialization Functions------*/


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


/*-----------------Main begins-----------------*/

int main(int argc, char *argv[])
{
  dv_status_init();
  dv_get_env();
  if (argc > 1) {
    dv_read_dag_file_to_pidag(argv[1], P);
    dv_convert_pidag_to_dvdag(P, G);
    //print_dvdag(G);
    int i;
    for (i=0; i<2; i++)
      dv_do_expanding_one();
    //dv_layout_dvdag(G);
    //check_layout(G);
  }
  //if (argc > 1)  print_dag_file(argv[1]);
  return open_gui(argc, argv);
  //return 1;
}

/*-----------------Main ends-------------------*/
