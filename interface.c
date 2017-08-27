#include "dagviz-gtk.h"
#include "callback.h"

dv_global_state_t  DVG[1];
dv_gui_t GUI[1];

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
  //{"white", "white", "white"};
  {"orange", "yellow", "cyan"};

const char * const DV_RADIAL_PATTERN_STOPS[] =
  {"black", "white"};

const int DV_RADIAL_PATTERN_STOPS_NUM = 2;

/*---------------Environment Variables-----*/

_static_unused_ int
dv_get_env_int(char * s, int * t) {
  char * v = getenv(s);
  if (v) {
    *t = atoi(v);
    return 1;
  }
  return 0;
}

_static_unused_ int
dv_get_env_long(char * s, long * t) {
  char * v = getenv(s);
  if (v) {
    *t = atol(v);
    return 1;
  }
  return 0;
}

_static_unused_ int
dv_get_env_string(char * s, char ** t) {
  char * v = getenv(s);
  if (v) {
    *t = strdup(v);
    return 1;
  }
  return 0;
}

_static_unused_ void
dv_get_env() {
  //dv_get_env_int("DV_DEPTH", &S->cur_d);
}

/*---------------end of Environment Variables-----*/



/*-----------------Global State-----------------*/

void
dv_global_state_init() {
  dm_global_state_init();
  memset(DVG, 0, sizeof(dv_global_state_t));
  DVG->nV = 0;
  DVG->nVP = 0;
  DVG->activeV = NULL;
  DVG->activeVP = NULL;
  DVG->error = DV_OK;
  int i;
  for (i = 0; i < DV_NUM_COLOR_POOLS; i++)
    DVG->CP_sizes[i] = 0;
  dv_btsample_viewer_init(DVG->btviewer);
  DVG->SD->ne = 0;
  for (i = 0; i < DV_MAX_DISTRIBUTION; i++) {
    DVG->SD->e[i].dag_id = -1; /* none */
    DVG->SD->e[i].type = 0;
    DVG->SD->e[i].stolen = 0;
    DVG->SD->e[i].title = NULL;
    DVG->SD->e[i].title_entry = NULL;
  }
  DVG->SD->xrange_from = 0;
  DVG->SD->xrange_to = 10000;
  DVG->SD->node_pool_label = NULL;
  DVG->SD->entry_pool_label = NULL;
  DVG->SD->fn = DV_STAT_DISTRIBUTION_OUTPUT_DEFAULT_NAME;
  DVG->SD->bar_width = 20;
  
  DVG->context_view = NULL;
  DVG->context_node = NULL;

  DVG->verbose_level = DV_VERBOSE_LEVEL_DEFAULT;

  DVG->cp_colors[DV_CRITICAL_PATH_0] = DV_CRITICAL_PATH_0_COLOR;
  DVG->cp_colors[DV_CRITICAL_PATH_1] = DV_CRITICAL_PATH_1_COLOR;
  DVG->cp_colors[DV_CRITICAL_PATH_2] = DV_CRITICAL_PATH_2_COLOR;

  DVG->opts = dv_options_default_values;
}

/*-----------------end of Global State-----------------*/



/*--------Interactive processing functions------------*/

void
dv_queue_draw_viewport(dv_viewport_t * VP) {
  if (DVG->verbose_level >= 2) {
    fprintf(stderr, "dv_queue_draw_viewport()\n");
  }
  fflush(stdin);
  gtk_widget_queue_draw(VP->darea);
}

void
dv_queue_draw_view(dv_view_t * V) {
  int i;
  for (i=0; i<DVG->nVP; i++)
    if (V->mVP[i])
      dv_queue_draw_viewport(&DVG->VP[i]);
}

void
dv_queue_draw_dag(dm_dag_t * D) {
  int todraw[DVG->nVP];
  int i, j;
  for (i = 0; i < DVG->nVP; i++)
    todraw[i] = 0;
  for (i = 0; i < DVG->nV; i++)
    if (((dv_dag_t*)D->g)->mV[i]) {
      for (j = 0; j < DVG->nVP; j++)
        if (DVG->V[i].mVP[j])
          todraw[j]++;
    }
  for (i = 0; i < DVG->nVP; i++)
    if (todraw[i])
      dv_queue_draw_viewport(&DVG->VP[i]);
}

void
dv_queue_draw_pidag(dm_pidag_t * P) {
  int i;
  for (i = 0; i < DMG->nD; i++)
    if (DMG->D[i].P == P)
      dv_queue_draw_dag(&DMG->D[i]);
}

void
dv_queue_draw(dv_view_t * V) {
  dv_queue_draw_view(V);
}

void
dv_queue_draw_d(dv_view_t * V) {
  dm_dag_t * D = V->D;
  dv_queue_draw_dag(D);
  /*
  int i;
  for (i=0; i<DVG->nV; i++)
    if (DVG->V[i].D == D)
      dv_queue_draw(&DVG->V[i]);
  */
}

void
dv_queue_draw_d_p(dv_view_t * V) {
  dm_pidag_t * P = V->D->P;
  dv_queue_draw_pidag(P);
  /*
  int i;
  for (i=0; i<DVG->nV; i++)
    if (DVG->V[i].D->P == P)
      dv_queue_draw(&DVG->V[i]);
  */
}

void
dv_view_clip(dv_view_t * V, cairo_t * cr) {
  double margin = DVG->opts.clipping_frame_margin;
  cairo_rectangle(cr, margin, margin, V->S->vpw - margin * 2, V->S->vph - margin * 2);
  cairo_clip(cr);
}

double
dv_view_clip_get_bound_left(dv_view_t * V) {
  double margin = DVG->opts.clipping_frame_margin;
  return dv_view_convert_viewport_x_to_graph_x(V, margin);
}

double
dv_view_clip_get_bound_right(dv_view_t * V) {
  double margin = DVG->opts.clipping_frame_margin;
  return dv_view_convert_viewport_x_to_graph_x(V, V->S->vpw - margin);
}

double
dv_view_clip_get_bound_up(dv_view_t * V) {
  double margin = DVG->opts.clipping_frame_margin;
  return dv_view_convert_viewport_y_to_graph_y(V, margin);
}

double
dv_view_clip_get_bound_down(dv_view_t * V) {
  double margin = DVG->opts.clipping_frame_margin;
  return dv_view_convert_viewport_y_to_graph_y(V, V->S->vph - margin);
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

void
dv_viewport_draw(dv_viewport_t * VP, cairo_t * cr, int to_draw_rulers) {
  dv_view_t * V;
  dv_view_status_t * S;
  int count = 0;
  int i;
  for (i=0; i<DVG->nV; i++)
    if (VP->mV[i]) {
      V = DVG->V + i;
      S = V->S;
      switch (S->lt) {
      case DV_LAYOUT_TYPE_DAG:
        S->basex = 0.5 * S->vpw;
        S->basey = DVG->opts.zoom_to_fit_margin;
        break;
      case DV_LAYOUT_TYPE_DAG_BOX_LOG:
      case DV_LAYOUT_TYPE_DAG_BOX_POWER:
      case DV_LAYOUT_TYPE_DAG_BOX_LINEAR:
        //G->basex = 0.5 * S->vpw - 0.5 * (G->rt->rw - G->rt->lw);
        S->basex = 0.5 * S->vpw;
        S->basey = DVG->opts.zoom_to_fit_margin;
        break;
      case DV_LAYOUT_TYPE_TIMELINE_VER:
      case DV_LAYOUT_TYPE_TIMELINE:
        S->basex = DVG->opts.zoom_to_fit_margin;
        S->basey = DVG->opts.zoom_to_fit_margin;
        break;
      case DV_LAYOUT_TYPE_PARAPROF:
        S->basex = DVG->opts.paraprof_zoom_to_fit_margin;
        S->basey = S->vph - DVG->opts.paraprof_zoom_to_fit_margin_bottom;
        break;
      case DV_LAYOUT_TYPE_CRITICAL_PATH:
        S->basex = DVG->opts.paraprof_zoom_to_fit_margin;
        S->basey = S->vph - DVG->opts.paraprof_zoom_to_fit_margin_bottom;
        break;
      default:
        dv_check(0);
      }
      // Draw
      dv_view_prepare_drawing(V, cr);
      if (V->S->show_status)
        dv_view_draw_status(V, cr, count);
      if (V->S->show_legend)
        dv_view_draw_legend(V, cr);
      dv_dag_update_status_label(V->D);
      count++;
    }
  //dv_viewport_draw_label(VP, cr);
  if (to_draw_rulers) {
    if (VP->mainV) {
      switch (VP->mainV->S->lt) {
      case DV_LAYOUT_TYPE_DAG:
        break;
      case DV_LAYOUT_TYPE_DAG_BOX_LOG:
      case DV_LAYOUT_TYPE_DAG_BOX_POWER:
      case DV_LAYOUT_TYPE_DAG_BOX_LINEAR:
        break;
      case DV_LAYOUT_TYPE_TIMELINE_VER:
        break;
      case DV_LAYOUT_TYPE_TIMELINE:
      case DV_LAYOUT_TYPE_PARAPROF:
      case DV_LAYOUT_TYPE_CRITICAL_PATH:
        dv_paraprof_draw_rulers(VP, VP->mainV, cr);
        break;
      }
    }
    dv_viewport_draw_rulers(VP, cr);
    if (VP == DVG->activeVP && VP != DVG->VP)
      dv_viewport_draw_focused_mark(VP, cr);
  }
  dv_statusbar_update_selection_status();
  dv_statusbar_update_pointer_status();
}

/*--------end of Interactive processing functions------------*/


/*-----------------VIEW's functions-----------------*/

void
dv_view_toolbox_init(dv_view_toolbox_t * T, dv_view_t * V) {
  T->V = V;
  T->window = NULL;
  T->combobox_lt = NULL;
  T->combobox_nc = NULL;
  //T->combobox_sdt = NULL;
  T->entry_radix = NULL;
  T->combobox_frombt = NULL;
  T->combobox_et = NULL;
  T->togg_eaffix = NULL;
  T->checkbox_legend = NULL;
  T->checkbox_status = NULL;
  T->entry_remark = NULL;
  T->checkbox_remain_inner = NULL;
  T->checkbox_color_remarked_only = NULL;
  T->entry_paraprof_min_interval = NULL;

  /* entry_search */
  T->entry_search = gtk_entry_new();
  gtk_entry_set_placeholder_text(GTK_ENTRY(T->entry_search), "Search e.g. 0");
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->entry_search), "Search by node's index in DAG file (first number in node's info tag)");
  gtk_entry_set_max_length(GTK_ENTRY(T->entry_search), 7);
  g_signal_connect(G_OBJECT(T->entry_search), "activate", G_CALLBACK(on_entry_search_activate), (void *) V);

  /* checkbox_xzoom */
  T->checkbox_xzoom = gtk_check_button_new_with_label("X Zoom");
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->checkbox_xzoom), "Cairo's horizontal zoom when scrolling");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_xzoom), V->S->do_zoom_x);
  g_signal_connect(G_OBJECT(T->checkbox_xzoom), "toggled", G_CALLBACK(on_checkbox_xzoom_toggled), (void *) V);

  /* checkbox_yzoom */
  T->checkbox_yzoom = gtk_check_button_new_with_label("Y Zoom");
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->checkbox_yzoom), "Cairo's vertical zoom when scrolling");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_yzoom), V->S->do_zoom_y);
  g_signal_connect(G_OBJECT(T->checkbox_yzoom), "toggled", G_CALLBACK(on_checkbox_yzoom_toggled), (void *) V);

  /* checkbox_scale_radix */
  T->checkbox_scale_radix = gtk_check_button_new_with_label("Scale Node Length");
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->checkbox_scale_radix), "Scale node's lenth when scrolling");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_scale_radix), V->S->do_scale_radix);
  g_signal_connect(G_OBJECT(T->checkbox_scale_radix), "toggled", G_CALLBACK(on_checkbox_scale_radix_toggled), (void *) V);

  /* checkbox_scale_radius */
  T->checkbox_scale_radius = gtk_check_button_new_with_label("Scale Node Width");
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->checkbox_scale_radius), "Scale node's width when scrolling");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_scale_radius), V->S->do_scale_radius);
  g_signal_connect(G_OBJECT(T->checkbox_scale_radius), "toggled", G_CALLBACK(on_checkbox_scale_radius_toggled), (void *) V);

  /* combobox_cm */
  T->combobox_cm = gtk_combo_box_text_new();
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->combobox_cm), "When clicking a node");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_cm), "none", "None");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_cm), "info", "Info box");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_cm), "expand", "Expand/Collapse");
  gtk_combo_box_set_active(GTK_COMBO_BOX(T->combobox_cm), V->S->cm);
  g_signal_connect(G_OBJECT(T->combobox_cm), "changed", G_CALLBACK(on_combobox_cm_changed), (void *) V);
  
  /* combobox_hm */
  T->combobox_hm = gtk_combo_box_text_new();
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->combobox_hm), "When hovering a node");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_hm), "none", "None");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_hm), "info", "Info box");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_hm), "expand", "Expand");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_hm), "collapse", "Colapse");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_hm), "expcoll", "Expand/Collapse");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_hm), "remark", "Remark similar nodes");
  gtk_combo_box_set_active(GTK_COMBO_BOX(T->combobox_hm), V->S->hm);
  g_signal_connect(G_OBJECT(T->combobox_hm), "changed", G_CALLBACK(on_combobox_hm_changed), (void *) V);

  /* combobox_azf */
  T->combobox_azf = gtk_combo_box_text_new();
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->combobox_azf), "Auto zoom DAG fitly");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_azf), "none", "None");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_azf), "hor", "Horizontal");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_azf), "ver", "Vertical");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_azf), "based", "based on view type");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(T->combobox_azf), "full", "Full");  
  gtk_combo_box_set_active(GTK_COMBO_BOX(T->combobox_azf), V->S->auto_zoomfit);
  g_signal_connect(G_OBJECT(T->combobox_azf), "changed", G_CALLBACK(on_combobox_azf_changed), (void *) V);

  /* checkbox_azf */
  T->checkbox_azf = gtk_check_button_new();
  gtk_widget_set_tooltip_text(GTK_WIDGET(T->checkbox_scale_radius), "Auto adjust auto zoom fit based on context");
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_azf), V->S->adjust_auto_zoomfit);
  g_signal_connect(G_OBJECT(T->checkbox_azf), "toggled", G_CALLBACK(on_checkbox_azf_toggled), (void *) V);
}

GtkWidget *
dv_view_toolbox_get_combobox_lt(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->combobox_lt)) {
    GtkWidget * combobox_lt = T->combobox_lt = gtk_combo_box_text_new();
    gtk_widget_set_tooltip_text(GTK_WIDGET(combobox_lt), "How to layout nodes");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "dag", "DAG");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "dagboxlog", "DAG with timing (log scale)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "dagboxpower", "DAG with timing (power scale)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "dagboxlinear", "DAG with timing (linear scale)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "timeline", "Horizontal timeline");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "timelinev", "Vertical timeline");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "paraprof", "Parallelism profile");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_lt), "criticalpath", "Critical path");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_lt), T->V->S->lt);
    g_signal_connect(G_OBJECT(combobox_lt), "changed", G_CALLBACK(on_combobox_lt_changed), (void *) T->V);
  }
  return T->combobox_lt;
}

GtkWidget *
dv_view_toolbox_get_combobox_nc(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->combobox_nc)) {
    GtkWidget * combobox_nc = T->combobox_nc = gtk_combo_box_text_new();
    gtk_widget_set_tooltip_text(GTK_WIDGET(combobox_nc), "How to color nodes");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "worker", "Worker");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "cpu", "CPU");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "kind", "Node kind");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "code_start", "Code start");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "code_end", "Code end");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_nc), "code_start_end", "Code start-end pair");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_nc), T->V->S->nc);
    g_signal_connect(G_OBJECT(combobox_nc), "changed", G_CALLBACK(on_combobox_nc_changed), (void *) T->V);
  }
  return T->combobox_nc;
}

/*
GtkWidget *
dv_view_toolbox_get_combobox_sdt(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->combobox_sdt)) {
    GtkWidget * combobox_sdt = T->combobox_sdt = gtk_combo_box_text_new();
    gtk_widget_set_tooltip_text(GTK_WIDGET(combobox_sdt), "How to scale down nodes' length");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_sdt), "log", "Log");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_sdt), "power", "Power");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_sdt), "linear", "Linear");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_sdt), T->V->D->sdt);
    g_signal_connect(G_OBJECT(combobox_sdt), "changed", G_CALLBACK(on_combobox_sdt_changed), (void *) T->V);
  }
  return T->combobox_sdt;
}
*/

GtkWidget *
dv_view_toolbox_get_entry_radix(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->entry_radix)) {
    GtkWidget * entry_radix = T->entry_radix = gtk_entry_new();
    gtk_widget_set_tooltip_text(GTK_WIDGET(entry_radix), "Radix of the scale-down function");
    g_signal_connect(G_OBJECT(entry_radix), "activate", G_CALLBACK(on_entry_radix_activate), (void *) T->V);
  }
  dv_view_set_entry_radix_text(T->V);
  return T->entry_radix;
}

GtkWidget *
dv_view_toolbox_get_combobox_frombt(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->combobox_frombt)) {
    GtkWidget * combobox_frombt = T->combobox_frombt = gtk_combo_box_text_new();
    gtk_widget_set_tooltip_text(GTK_WIDGET(combobox_frombt), "Scale down overall from start time (must for timeline)");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_frombt), "not", "No");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_frombt), "frombt", "Yes");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_frombt), T->V->D->frombt);
    g_signal_connect(G_OBJECT(combobox_frombt), "changed", G_CALLBACK(on_combobox_frombt_changed), (void *) T->V);
  }
  return T->combobox_frombt;
}

GtkWidget *
dv_view_toolbox_get_combobox_et(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->combobox_et)) {
    GtkWidget * combobox_et = T->combobox_et = gtk_combo_box_text_new();
    gtk_widget_set_tooltip_text(GTK_WIDGET(combobox_et), "How to draw edges");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_et), "none", "None");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_et), "straight", "Straight");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_et), "down", "Down");
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox_et), "winding", "Winding");
    gtk_combo_box_set_active(GTK_COMBO_BOX(combobox_et), T->V->S->et);
    g_signal_connect(G_OBJECT(combobox_et), "changed", G_CALLBACK(on_combobox_et_changed), (void *) T->V);
  }
  return T->combobox_et;
}

GtkWidget *
dv_view_toolbox_get_togg_eaffix(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->togg_eaffix) || gtk_widget_get_parent(T->togg_eaffix)) {
    GtkWidget * togg_eaffix = T->togg_eaffix = gtk_toggle_button_new_with_label("Edge Affix");
    gtk_widget_set_tooltip_text(GTK_WIDGET(togg_eaffix), "Add a short line segment btwn edges & nodes");
    if (T->V->S->edge_affix == 0)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(togg_eaffix), FALSE);
    else
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(togg_eaffix), TRUE);
    g_signal_connect(G_OBJECT(togg_eaffix), "toggled", G_CALLBACK(on_togg_eaffix_toggled), (void *) T->V);
  }
  return T->togg_eaffix;
}

GtkWidget *
dv_view_toolbox_get_checkbox_legend(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->checkbox_legend)) {
    T->checkbox_legend = gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_legend), T->V->S->show_legend);
    g_signal_connect(G_OBJECT(T->checkbox_legend), "toggled", G_CALLBACK(on_checkbox_legend_toggled), (void *) T->V);
  }
  return T->checkbox_legend;
}

GtkWidget *
dv_view_toolbox_get_checkbox_status(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->checkbox_status)) {
    T->checkbox_status = gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_status), T->V->S->show_status);
    g_signal_connect(G_OBJECT(T->checkbox_status), "toggled", G_CALLBACK(on_checkbox_status_toggled), (void *) T->V);
  }
  return T->checkbox_status;
}

GtkWidget *
dv_view_toolbox_get_entry_remark(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->entry_remark)) {
    T->entry_remark = gtk_entry_new();
    gtk_widget_set_tooltip_text(GTK_WIDGET(T->entry_remark), "All ID(s) (e.g. worker numbers) to remark");
    gtk_entry_set_placeholder_text(GTK_ENTRY(T->entry_remark), "e.g. 0, 1, 2");
    g_signal_connect(G_OBJECT(T->entry_remark), "activate", G_CALLBACK(on_entry_remark_activate), (void *) T->V);
  }
  return T->entry_remark;
}

GtkWidget *
dv_view_toolbox_get_checkbox_remain_inner(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->checkbox_remain_inner)) {
    T->checkbox_remain_inner = gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_remain_inner), T->V->S->remain_inner);
    g_signal_connect(G_OBJECT(T->checkbox_remain_inner), "toggled", G_CALLBACK(on_checkbox_remain_inner_toggled), (void *) T->V);
  }
  return T->checkbox_remain_inner;
}

GtkWidget *
dv_view_toolbox_get_checkbox_color_remarked_only(dv_view_toolbox_t * T) {
  if (!GTK_IS_WIDGET(T->checkbox_color_remarked_only)) {
    T->checkbox_color_remarked_only = gtk_check_button_new();
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(T->checkbox_color_remarked_only), T->V->S->color_remarked_only);
    g_signal_connect(G_OBJECT(T->checkbox_color_remarked_only), "toggled", G_CALLBACK(on_checkbox_color_remarked_only_toggled), (void *) T->V);
  }
  return T->checkbox_color_remarked_only;
}

//gboolean
//on_view_toolbox_window_closed(_unused_ GtkWidget * widget, GdkEvent * event, gpointer user_data) {
//  dv_view_t * V = (dv_view_t *) user_data;
//  gtk_widget_hide(V->T->window);
//}

GtkWidget *
dv_view_toolbox_get_window(dv_view_toolbox_t * T) {
  if (T->window) {
    return T->window;
  }
  
  /* Build toolbox window */
  GtkWidget * window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  char s[DV_STRING_LENGTH];
  sprintf(s, "Toolbox of View %ld (DAG %ld)", T->V - DVG->V, T->V->D - DMG->D);
  gtk_window_set_title(GTK_WINDOW(window), s);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
  gtk_window_set_modal(GTK_WINDOW(window), 0);
  gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(GUI->window));
  g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
  GtkWidget * window_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(window), window_box);
  GtkWidget * notebook = gtk_notebook_new();
  gtk_box_pack_start(GTK_BOX(window_box), notebook, TRUE, TRUE, 0);

  GtkWidget * tab_label;
  GtkWidget * tab_box;

  /* Build tab "Common" */
  {
    tab_label = gtk_label_new("Common");
    tab_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_box, tab_label);
    gtk_container_set_border_width(GTK_CONTAINER(tab_box), 5);

    GtkWidget * grid = gtk_grid_new();
    gtk_box_pack_start(GTK_BOX(tab_box), grid, FALSE, FALSE, 0);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    int num;
    GtkWidget * label;
    GtkWidget * widget;

    num = 0;
    label = gtk_label_new("View type:");
    widget = dv_view_toolbox_get_combobox_lt(T);
    //gtk_widget_set_hexpand(GTK_WIDGET(label), TRUE);
    //gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_RIGHT);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
    
    num++;
    label = gtk_label_new("Node color:");
    widget = dv_view_toolbox_get_combobox_nc(T);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    /*
    num++;
    label = gtk_label_new("Scale-down type:");
    widget = dv_view_toolbox_get_combobox_sdt(T);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
    */

    num++;
    label = gtk_label_new("Scale-down radix:");
    widget = dv_view_toolbox_get_entry_radix(T);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("Scale radix by scrolling:");
    widget = T->checkbox_scale_radix;
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("Clicking mode:");
    widget = T->combobox_cm;
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("Hovering mode:");
    widget = T->combobox_hm;
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("Show legend:");
    widget = dv_view_toolbox_get_checkbox_legend(T);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("Show status:");
    widget = dv_view_toolbox_get_checkbox_status(T);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("Automatically zoom fit DAG:");
    widget = T->combobox_azf;
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("Auto adjust auto zoom fit:");
    widget = T->checkbox_azf;
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("Zoom fit DAG horizontally:");
    widget = gtk_button_new_with_label("Zoom");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_widget_set_tooltip_text(GTK_WIDGET(widget), "Zoom fit horizontally (H):");
    g_signal_connect(G_OBJECT(widget), "clicked", G_CALLBACK(on_btn_zoomfit_hor_clicked), (void *) T->V);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("Zoom fit DAG vertically:");
    widget = gtk_button_new_with_label("Zoom");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_widget_set_tooltip_text(GTK_WIDGET(widget), "Zoom fit vertically (V)");
    g_signal_connect(G_OBJECT(widget), "clicked", G_CALLBACK(on_btn_zoomfit_ver_clicked), (void *) T->V);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("Parallelism profile's resolution:");
    widget = T->entry_paraprof_min_interval = gtk_entry_new();
    gtk_widget_set_tooltip_text(GTK_WIDGET(widget), "Minimum entry interval on parallelism profile (unit: processor cycles)");
    g_signal_connect(G_OBJECT(widget), "activate", G_CALLBACK(on_entry_paraprof_resolution_activate), (void *) T->V);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
    dv_view_set_entry_paraprof_resolution(T->V);
  }
  
  /* Build tab "Advance" */
  {
    tab_label = gtk_label_new("Advance");
    tab_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_box, tab_label);
    gtk_container_set_border_width(GTK_CONTAINER(tab_box), 5);

    GtkWidget * grid = gtk_grid_new();
    gtk_box_pack_start(GTK_BOX(tab_box), grid, FALSE, FALSE, 0);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    int num;
    GtkWidget * label;
    GtkWidget * widget;

    num = 0;
    label = gtk_label_new("Remark:");
    widget = dv_view_toolbox_get_entry_remark(T);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("Remain inner after scanning:");
    widget = dv_view_toolbox_get_checkbox_remain_inner(T);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("Scan DAG:");
    widget = gtk_button_new_with_label("Scan:");
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    g_signal_connect(G_OBJECT(widget), "clicked", G_CALLBACK(on_btn_run_dag_scan_clicked), (void *) T->V);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("Color only remarked nodes:");
    widget = dv_view_toolbox_get_checkbox_color_remarked_only(T);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);    
  }
  
  /* Build tab "Developer" */
  {
    tab_label = gtk_label_new("Developer");
    tab_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_box, tab_label);
    gtk_container_set_border_width(GTK_CONTAINER(tab_box), 5);

    GtkWidget * grid = gtk_grid_new();
    gtk_box_pack_start(GTK_BOX(tab_box), grid, FALSE, FALSE, 0);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    int num;
    GtkWidget * label;
    GtkWidget * widget;

    num = 0;
    label = gtk_label_new("Cairo's X Zoom:");
    widget = T->checkbox_xzoom;
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("Cairo's Y Zoom:");
    widget = T->checkbox_yzoom;
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("Scale Node Width by Scrolling:");
    widget = T->checkbox_scale_radius;
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);    

    num++;
    label = gtk_label_new("Search:");
    widget = T->entry_search;
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("From start time:");
    widget = dv_view_toolbox_get_combobox_frombt(T);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("Edge drawing type:");
    widget = dv_view_toolbox_get_combobox_et(T);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);

    num++;
    label = gtk_label_new("Affix btwn edges & nodes:");
    widget = dv_view_toolbox_get_togg_eaffix(T);
    gtk_widget_set_halign(label, GTK_ALIGN_END);
    gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  }

  T->window = window;
  return T->window;
}


void
dv_view_init(dv_view_t * V) {
  V->name = malloc( 10 * sizeof(char) );
  sprintf(V->name, "View %ld", V - DVG->V);
  V->D = NULL;
  dv_view_status_init(V, V->S);
  int i;
  for (i = 0; i < DV_MAX_VIEWPORT; i++)
    V->mVP[i] = 0;
  V->mainVP = NULL;
  dv_view_toolbox_init(V->T, V);  
}

dv_view_t *
dv_view_create_new_with_dag(dm_dag_t * D) {
  /* Allocate and Initialize */
  if (DVG->nV >= DV_MAX_VIEW) {
    fprintf(stderr, "Error: too many Views (%d>=%d).\n", DVG->nV, DV_MAX_VIEW);
    return NULL;
  }
  dv_view_t * V = &DVG->V[DVG->nV++];
  dv_view_init(V);

  /* Set values */
  V->D = D;
  dv_dag_t * g = (dv_dag_t *) D->g;
  g->nviews[V->S->lt]++;
  g->mV[V - DVG->V] = 1;
  
  return V;
}

void
dv_view_open_toolbox_window(dv_view_t * V) {
  GtkWidget * toolbox_window = dv_view_toolbox_get_window(V->T);
  gtk_widget_show_all(toolbox_window);
}

void
dv_view_change_mainvp(dv_view_t * V, dv_viewport_t * VP) {
  if (V->mainVP == VP)
    return;
  V->mainVP = VP;
  if (!VP)
    return;
  V->S->vpw = VP->vpw;
  V->S->vph = VP->vph;
  V->S->do_zoomfit = 1;
} 

void
dv_view_add_viewport(dv_view_t * V, dv_viewport_t * VP) {
  int idx = VP - DVG->VP;
  if (V->mVP[idx])
    return;
  V->mVP[idx] = 1;
  V->S->nviewports++;
  dv_viewport_add_view(VP, V);
  if (!V->mainVP)
    dv_view_change_mainvp(V, VP);
}

void
dv_view_remove_viewport(dv_view_t * V, dv_viewport_t * VP) {
  int idx = VP - DVG->VP;
  if (!V->mVP[idx])
    return;
  V->mVP[idx] = 0;
  V->S->nviewports--;
  dv_viewport_remove_view(VP, V);
  if (V->mainVP == VP) {
    dv_viewport_t * new_vp = NULL;
    int i;
    for (i = 0; i < DVG->nVP; i++)
      if (V->mVP[i]) {
        new_vp = &DVG->VP[i];
        break;
      }
    dv_view_change_mainvp(V, new_vp);
  }
}

void
dv_view_switch_viewport(dv_view_t * V, dv_viewport_t * VP1, dv_viewport_t * VP2) {
  int sw = 0;
  //fprintf(stderr, "mainvp:%ld, vp1:%ld\n", V->mainVP - DVG->VP, VP1 - DVG->VP);
  if (V->mainVP == VP1)
    sw = 1;
  dv_view_remove_viewport(V, VP1);
  dv_view_add_viewport(V, VP2);
  if (sw)
    dv_view_change_mainvp(V, VP2);
}

/*-----------------end of VIEW's functions-----------------*/



/*-----------------VIEWPORT's functions-----------------*/

dv_viewport_t *
dv_viewport_create_new() {
  /* Get new Viewport */
  if (DVG->nVP >= DV_MAX_VIEWPORT) {
    fprintf(stderr, "Error: too many Viewports (%d>=%d).\n", DVG->nVP, DV_MAX_VIEWPORT);
    return NULL;
  }
  dv_viewport_t * VP = &DVG->VP[DVG->nVP++];
  return VP;
}

void
dv_viewport_toolbox_init(dv_viewport_t * VP) {
  char s[DV_STRING_LENGTH];
  sprintf(s, "DAG %ld", VP->mainV - DVG->V);

  // White color
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");

  // Toolbar
  GtkWidget * toolbar = VP->T->toolbar = gtk_toolbar_new();
  g_object_ref(toolbar);
  //gtk_widget_override_background_color(GTK_WIDGET(toolbar), GTK_STATE_FLAG_NORMAL, white);

  /*
  GtkListStore * list_store = gtk_list_store_new(2, G_TYPE_STRING, G_TYPE_STRING);
  GtkTreeIter iter;
  gtk_list_store_append(list_store, &iter);
  gtk_list_store_set(list_store, &iter,
                     0, "abc",
                     -1);
  gtk_list_store_append(list_store, &iter);
  gtk_list_store_set(list_store, &iter,
                     1, "xyz",
                     -1);
  GtkWidget * tree_view = gtk_tree_view_new_with_model(GTK_TREE_MODEL(list_store));

  GtkToolItem * btn_views = gtk_tool_item_new();
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_views, -1);
  GtkWidget * combobox_views = gtk_combo_box_new_with_model(GTK_TREE_MODEL(list_store));
  gtk_container_add(GTK_CONTAINER(btn_views), combobox_views);
  //gtk_combo_box_set_model(GTK_COMBO_BOX(combobox_views), GTK_TREE_MODEL(list_store));
  */
  
  // Focused toggle
  /*
  GtkToolItem * btn_togg_focused = gtk_tool_item_new();
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_togg_focused, -1);
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_togg_focused), "Indicates if this VIEW is focused for hotkeys (use tab key to change btwn VIEWs)");
  GtkWidget * togg_focused = VP->T->togg_focused = gtk_toggle_button_new_with_label(s);
  gtk_container_add(GTK_CONTAINER(btn_togg_focused), togg_focused);
  g_signal_connect(G_OBJECT(togg_focused), "toggled", G_CALLBACK(on_togg_focused_toggled), (void *) VP);
  */

  // Label
  GtkWidget * label = VP->T->label = gtk_label_new(s);
  //GtkWidget * btn_label = gtk_tool_item_new();
  //gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_label, -1);
  //gtk_container_add(GTK_CONTAINER(btn_label), label);
  gtk_widget_set_tooltip_text(GTK_WIDGET(label), "The DAG that is focused for e.g. hot keys (change by Tab)");

  // Separator
  //gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

  // View-attribute dialog button
  GtkToolItem *btn_attrs = gtk_tool_button_new(NULL, NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_attrs, -1);
  gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_attrs), "preferences-system-symbolic");
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_attrs), "Open dialog to adjust VIEW's attributes (A)");
  g_signal_connect(G_OBJECT(btn_attrs), "clicked", G_CALLBACK(on_viewport_tool_icon_clicked), (void *) VP);

  // Separator
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);

  // Zoomfit-full button
  GtkToolItem *btn_zoomfit_full = gtk_tool_button_new(NULL, NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoomfit_full, -1);
  gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_zoomfit_full), "zoom-fit-best");
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_zoomfit_full), "Fit full (F)");
  g_signal_connect(G_OBJECT(btn_zoomfit_full), "clicked", G_CALLBACK(on_btn_zoomfit_full_clicked), (void *) VP);

  // Shrink/Expand buttons
  GtkToolItem *btn_shrink = gtk_tool_button_new(NULL, NULL);
  GtkToolItem *btn_expand = gtk_tool_button_new(NULL, NULL);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_shrink, -1);
  gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_expand, -1);
  gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_shrink), "zoom-out");
  gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_expand), "zoom-in");
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_shrink), "Collapse one depth (C)"); 
  gtk_widget_set_tooltip_text(GTK_WIDGET(btn_expand), "Expand one depth (X)");
  g_signal_connect(G_OBJECT(btn_shrink), "clicked", G_CALLBACK(on_btn_shrink_clicked), (void *) VP);  
  g_signal_connect(G_OBJECT(btn_expand), "clicked", G_CALLBACK(on_btn_expand_clicked), (void *) VP);
}

void
override_background_color(GtkWidget * widget, GdkRGBA * rgba) {
  GtkCssProvider * provider = gtk_css_provider_new();
  gchar * css = g_strdup_printf ("* { background-color: %s; }", gdk_rgba_to_string (rgba));
  gtk_css_provider_load_from_data(provider, css, -1, NULL);
  g_free(css);
  gtk_style_context_add_provider(gtk_widget_get_style_context(widget),
                                 GTK_STYLE_PROVIDER (provider),
                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
  g_object_unref(provider);
}

void
dv_viewport_init(dv_viewport_t * VP) {
  char s[DV_STRING_LENGTH];
  sprintf(s, "Viewport %ld", VP - DVG->VP);
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");

  /* Split */
  VP->split = 0;

  /* Frame */
  VP->frame = gtk_frame_new(NULL);
  g_object_ref(G_OBJECT(VP->frame));

  /* Paned */
  VP->paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  g_object_ref(G_OBJECT(VP->paned));
  VP->orientation = GTK_ORIENTATION_HORIZONTAL;
  VP->vp1 = VP->vp2 = NULL;
  
  /* Box */
  VP->box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  g_object_ref(G_OBJECT(VP->box));
  gtk_container_add(GTK_CONTAINER(VP->frame), GTK_WIDGET(VP->box));
  
  /* Drawing Area */
  VP->darea = gtk_drawing_area_new();
  g_object_ref(G_OBJECT(VP->darea));
  gtk_box_pack_end(GTK_BOX(VP->box), VP->darea, TRUE, TRUE, 0);
  GtkWidget * darea = VP->darea;
  override_background_color(GTK_WIDGET(darea), white);
  gtk_widget_set_can_focus(darea, TRUE);
  gtk_widget_add_events(GTK_WIDGET(darea), GDK_SCROLL_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
  g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_darea_draw_event), (void *) VP);
  g_signal_connect(G_OBJECT(darea), "scroll-event", G_CALLBACK(on_darea_scroll_event), (void *) VP);
  g_signal_connect(G_OBJECT(darea), "button-press-event", G_CALLBACK(on_darea_button_event), (void *) VP);
  g_signal_connect(G_OBJECT(darea), "button-release-event", G_CALLBACK(on_darea_button_event), (void *) VP);
  g_signal_connect(G_OBJECT(darea), "motion-notify-event", G_CALLBACK(on_darea_motion_event), (void *) VP);
  g_signal_connect(G_OBJECT(darea), "configure-event", G_CALLBACK(on_darea_configure_event), (void *) VP);
  g_signal_connect(G_OBJECT(darea), "key-press-event", G_CALLBACK(on_darea_key_press_event), (void *) VP);
  
  /* VP <-> V */
  int i;
  for (i = 0; i < DV_MAX_VIEW; i++)
    VP->mV[i] = 0;
  
  /* width, height */
  VP->vpw = VP->vph = 0.0;

  /* Viewport toolbox */
  dv_viewport_toolbox_init(VP);
  //gtk_box_pack_start(GTK_BOX(VP->box), VP->T->toolbar, FALSE, FALSE, 0);

  
  /* Management window */

  /* Mini frame */
  VP->mini_frame = gtk_frame_new(s);
  g_object_ref(G_OBJECT(VP->mini_frame));
  gtk_container_set_border_width(GTK_CONTAINER(VP->mini_frame), 5);
  VP->mini_paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
  g_object_ref(G_OBJECT(VP->mini_paned));
  gtk_container_set_border_width(GTK_CONTAINER(VP->mini_paned), 5);

  /* Mini frame 2 */
  VP->mini_frame_2 = gtk_frame_new(s);
  g_object_ref(G_OBJECT(VP->mini_frame_2));
  gtk_container_set_border_width(GTK_CONTAINER(VP->mini_frame_2), 5);
  VP->mini_frame_2_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  g_object_ref(G_OBJECT(VP->mini_frame_2_box));
  gtk_container_add(GTK_CONTAINER(VP->mini_frame_2), VP->mini_frame_2_box);
  gtk_container_set_border_width(GTK_CONTAINER(VP->mini_frame_2_box), 5);
  
  GtkWidget * hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_pack_start(GTK_BOX(VP->mini_frame_2_box), hbox, FALSE, FALSE, 3);
  
  /* Split combobox */
  GtkWidget * split_combobox = VP->split_combobox = gtk_combo_box_text_new();
  gtk_box_pack_start(GTK_BOX(hbox), split_combobox, FALSE, FALSE, 0);
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(split_combobox), "nosplit", "No split");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(split_combobox), "split", "Split");
  gtk_combo_box_set_active(GTK_COMBO_BOX(split_combobox), VP->split);
  g_signal_connect(G_OBJECT(split_combobox), "changed", G_CALLBACK(on_management_window_viewport_options_split_changed), (void *) VP);

  /* Orientation combobox */
  GtkWidget * orient_combobox = VP->orient_combobox = gtk_combo_box_text_new();
  gtk_box_pack_start(GTK_BOX(hbox), orient_combobox, FALSE, FALSE, 0);
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(orient_combobox), "horizontal", "Horizontally");
  gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(orient_combobox), "vertical", "Vertically");
  gtk_combo_box_set_active(GTK_COMBO_BOX(orient_combobox), VP->orientation);
  g_signal_connect(G_OBJECT(orient_combobox), "changed", G_CALLBACK(on_management_window_viewport_options_orientation_changed), (void *) VP);

  /* DAG menubutton */
  {
    GtkWidget * dag_menubutton = VP->dag_menubutton = gtk_menu_button_new();
    gtk_box_pack_start(GTK_BOX(hbox), dag_menubutton, FALSE, FALSE, 0);
    GtkWidget * dag_menu = VP->dag_menu = gtk_menu_new();
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(dag_menubutton), dag_menu);

    int i, j;
    for (i = 0; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      dv_view_t * V = NULL;
      int c = 0;
      for (j = 0; j < DVG->nV; j++)
        if (((dv_dag_t*)D->g)->mV[j]) {
          c++;
          V = &DVG->V[j];        
        }
      if (c == 1) {
        GtkWidget * item = gtk_check_menu_item_new_with_label(D->name);
        gtk_menu_shell_append(GTK_MENU_SHELL(dag_menu), item);
        if (VP->mV[V - DVG->V])
          gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
        g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(on_management_window_viewport_dag_menu_item_toggled), (void *) VP);
      } else if (c > 1) {
        GtkWidget * item = gtk_menu_item_new_with_label(D->name);
        gtk_menu_shell_append(GTK_MENU_SHELL(dag_menu), item);
        GtkWidget * submenu = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
        for (j = 0; j < DVG->nV; j++)
          if (((dv_dag_t*)D->g)->mV[j]) {
            item = gtk_check_menu_item_new_with_label(DVG->V[j].name);
            gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
            if (VP->mV[j])
              gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
            g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(on_management_window_viewport_dag_menu_item_toggled), (void *) VP);
          }
      }
    }
    
    gtk_widget_show_all(dag_menu);
  }
  
}

void
dv_viewport_miniver2_show(dv_viewport_t * VP) {
  GList * children = gtk_container_get_children(GTK_CONTAINER(VP->mini_frame_2_box));
  GList * child = children;
  int exist_1 = 0;
  int exist_2 = 0;
  while (child) {
    GtkWidget * widget = GTK_WIDGET(child->data);
    if (VP->vp1 && widget == VP->vp1->mini_frame_2)
      exist_1 = 1;
    if (VP->vp2 && widget == VP->vp2->mini_frame_2)
      exist_2 = 1;
    child = child->next;
  }
  if (VP->split) {
    gtk_widget_set_sensitive(GTK_WIDGET(VP->orient_combobox), TRUE);
    gtk_widget_set_sensitive(GTK_WIDGET(VP->dag_menubutton), FALSE);
    if (!exist_1)
      gtk_box_pack_start(GTK_BOX(VP->mini_frame_2_box), VP->vp1->mini_frame_2, FALSE, FALSE, 0);
    if (!exist_2)
      gtk_box_pack_start(GTK_BOX(VP->mini_frame_2_box), VP->vp2->mini_frame_2, FALSE, FALSE, 0);
    dv_viewport_miniver2_show(VP->vp1);
    dv_viewport_miniver2_show(VP->vp2);
  } else {
    gtk_widget_set_sensitive(GTK_WIDGET(VP->orient_combobox), FALSE);
    gtk_widget_set_sensitive(GTK_WIDGET(VP->dag_menubutton), TRUE);
    if (exist_1)
      gtk_container_remove(GTK_CONTAINER(VP->mini_frame_2_box), VP->vp1->mini_frame_2);
    if (exist_2)
      gtk_container_remove(GTK_CONTAINER(VP->mini_frame_2_box), VP->vp2->mini_frame_2);
  }
}

void
dv_viewport_miniver_show(dv_viewport_t * VP) {
  GtkWidget * child = gtk_bin_get_child(GTK_BIN(VP->mini_frame));
  if (VP->split) {
    if (!child)
      gtk_container_add(GTK_CONTAINER(VP->mini_frame), VP->mini_paned);
    else
      dv_check(child == VP->mini_paned);
    if (VP->vp1) {
      gtk_paned_pack1(GTK_PANED(VP->mini_paned), VP->vp1->mini_frame, TRUE, TRUE);
      dv_viewport_miniver_show(VP->vp1);
    }
    if (VP->vp2) {
      gtk_paned_pack2(GTK_PANED(VP->mini_paned), VP->vp2->mini_frame, TRUE, TRUE);
      dv_viewport_miniver_show(VP->vp2);
    }
  } else {
    if (child)
      gtk_container_remove(GTK_CONTAINER(VP->mini_frame), child);    
  }
}

void
dv_viewport_show(dv_viewport_t * VP) {
  GtkWidget * child = gtk_bin_get_child(GTK_BIN(VP->frame));
  if (VP->split) {
    if (!child)
      gtk_container_add(GTK_CONTAINER(VP->frame), VP->paned);
    else if (child != VP->paned) {
      gtk_container_remove(GTK_CONTAINER(VP->frame), child);
      gtk_container_add(GTK_CONTAINER(VP->frame), VP->paned);
    }
    // Child 1
    if (VP->vp1) {
      gtk_paned_pack1(GTK_PANED(VP->paned), VP->vp1->frame, TRUE, TRUE);
      dv_viewport_show(VP->vp1);
    }
    // Child 2
    if (VP->vp2) {
      gtk_paned_pack2(GTK_PANED(VP->paned), VP->vp2->frame, TRUE, TRUE);
      dv_viewport_show(VP->vp2);
    }
  } else {
    if (child != VP->box) {
      if (child) gtk_container_remove(GTK_CONTAINER(VP->frame), child);
      gtk_container_add(GTK_CONTAINER(VP->frame), VP->box);
    }
  }
}

void
dv_viewport_change_split(dv_viewport_t * VP, int split) {
  if (split == VP->split) return;

  /* Real one */
  if (split) {
    // Check children's existence
    if (!VP->vp1) {
      VP->vp1 = dv_viewport_create_new();
      if (VP->vp1)
        dv_viewport_init(VP->vp1);
    }
    int i;
    for (i=0; i<DVG->nV; i++)
      if (VP->mV[i]) {
        dv_view_switch_viewport(&DVG->V[i], VP, VP->vp1);
      }
    if (!VP->vp2) {
      VP->vp2 = dv_viewport_create_new();
      if (VP->vp2)
        dv_viewport_init(VP->vp2);
    }
    /* switch active VP */
    if (DVG->activeVP == VP)
      DVG->activeVP = VP->vp1;
  } else {
    dv_viewport_change_split(VP->vp1, 0);
    dv_viewport_change_split(VP->vp2, 0);
    int i;
    for (i=0; i<DVG->nV; i++) {
      if (VP->vp1->mV[i]) {
        dv_view_switch_viewport(&DVG->V[i], VP->vp1, VP);
      }
      if (VP->vp2->mV[i]) {
        dv_view_switch_viewport(&DVG->V[i], VP->vp2, VP);
      }
    }
    /* switch active VP */
    if (DVG->activeVP == VP->vp1 || DVG->activeVP == VP->vp2)
      DVG->activeVP = VP;
  }

  /* Update widgets */
  if (VP->split_combobox)
    gtk_combo_box_set_active(GTK_COMBO_BOX(VP->split_combobox), split);

  VP->split = split;
  dv_viewport_show(VP);
  dv_viewport_miniver_show(VP);
  dv_viewport_miniver2_show(VP);
}

void
dv_viewport_change_orientation(dv_viewport_t * VP, GtkOrientation o) {
  if (o == VP->orientation) return;
  GtkWidget * child1 = NULL;
  GtkWidget * child2 = NULL;
  
  /* Real one */
  if (VP->paned) {
    child1 = gtk_paned_get_child1(GTK_PANED(VP->paned));
    if (child1)
      gtk_container_remove(GTK_CONTAINER(VP->paned), child1);
    child2 = gtk_paned_get_child2(GTK_PANED(VP->paned));
    if (child2)
      gtk_container_remove(GTK_CONTAINER(VP->paned), child2);
    gtk_widget_destroy(GTK_WIDGET(VP->paned));
  }
  VP->paned = gtk_paned_new(o);
  g_object_ref(VP->paned);
  
  /* Mini */
  if (VP->mini_paned) {
    child1 = gtk_paned_get_child1(GTK_PANED(VP->mini_paned));
    if (child1)
      gtk_container_remove(GTK_CONTAINER(VP->mini_paned), child1);
    child2 = gtk_paned_get_child2(GTK_PANED(VP->mini_paned));
    if (child2)
      gtk_container_remove(GTK_CONTAINER(VP->mini_paned), child2);
    gtk_widget_destroy(GTK_WIDGET(VP->mini_paned));
  }
  VP->mini_paned = gtk_paned_new(o);
  g_object_ref(VP->mini_paned);
  gtk_container_set_border_width(GTK_CONTAINER(VP->mini_paned), 5);
  
  /* Update widgets */
  if (VP->orient_combobox)
    gtk_combo_box_set_active(GTK_COMBO_BOX(VP->orient_combobox), o);

  VP->orientation = o;
  dv_viewport_show(VP);
  dv_viewport_miniver_show(VP);
  dv_viewport_miniver2_show(VP);
}

void
dv_viewport_change_mainv(dv_viewport_t * VP, dv_view_t * V) {
  if (VP->mainV == V)
    return;
  VP->mainV = V;
  char s[DV_STRING_LENGTH];
  if (!V) {
    sprintf(s, "None");
  } else {
    sprintf(s, "DAG %ld", V - DVG->V);
    //V->S->do_zoomfit = 1;
  }
  gtk_label_set_text(GTK_LABEL(VP->T->label), s);
} 

static void
dv_viewport_dag_menu_item_set_active(dv_viewport_t * VP, dv_view_t * V, gboolean active) {
  dm_dag_t * D = V->D;
  /* Seek D's item */
  GList * children = gtk_container_get_children(GTK_CONTAINER(VP->dag_menu));
  GList * child = children;
  while (child) {
    GtkWidget * widget = GTK_WIDGET(child->data);
    const gchar * str = gtk_menu_item_get_label(GTK_MENU_ITEM(widget));
    if (strcmp(str, D->name) == 0) {
      GtkWidget * submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget));
      if (!submenu) {
        gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), active);
      } else {
        /* Seek V's item */
        GList * children = gtk_container_get_children(GTK_CONTAINER(submenu));
        GList * child = children;
        while (child) {
          GtkWidget * widget = GTK_WIDGET(child->data);
          const gchar * str = gtk_menu_item_get_label(GTK_MENU_ITEM(widget));
          if (strcmp(str, V->name) == 0) {
            gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(widget), active);
            break;
          }
          child = child->next;
        }
      }
      break;
    }
    child = child->next;
  }
  gtk_widget_queue_draw(VP->dag_menu);
}

void
dv_viewport_add_view(dv_viewport_t * VP, dv_view_t * V) {
  int idx = V - DVG->V;
  VP->mV[idx] = 1;
  if (!VP->mainV)
    dv_viewport_change_mainv(VP, V);
  /* Update widgets */
  dv_viewport_dag_menu_item_set_active(VP, V, TRUE);
  /* Redraw viewports */
  gtk_widget_queue_draw(GTK_WIDGET(VP->frame));
}

void
dv_viewport_remove_view(dv_viewport_t * VP, dv_view_t * V) {
  int idx = V - DVG->V;
  dv_check(VP->mV[idx]);
  VP->mV[idx] = 0;
  if (VP->mainV == V) {
    dv_view_t * new_main_v = NULL;
    int i;
    for (i = 0; i < DVG->nV; i++)
      if (VP->mV[i]) {
        new_main_v = &DVG->V[i];
        break;
      }
    dv_viewport_change_mainv(VP, new_main_v);
  }
  /* Update widgets */
  dv_viewport_dag_menu_item_set_active(VP, V, FALSE);
  /* Redraw viewports */
  gtk_widget_queue_draw(GTK_WIDGET(VP->frame));
}


/* Divisions for one DAG: D , T , D|T , D/T , (D|Dlog)/T, D|Dlog, (Dpower|Dlinear)/T  */
void
dv_viewport_divide_onedag_1(dv_viewport_t * VP, dm_dag_t * D) {
  /* D */
  /* View */
  dv_view_t * V = NULL;
  int i;
  for (i = 0; i < DVG->nV; i++)
    if (((dv_dag_t*)D->g)->mV[i]) {
      V = &DVG->V[i];
      break;
    }
  if (!V)
    V = dv_create_new_view(D);
  dv_view_change_lt(V, DV_LAYOUT_TYPE_DAG);
  
  /* Viewport */
  dv_viewport_change_split(VP, 0);
  for (i = 0; i < DVG->nV; i++)
    if (VP->mV[i]) {
      dv_view_t * V = &DVG->V[i];
      dv_view_remove_viewport(V, VP);
    }
  dv_view_add_viewport(V, VP);
  dv_view_change_mainvp(V, VP);
}

void
dv_viewport_divide_onedag_2(dv_viewport_t * VP, dm_dag_t * D) {
  /* T */
  /* View */
  dv_view_t * V = NULL;
  int i;
  for (i = 0; i < DVG->nV; i++)
    if (((dv_dag_t*)D->g)->mV[i]) {
      V = &DVG->V[i];
      break;
    }
  if (!V)
    V = dv_create_new_view(D);
  dv_view_change_lt(V, DV_LAYOUT_TYPE_PARAPROF);
  
  /* Viewport */
  dv_viewport_change_split(VP, 0);
  for (i = 0; i < DVG->nV; i++)
    if (VP->mV[i]) {
      dv_view_t * V = &DVG->V[i];
      dv_view_remove_viewport(V, VP);
    }
  dv_view_add_viewport(V, VP);
  dv_view_change_mainvp(V, VP);
}

void
dv_viewport_divide_onedag_3(dv_viewport_t * VP, dm_dag_t * D) {
  /* D | T */
  /* View */
  dv_view_t * V1 = NULL;
  dv_view_t * V2 = NULL;
  int i;
  for (i = 0; i < DVG->nV; i++)
    if (((dv_dag_t*)D->g)->mV[i]) {
      if (!V1) V1 = &DVG->V[i];
      else if (!V2) {
        V2 = &DVG->V[i];
        break;
      }
    }
  if (!V1)
    V1 = dv_create_new_view(D);
  if (!V2)
    V2 = dv_create_new_view(D);
  dv_view_change_lt(V1, DV_LAYOUT_TYPE_DAG);
  dv_view_change_lt(V2, DV_LAYOUT_TYPE_PARAPROF);
  
  /* Viewport */
  dv_viewport_change_split(VP, 1);
  dv_viewport_change_orientation(VP, GTK_ORIENTATION_HORIZONTAL);
  dv_viewport_t * VP1 = VP->vp1;
  dv_viewport_t * VP2 = VP->vp2;
  dv_viewport_change_split(VP1, 0);
  dv_viewport_change_split(VP2, 0);
  for (i = 0; i < DVG->nV; i++) {
    if (VP1->mV[i]) {
      dv_view_t * V = &DVG->V[i];
      dv_view_remove_viewport(V, VP1);
    }
    if (VP2->mV[i]) {
      dv_view_t * V = &DVG->V[i];
      dv_view_remove_viewport(V, VP2);
    }
  }
  dv_view_add_viewport(V1, VP1);
  dv_view_add_viewport(V2, VP2);
  dv_view_change_mainvp(V1, VP1);
  dv_view_change_mainvp(V2, VP2);
}

void
dv_viewport_divide_onedag_4(dv_viewport_t * VP, dm_dag_t * D) {
  /* D / T */
  /* View */
  dv_view_t * V1 = NULL;
  dv_view_t * V2 = NULL;
  int i;
  for (i = 0; i < DVG->nV; i++)
    if (((dv_dag_t*)D->g)->mV[i]) {
      if (!V1) V1 = &DVG->V[i];
      else if (!V2) {
        V2 = &DVG->V[i];
        break;
      }
    }
  if (!V1)
    V1 = dv_create_new_view(D);
  if (!V2)
    V2 = dv_create_new_view(D);
  dv_view_change_lt(V1, DV_LAYOUT_TYPE_DAG);
  dv_view_change_lt(V2, DV_LAYOUT_TYPE_PARAPROF);
  
  /* Viewport */
  dv_viewport_change_split(VP, 1);
  dv_viewport_change_orientation(VP, GTK_ORIENTATION_VERTICAL);
  dv_viewport_t * VP1 = VP->vp1;
  dv_viewport_t * VP2 = VP->vp2;
  dv_viewport_change_split(VP1, 0);
  dv_viewport_change_split(VP2, 0);
  for (i = 0; i < DVG->nV; i++) {
    if (VP1->mV[i]) {
      dv_view_t * V = &DVG->V[i];
      dv_view_remove_viewport(V, VP1);
    }
    if (VP2->mV[i]) {
      dv_view_t * V = &DVG->V[i];
      dv_view_remove_viewport(V, VP2);
    }
  }
  dv_view_add_viewport(V1, VP1);
  dv_view_add_viewport(V2, VP2);
  dv_view_change_mainvp(V1, VP1);
  dv_view_change_mainvp(V2, VP2);
}

void
dv_viewport_divide_onedag_5(dv_viewport_t * VP, dm_dag_t * D) {
  /* (D | B) / T */
  /* View */
  dv_view_t * V1 = NULL;
  dv_view_t * V2 = NULL;
  dv_view_t * V3 = NULL;
  int i;
  for (i = 0; i < DVG->nV; i++)
    if (((dv_dag_t*)D->g)->mV[i]) {
      if (!V1) V1 = &DVG->V[i];
      else if (!V2) V2 = &DVG->V[i];
      else if (!V3) {
        V3 = &DVG->V[i];
        break;
      }
    }
  if (!V1)
    V1 = dv_create_new_view(D);
  if (!V2)
    V2 = dv_create_new_view(D);
  if (!V3)
    V3 = dv_create_new_view(D);
  dv_view_change_lt(V1, DV_LAYOUT_TYPE_DAG);
  dv_view_change_lt(V2, DV_LAYOUT_TYPE_DAG_BOX_LOG);
  dv_view_change_lt(V3, DV_LAYOUT_TYPE_PARAPROF);
  
  /* Viewport */
  dv_viewport_t * VP1 = NULL;
  dv_viewport_t * VP2 = NULL;
  {
    dv_viewport_change_split(VP, 1);
    dv_viewport_change_orientation(VP, GTK_ORIENTATION_VERTICAL);
    VP1 = VP->vp1;
    VP2 = VP->vp2;
    dv_viewport_change_split(VP2, 0);
    for (i = 0; i < DVG->nV; i++) {
      if (VP1->mV[i]) {
        dv_view_t * V = &DVG->V[i];
        dv_view_remove_viewport(V, VP1);
      }
      if (VP2->mV[i]) {
        dv_view_t * V = &DVG->V[i];
        dv_view_remove_viewport(V, VP2);
      }
    }
    dv_view_add_viewport(V3, VP2);
    dv_view_change_mainvp(V3, VP2);
  }
  
  VP = VP1;
  {
    dv_viewport_change_split(VP, 1);
    dv_viewport_change_orientation(VP, GTK_ORIENTATION_HORIZONTAL);
    VP1 = VP->vp1;
    VP2 = VP->vp2;
    dv_viewport_change_split(VP1, 0);
    dv_viewport_change_split(VP2, 0);
    for (i = 0; i < DVG->nV; i++) {
      if (VP1->mV[i]) {
        dv_view_t * V = &DVG->V[i];
        dv_view_remove_viewport(V, VP1);
      }
      if (VP2->mV[i]) {
        dv_view_t * V = &DVG->V[i];
        dv_view_remove_viewport(V, VP2);
      }
    }
    dv_view_add_viewport(V1, VP1);
    dv_view_add_viewport(V2, VP2);
    dv_view_change_mainvp(V1, VP1);
    dv_view_change_mainvp(V2, VP2);
  }
}

void
dv_viewport_divide_onedag_6(dv_viewport_t * VP, dm_dag_t * D) {
  /* D | B */
  /* View */
  dv_view_t * V1 = NULL;
  dv_view_t * V2 = NULL;
  int i;
  for (i = 0; i < DVG->nV; i++)
    if (((dv_dag_t*)D->g)->mV[i]) {
      if (!V1) V1 = &DVG->V[i];
      else if (!V2) {
        V2 = &DVG->V[i];
        break;
      }
    }
  if (!V1)
    V1 = dv_create_new_view(D);
  if (!V2)
    V2 = dv_create_new_view(D);
  dv_view_change_lt(V1, DV_LAYOUT_TYPE_DAG);
  dv_view_change_lt(V2, DV_LAYOUT_TYPE_DAG_BOX_LOG);
  
  /* Viewport */
  dv_viewport_change_split(VP, 1);
  dv_viewport_change_orientation(VP, GTK_ORIENTATION_HORIZONTAL);
  dv_viewport_t * VP1 = VP->vp1;
  dv_viewport_t * VP2 = VP->vp2;
  dv_viewport_change_split(VP1, 0);
  dv_viewport_change_split(VP2, 0);
  for (i = 0; i < DVG->nV; i++) {
    if (VP1->mV[i]) {
      dv_view_t * V = &DVG->V[i];
      dv_view_remove_viewport(V, VP1);
    }
    if (VP2->mV[i]) {
      dv_view_t * V = &DVG->V[i];
      dv_view_remove_viewport(V, VP2);
    }
  }
  dv_view_add_viewport(V1, VP1);
  dv_view_add_viewport(V2, VP2);
  dv_view_change_mainvp(V1, VP1);
  dv_view_change_mainvp(V2, VP2);
}

void
dv_viewport_divide_onedag_7(dv_viewport_t * VP, dm_dag_t * D) {
  /* (D | B) / T */
  /* View */
  dv_view_t * V1 = NULL;
  dv_view_t * V2 = NULL;
  dv_view_t * V3 = NULL;
  int i;
  for (i = 0; i < DVG->nV; i++)
    if (((dv_dag_t*)D->g)->mV[i]) {
      if (!V1) V1 = &DVG->V[i];
      else if (!V2) V2 = &DVG->V[i];
      else if (!V3) {
        V3 = &DVG->V[i];
        break;
      }
    }
  if (!V1)
    V1 = dv_create_new_view(D);
  if (!V2)
    V2 = dv_create_new_view(D);
  if (!V3)
    V3 = dv_create_new_view(D);
  dv_view_change_lt(V1, DV_LAYOUT_TYPE_DAG);
  dv_view_change_lt(V2, DV_LAYOUT_TYPE_DAG_BOX_LINEAR);
  dv_view_change_lt(V3, DV_LAYOUT_TYPE_PARAPROF);
  
  /* Viewport */
  dv_viewport_t * VP1 = NULL;
  dv_viewport_t * VP2 = NULL;
  {
    dv_viewport_change_split(VP, 1);
    dv_viewport_change_orientation(VP, GTK_ORIENTATION_VERTICAL);
    VP1 = VP->vp1;
    VP2 = VP->vp2;
    dv_viewport_change_split(VP2, 0);
    for (i = 0; i < DVG->nV; i++) {
      if (VP1->mV[i]) {
        dv_view_t * V = &DVG->V[i];
        dv_view_remove_viewport(V, VP1);
      }
      if (VP2->mV[i]) {
        dv_view_t * V = &DVG->V[i];
        dv_view_remove_viewport(V, VP2);
      }
    }
    dv_view_add_viewport(V3, VP2);
    dv_view_change_mainvp(V3, VP2);
  }
  
  VP = VP1;
  {
    dv_viewport_change_split(VP, 1);
    dv_viewport_change_orientation(VP, GTK_ORIENTATION_HORIZONTAL);
    VP1 = VP->vp1;
    VP2 = VP->vp2;
    dv_viewport_change_split(VP1, 0);
    dv_viewport_change_split(VP2, 0);
    for (i = 0; i < DVG->nV; i++) {
      if (VP1->mV[i]) {
        dv_view_t * V = &DVG->V[i];
        dv_view_remove_viewport(V, VP1);
      }
      if (VP2->mV[i]) {
        dv_view_t * V = &DVG->V[i];
        dv_view_remove_viewport(V, VP2);
      }
    }
    dv_view_add_viewport(V1, VP1);
    dv_view_add_viewport(V2, VP2);
    dv_view_change_mainvp(V1, VP1);
    dv_view_change_mainvp(V2, VP2);
  }
}

/* Divisions for 2 DAGs: D|D , T|T , T/T , (D/T)|(D/T) , (D|T)/(D|T) , (D|B)/T | (D|B)/T */
void
dv_viewport_divide_twodags_1(dv_viewport_t * VP, dm_dag_t * D1, dm_dag_t * D2) {
  /* D | D */
  /* Viewport */
  dv_viewport_change_split(VP, 1);
  dv_viewport_change_orientation(VP, GTK_ORIENTATION_HORIZONTAL);
  dv_viewport_t * VP1 = VP->vp1;
  dv_viewport_t * VP2 = VP->vp2;
  dv_viewport_divide_onedag_1(VP1, D1);
  dv_viewport_divide_onedag_1(VP2, D2);
}

void
dv_viewport_divide_twodags_2(dv_viewport_t * VP, dm_dag_t * D1, dm_dag_t * D2) {
  /* T | T */
  /* Viewport */
  dv_viewport_change_split(VP, 1);
  dv_viewport_change_orientation(VP, GTK_ORIENTATION_HORIZONTAL);
  dv_viewport_t * VP1 = VP->vp1;
  dv_viewport_t * VP2 = VP->vp2;
  dv_viewport_divide_onedag_2(VP1, D1);
  dv_viewport_divide_onedag_2(VP2, D2);
}

void
dv_viewport_divide_twodags_3(dv_viewport_t * VP, dm_dag_t * D1, dm_dag_t * D2) {
  /* T / T */
  /* Viewport */
  dv_viewport_change_split(VP, 1);
  dv_viewport_change_orientation(VP, GTK_ORIENTATION_VERTICAL);
  dv_viewport_t * VP1 = VP->vp1;
  dv_viewport_t * VP2 = VP->vp2;
  dv_viewport_divide_onedag_2(VP1, D1);
  dv_viewport_divide_onedag_2(VP2, D2);
}

void
dv_viewport_divide_twodags_4(dv_viewport_t * VP, dm_dag_t * D1, dm_dag_t * D2) {
  /* (D / T) | (D / T) */
  /* Viewport */
  dv_viewport_change_split(VP, 1);
  dv_viewport_change_orientation(VP, GTK_ORIENTATION_HORIZONTAL);
  dv_viewport_t * VP1 = VP->vp1;
  dv_viewport_t * VP2 = VP->vp2;
  dv_viewport_divide_onedag_4(VP1, D1);
  dv_viewport_divide_onedag_4(VP2, D2);
}

void
dv_viewport_divide_twodags_5(dv_viewport_t * VP, dm_dag_t * D1, dm_dag_t * D2) {
  /* (D | T) / (D | T) */
  /* Viewport */
  dv_viewport_change_split(VP, 1);
  dv_viewport_change_orientation(VP, GTK_ORIENTATION_VERTICAL);
  dv_viewport_t * VP1 = VP->vp1;
  dv_viewport_t * VP2 = VP->vp2;
  dv_viewport_divide_onedag_3(VP1, D1);
  dv_viewport_divide_onedag_3(VP2, D2);
}

void
dv_viewport_divide_twodags_6(dv_viewport_t * VP, dm_dag_t * D1, dm_dag_t * D2) {
  /* (D | B) / T | (D | B) / T */
  /* Viewport */
  dv_viewport_change_split(VP, 1);
  dv_viewport_change_orientation(VP, GTK_ORIENTATION_HORIZONTAL);
  dv_viewport_t * VP1 = VP->vp1;
  dv_viewport_t * VP2 = VP->vp2;
  dv_viewport_divide_onedag_5(VP1, D1);
  dv_viewport_divide_onedag_5(VP2, D2);
}

void
dv_viewport_divide_twodags_7(dv_viewport_t * VP, dm_dag_t * D1, dm_dag_t * D2) {
  /* D | T */
  /* Viewport */
  dv_viewport_change_split(VP, 1);
  dv_viewport_change_orientation(VP, GTK_ORIENTATION_HORIZONTAL);
  dv_viewport_t * VP1 = VP->vp1;
  dv_viewport_t * VP2 = VP->vp2;
  dv_viewport_divide_onedag_1(VP1, D1);
  dv_viewport_divide_onedag_2(VP2, D2);
}

/* Divisions for 3 DAGs: D | D | D */
void
dv_viewport_divide_threedags_1(dv_viewport_t * VP, dm_dag_t * D1, dm_dag_t * D2, dm_dag_t * D3) {
  /* D | D | D*/
  /* Viewport */
  dv_viewport_change_split(VP, 1);
  dv_viewport_change_orientation(VP, GTK_ORIENTATION_HORIZONTAL);
  dv_viewport_t * VP1 = VP->vp1;
  dv_viewport_t * VP2 = VP->vp2;
  dv_viewport_divide_onedag_1(VP1, D1);
  dv_viewport_divide_twodags_1(VP2, D2, D3);
}

/*-----------------end of VIEWPORT's functions-----------------*/


void
dv_open_statistics_dialog() {
  /* Get default DAG */
  dv_view_t * V = DVG->activeV;
  if (!V) {
    V = &DVG->V[0];
  }
  
  /* Build dialog */
  GtkWidget * dialog = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog), "Statistics");
  //gtk_window_set_default_size(GTK_WINDOW(dialog), 600, -1);
  gtk_window_set_position(GTK_WINDOW(dialog), GTK_WIN_POS_MOUSE);
  GtkWidget * dialog_box = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
  gtk_container_set_border_width(GTK_CONTAINER(dialog_box), 5);
  GtkWidget * notebook = gtk_notebook_new();
  gtk_box_pack_start(GTK_BOX(dialog_box), notebook, TRUE, TRUE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(notebook), 5);
  gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook), TRUE);

  GtkWidget * tab_label;
  GtkWidget * tab_box;

  /* Build delay distribution tab */
  {
    tab_label = gtk_label_new("Delay Distributions");
    tab_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_box, tab_label);
    if (DVG->SD->ne == 0) {
      DVG->SD->ne = DMG->nP;
      if (DVG->SD->ne > DV_MAX_DISTRIBUTION)
        DVG->SD->ne = DV_MAX_DISTRIBUTION;
      if (DVG->SD->ne < 4)
        DVG->SD->ne = 4;
    }
    long i;
    for (i = 0; i < DVG->SD->ne; i++) {
      dv_stat_distribution_entry_t * e = &DVG->SD->e[i];
      GtkWidget * e_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_container_add(GTK_CONTAINER(tab_box), e_box);

      GtkWidget * combobox;
      /* dag */
      combobox = gtk_combo_box_text_new();
      gtk_box_pack_start(GTK_BOX(e_box), combobox, FALSE, FALSE, 0);
      gtk_widget_set_tooltip_text(GTK_WIDGET(combobox), "Choose DAG");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "none", "None");
      int j;
      for (j = 0; j < DMG->nD; j++) {
        char str[30];
        sprintf(str, "DAG %d", j);
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "dag", str);
      }
      gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), e->dag_id + 1);
      g_signal_connect(G_OBJECT(combobox), "changed", G_CALLBACK(on_stat_distribution_dag_changed), (void *) i);
      /* type */
      combobox = gtk_combo_box_text_new();
      gtk_box_pack_start(GTK_BOX(e_box), combobox, FALSE, FALSE, 0);
      gtk_widget_set_tooltip_text(GTK_WIDGET(combobox), "Choose delay type");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "spawn", "spawn");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "cont", "cont");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "sync", "sync");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "intervals", "intervals");
      gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), e->type);
      g_signal_connect(G_OBJECT(combobox), "changed", G_CALLBACK(on_stat_distribution_type_changed), (void *) i);
      /* stolen */
      combobox = gtk_combo_box_text_new();
      gtk_box_pack_start(GTK_BOX(e_box), combobox, FALSE, FALSE, 0);
      gtk_widget_set_tooltip_text(GTK_WIDGET(combobox), "Choose all or stolen only");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "all", "All");
      gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "stolen", "Stolen");
      gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), e->stolen);
      g_signal_connect(G_OBJECT(combobox), "changed", G_CALLBACK(on_stat_distribution_stolen_changed), (void *) i);
      /* title */
      GtkWidget * entry = gtk_entry_new();
      e->title_entry = entry;
      gtk_box_pack_start(GTK_BOX(e_box), e->title_entry, FALSE, FALSE, 0);
      if (e->title)
        gtk_entry_set_text(GTK_ENTRY(entry), e->title);
      gtk_widget_set_tooltip_text(GTK_WIDGET(entry), "Title of the plot");
      gtk_entry_set_width_chars(GTK_ENTRY(entry), 20);
      g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_stat_distribution_title_activate), (void *) i);
    }

    GtkWidget * hbox, * label, * entry;
    GtkWidget * button;

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    button = gtk_button_new_with_mnemonic("_Add +");
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_stat_distribution_add_button_clicked), (void *) NULL);
    
    gtk_box_pack_start(GTK_BOX(tab_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    label = gtk_label_new("xrange: from ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 10);
    char str[10];
    sprintf(str, "%ld", DVG->SD->xrange_from);
    gtk_entry_set_text(GTK_ENTRY(entry), str);
    g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_stat_distribution_xrange_from_activate), (void *) NULL);
    label = gtk_label_new(" to ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 10);
    sprintf(str, "%ld", DVG->SD->xrange_to);
    gtk_entry_set_text(GTK_ENTRY(entry), str);
    g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_stat_distribution_xrange_to_activate), (void *) NULL);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    label = gtk_label_new("granularity: ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 10);
    sprintf(str, "%d", DVG->SD->bar_width);
    gtk_entry_set_text(GTK_ENTRY(entry), str);
    g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_stat_distribution_granularity_activate), (void *) NULL);
    
    gtk_box_pack_start(GTK_BOX(tab_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);
  
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    label = gtk_label_new("Output: ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 15);
    gtk_entry_set_text(GTK_ENTRY(entry), DVG->SD->fn);
    g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_stat_distribution_output_filename_activate), (void *) NULL);
    button = gtk_button_new_with_mnemonic("_Show");
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_stat_distribution_show_button_clicked), (void *) NULL);
  }

  /* Build breakdown graphs tab */
  {
    tab_label = gtk_label_new("Breakdown Graphs");
    tab_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_box, tab_label);
    long i;
    for (i = 0; i < DMG->nD; i++) {
      dm_dag_t * D = &DMG->D[i];
      GtkWidget * dag_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start(GTK_BOX(tab_box), dag_box, FALSE, FALSE, 0);
      GtkWidget * checkbox = gtk_check_button_new_with_label(D->name);
      gtk_box_pack_start(GTK_BOX(dag_box), checkbox, FALSE, FALSE, 0);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), DMG->SBG->checked_D[i]);
      gtk_widget_set_tooltip_text(GTK_WIDGET(checkbox), D->P->name);
      g_signal_connect(G_OBJECT(checkbox), "toggled", G_CALLBACK(on_stat_breakdown_dag_checkbox_toggled), (void *) i);
      
      GtkWidget * entry = gtk_entry_new();
      gtk_box_pack_start(GTK_BOX(dag_box), entry, FALSE, FALSE, 0);
      gtk_entry_set_width_chars(GTK_ENTRY(entry), 20);
      gtk_entry_set_text(GTK_ENTRY(entry), D->name_on_graph);
      gtk_widget_set_tooltip_text(GTK_WIDGET(entry), D->P->filename); 
      g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_stat_breakdown_dag_name_on_graph_entry_activate), (void *) D);
    }

    GtkWidget * hbox, * label, * entry, * button;
    
    gtk_box_pack_start(GTK_BOX(tab_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    label = gtk_label_new("Output: ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 15);
    gtk_entry_set_text(GTK_ENTRY(entry), DMG->SBG->fn);
    g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_stat_breakdown_output_filename_activate), (void *) NULL);
    button = gtk_button_new_with_mnemonic("_Show");
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_stat_breakdown_show_button_clicked), (void *) NULL);

    gtk_box_pack_start(GTK_BOX(tab_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 4);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    label = gtk_label_new("Critical-path statistics: ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    entry = gtk_entry_new();
    gtk_box_pack_start(GTK_BOX(hbox), entry, FALSE, FALSE, 0);
    gtk_entry_set_width_chars(GTK_ENTRY(entry), 15);
    gtk_entry_set_text(GTK_ENTRY(entry), DMG->SBG->fn_2);
    g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_stat_breakdown_output_filename2_activate), (void *) NULL);
    
    for (i = 0; i < DV_NUM_CRITICAL_PATHS; i++) {
      GtkWidget * cp_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start(GTK_BOX(tab_box), cp_box, FALSE, FALSE, 0);
      char str[50];
      sprintf(str, "Critical path %ld", i + 1);
      GtkWidget * checkbox = gtk_check_button_new_with_label(str);
      gtk_box_pack_start(GTK_BOX(cp_box), checkbox, FALSE, FALSE, 0);
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checkbox), DMG->SBG->checked_cp[i]);
      g_signal_connect(G_OBJECT(checkbox), "toggled", G_CALLBACK(on_stat_breakdown_cp_checkbox_toggled), (void *) i);
    }

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    label = gtk_label_new("Work-delay breakdown: ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    button = gtk_button_new_with_mnemonic("_Show");
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_stat_breakdown_show2_button_clicked), (void *) NULL);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    label = gtk_label_new("Safe-problematic delay breakdown: ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    button = gtk_button_new_with_mnemonic("_Show");
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_stat_breakdown_show3_button_clicked), (void *) NULL);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    label = gtk_label_new("Edge-based delay breakdown: ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    button = gtk_button_new_with_mnemonic("_Show");
    gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_stat_breakdown_show4_button_clicked), (void *) NULL);
  }


  /* Run */
  gtk_widget_show_all(dialog_box);
  gtk_dialog_run(GTK_DIALOG(dialog));
  
  /* Destroy */
  gtk_widget_destroy(dialog);
}



static void
dv_change_focused_view(dv_view_t * V) {
  if (V == DVG->activeV) return;

  if (DVG->activeV) {
    DVG->activeV->S->focused = 0;
    DVG->activeV = NULL;
  }
  DVG->activeV = V;
  if (DVG->activeV) {
    DVG->activeV->S->focused = 1;
  }
  dv_statusbar_update_selection_status();
}

void
dv_change_focused_viewport(dv_viewport_t * VP) {
  if (VP == DVG->activeVP && DVG->activeV) return;

  if (DVG->activeVP && VP != DVG->activeVP) {
    dv_viewport_t * old_VP = DVG->activeVP;
    DVG->activeVP = NULL;
    dv_change_focused_view(NULL);
    dv_queue_draw_viewport(old_VP);
  }
  DVG->activeVP = VP;
  if (DVG->activeVP) {
    /*
    if (!DVG->activeVP->mV[DVG->activeV - DVG->V])
      dv_change_focused_view(NULL);
    */
    int i;
    for (i = 0; i < DVG->nV; i++)
      if (VP->mV[i]) {
        dv_change_focused_view(&DVG->V[i]);
        break;
      }
    gtk_widget_grab_focus(DVG->activeVP->darea);
    dv_queue_draw_viewport(DVG->activeVP);
  }
}

static dv_viewport_t *
dv_switch_focused_viewport_r(dv_viewport_t * VP, dv_viewport_t * curVP) {
  if (!VP->split) {
    if (!curVP)
      return VP;
    if (VP == curVP)
      return curVP;
    else
      return NULL;
  } else {
    dv_viewport_t * ret = NULL;
    ret = dv_switch_focused_viewport_r(VP->vp1, curVP);
    if (!curVP) {
      if (ret)
        return ret;
      ret = dv_switch_focused_viewport_r(VP->vp2, NULL);
      return ret;
    } else if (!ret) {
      ret = dv_switch_focused_viewport_r(VP->vp2, curVP);
      return ret;
    } else if (ret == curVP) {
      ret = dv_switch_focused_viewport_r(VP->vp2, NULL);
      if (!ret)
        return curVP;
      else
        return ret;
    } else {
      return ret;
    }
  }
}

void
dv_switch_focused_viewport() {
  dv_viewport_t * old_VP = (DVG->activeVP)?( (DVG->activeVP->split)?NULL:DVG->activeVP ): NULL;
  dv_viewport_t * VP = dv_switch_focused_viewport_r(DVG->VP, old_VP);
  if (VP == old_VP || VP == NULL) {
    dv_change_focused_viewport(NULL);
  } else {
    dv_change_focused_viewport(VP);
  }
  //printf("VP %ld - V %ld\n", (DVG->activeVP)?(DVG->activeVP-DVG->VP):-1, (DVG->activeV)?(DVG->activeV-DVG->V):-1);
}

void
dv_switch_focused_view() {
  /*
  long i = 0;
  if (DVG->activeV)
    i = DVG->activeV - DVG->V + 1;
  while (i < DVG->nV) {
    dv_view_t * V = &DVG->V[i];
    int has_viewport = 0;
    int j;
    for (j = 0; j < DVG->nVP; j++)
      if (V->mVP[j]) {
        has_viewport = 1;
        break;
      }
    if (has_viewport) {
      dv_change_focused_view(V, 1);
      DVG->activeVP = V->mainVP;
    }
    i++;
  }
  dv_change_focused_view(NULL, 0);
  */
}

void
dv_switch_focused_view_inside_viewport() {
  dv_viewport_t * VP = DVG->activeVP;
  if (!VP) return;
  long i = 0;
  if (DVG->activeV)
    i = DVG->activeV - DVG->V + 1;
  while (i < DVG->nV) {
    if (VP->mV[i]) {
      dv_change_focused_view(&DVG->V[i]);
      //printf("VP %ld - V %ld\n", (DVG->activeVP)?(DVG->activeVP-DVG->VP):-1, (DVG->activeV)?(DVG->activeV-DVG->V):-1);
      return;
    }
    i++;
  }
  dv_change_focused_view(NULL);
  //printf("VP %ld - V %ld\n", (DVG->activeVP)?(DVG->activeVP-DVG->VP):-1, (DVG->activeV)?(DVG->activeV-DVG->V):-1);
}

char *
dv_choose_a_new_dag_file() {
  GtkWidget * dialog = gtk_file_chooser_dialog_new("Open DAG File",
                                                   GTK_WINDOW(GUI->management_window),
                                                   GTK_FILE_CHOOSER_ACTION_OPEN,
                                                   "_Cancel", GTK_RESPONSE_CANCEL,
                                                   "_Open", GTK_RESPONSE_ACCEPT,
                                                   NULL);
  char * filename = NULL;
  if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_ACCEPT)
    filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));
  gtk_widget_destroy(dialog);
  return filename;
}

void
dv_open_dr_stat_file(dm_pidag_t * P) {
  int n = strlen(P->fn);
  char * filename = (char *) dv_malloc( sizeof(char) * (n + 2) );
  strcpy(filename, P->fn);
  filename[n] = '\0';
  if (strcmp(filename + n - 3, "dag") != 0)
    return;
  filename[n-3] = 's';
  filename[n-2] = 't';
  filename[n-1] = 'a';
  filename[n]   = 't';
  filename[n+1] = '\0';
  /* call gnuplot */
  GPid pid;
  char * argv[3];
  argv[0] = "gedit";
  argv[1] = filename;
  argv[2] = NULL;
  int ret = g_spawn_async_with_pipes(NULL, argv, NULL,
                                     G_SPAWN_SEARCH_PATH, //G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH,
                                     NULL, NULL, &pid,
                                     NULL, NULL, NULL, NULL);
  if (!ret) {
    fprintf(stderr, "g_spawn_async_with_pipes() failed.\n");
  }
  dv_free(filename, strlen(filename) + 1);
}

void
dv_open_dr_pp_file(dm_pidag_t * P) {
  int n = strlen(P->fn);
  char * filename = (char *) dv_malloc( sizeof(char) * (n + 1) );
  strcpy(filename, P->fn);
  filename[n] = '\0';
  if (strcmp(filename + n - 3, "dag") != 0)
    return;
  filename[n-3] = 'g';
  filename[n-2] = 'p';
  filename[n-1] = 'l';
  /* call gnuplot */
  GPid pid;
  char * argv[4];
  argv[0] = "gnuplot";
  argv[1] = "-persist";
  argv[2] = filename;
  argv[3] = NULL;
  int ret = g_spawn_async_with_pipes(NULL, argv, NULL,
                                     G_SPAWN_SEARCH_PATH, //G_SPAWN_DEFAULT | G_SPAWN_SEARCH_PATH,
                                     NULL, NULL, &pid,
                                     NULL, NULL, NULL, NULL);
  if (!ret) {
    fprintf(stderr, "g_spawn_async_with_pipes() failed.\n");
  }
  dv_free(filename, strlen(filename) + 1);
}

dv_view_t *
dv_create_new_view(dm_dag_t * D) {
  if (!D) return NULL;
  dv_view_t * V = dv_view_create_new_with_dag(D);
  if (!V) return NULL;
  dv_view_layout(V);

  /* Update GUI widgets */
  
  /* button in D's mini_frame */
  GtkWidget * button = gtk_button_new_with_label(V->name);
  gtk_box_pack_start(GTK_BOX(((dv_dag_t*)D->g)->views_box), button, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_management_window_view_clicked), (void *) V);

  /* check menu item in VP's menu */
  int i;
  for (i = 0; i < DVG->nVP; i++) {
    dv_viewport_t * VP = &DVG->VP[i];
    int c = 0;
    int j;
    for (j = 0; j < DVG->nV; j++)
      if (((dv_dag_t*)D->g)->mV[j]) c++;
    if (c <= 1) continue;
    GtkWidget * dag_menu = GTK_WIDGET(VP->dag_menu);
    GList * children = gtk_container_get_children(GTK_CONTAINER(dag_menu));
    GList * child = children;
    int pos = 0;
    while (child) {
      GtkWidget * widget = GTK_WIDGET(child->data);
      const gchar * str = gtk_menu_item_get_label(GTK_MENU_ITEM(widget));
      if (strcmp(str, D->name) == 0) {
        if (c > 2) {
          GtkWidget * submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget));
          GtkWidget * item = gtk_check_menu_item_new_with_label(V->name);
          gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
          g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(on_management_window_viewport_dag_menu_item_toggled), (void *) VP);
        } else if (c == 2) {
          gtk_container_remove(GTK_CONTAINER(dag_menu), widget);
          GtkWidget * d_item = gtk_menu_item_new_with_label(D->name);
          gtk_menu_shell_insert(GTK_MENU_SHELL(dag_menu), d_item, pos);
          GtkWidget * submenu = gtk_menu_new();
          gtk_menu_item_set_submenu(GTK_MENU_ITEM(d_item), submenu);
          for (j = 0; j < DVG->nV; j++)
            if (((dv_dag_t*)D->g)->mV[j]) {
              GtkWidget * item = gtk_check_menu_item_new_with_label(DVG->V[j].name);
              gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
              if (VP->mV[j])
                gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), TRUE);
              g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(on_management_window_viewport_dag_menu_item_toggled), (void *) VP);
            }
        }
        break;
      }
      child = child->next;
      pos++;
    }
    gtk_widget_show_all(DVG->VP[i].dag_menu);    
  }

  /* toolbar's settings button's dag menu */
  if (GUI->dag_menu) {
    GtkWidget * dag_menu = GUI->dag_menu;
    GList * children = gtk_container_get_children(GTK_CONTAINER(dag_menu));
    GList * child = children;
    int pos = 0;
    while (child) {
      GtkWidget * widget = GTK_WIDGET(child->data);
      const gchar * str = gtk_menu_item_get_label(GTK_MENU_ITEM(widget));
      if (strcmp(str, D->name) == 0) {
        int c = 0;
        int i;
        for (i = 0; i < DVG->nV; i++)
          if (((dv_dag_t*)D->g)->mV[i]) c++;
        if (c > 2) {
          GtkWidget * submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(widget));
          GtkWidget * item = gtk_menu_item_new_with_label(V->name);
          gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
          g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_dag_menu_item_activated), (void *) V);
        } else if (c == 2) {
          gtk_container_remove(GTK_CONTAINER(dag_menu), widget);
          widget = gtk_menu_item_new_with_label(D->name);
          gtk_menu_shell_insert(GTK_MENU_SHELL(dag_menu), widget, pos);
          GtkWidget * submenu = gtk_menu_new();
          gtk_menu_item_set_submenu(GTK_MENU_ITEM(widget), submenu);
          int j;
          for (j = 0; j < DVG->nV; j++)
            if (((dv_dag_t*)D->g)->mV[j]) {
              GtkWidget * item = gtk_menu_item_new_with_label(DVG->V[j].name);
              gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
              g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_dag_menu_item_activated), (void *) &DVG->V[j]);
            }
        }
        break;
      }
      child = child->next;
      pos++;
    }
    gtk_widget_show_all(dag_menu);
  }

  return V;
}

dm_dag_t *
dv_create_new_dag(dm_pidag_t * P) {
  if (!P) return NULL;
  dm_dag_t * D = dm_dag_create_new_with_pidag(P);
  if (!D) return NULL;
  D->g = (void *) dv_malloc(sizeof(dv_dag_t));
  memset(D->g, 0, sizeof(dv_dag_t));
  
  /* Update GUI widgets */
  
  /* DAG management window */
  GtkWidget * mini_frame = ((dv_dag_t*)D->g)->mini_frame = gtk_frame_new(D->name);
  g_object_ref(G_OBJECT(mini_frame));
  gtk_container_set_border_width(GTK_CONTAINER(mini_frame), 5);
  if (GUI->management_window) {
    GtkWidget * hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(GUI->scrolled_box), hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), ((dv_dag_t*)D->g)->mini_frame, TRUE, TRUE, 0);
  }
  GtkWidget * mini_frame_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  g_object_ref(G_OBJECT(mini_frame_box));
  gtk_container_add(GTK_CONTAINER(mini_frame), mini_frame_box);
  gtk_container_set_border_width(GTK_CONTAINER(mini_frame_box), 5);

  GtkWidget * box_1;
  box_1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_pack_start(GTK_BOX(mini_frame_box), box_1, FALSE, FALSE, 0);
  GtkWidget * label;
  char s[DV_STRING_LENGTH];
  sprintf(s, "DAG file %ld: %s", D->P - DMG->P, D->P->fn);
  label = gtk_label_new(s);
  gtk_box_pack_start(GTK_BOX(box_1), label, FALSE, FALSE, 0);

  box_1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_pack_start(GTK_BOX(mini_frame_box), box_1, FALSE, FALSE, 0);
  dv_dag_update_status_label(D);
  label = ((dv_dag_t*)D->g)->status_label;
  gtk_box_pack_start(GTK_BOX(box_1), label, FALSE, FALSE, 0);

  box_1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_pack_start(GTK_BOX(mini_frame_box), box_1, FALSE, FALSE, 0);
  label = gtk_label_new("Associated View(s):");
  gtk_box_pack_start(GTK_BOX(box_1), label, FALSE, FALSE, 0);
  GtkWidget * box_2;
  box_2 = ((dv_dag_t*)D->g)->views_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_pack_start(GTK_BOX(box_1), box_2, FALSE, FALSE, 0);
  
  box_1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_pack_start(GTK_BOX(mini_frame_box), box_1, FALSE, FALSE, 0);
  GtkWidget * button;
  button = gtk_button_new_with_label("Open DR's Stat");
  gtk_box_pack_start(GTK_BOX(box_1), button, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_management_window_open_stat_button_clicked), (void *) D);
  button = gtk_button_new_with_label("Open DR's PP");
  gtk_box_pack_start(GTK_BOX(box_1), button, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_management_window_open_pp_button_clicked), (void *) D);
  gtk_box_pack_start(GTK_BOX(box_1), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 4);
  button = gtk_button_new_with_label("Load Full");
  gtk_box_pack_start(GTK_BOX(box_1), button, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_management_window_expand_dag_button_clicked), (void *) D);
  button = gtk_button_new_with_label("Add a new View");
  gtk_box_pack_start(GTK_BOX(box_1), button, FALSE, FALSE, 0);  
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_management_window_add_new_view_clicked), (void *) D);

  /* check menu item in VP's menu */
  int i;
  for (i = 0; i < DVG->nVP; i++) {
    GtkWidget * item = gtk_check_menu_item_new_with_label(D->name);
    gtk_menu_shell_append(GTK_MENU_SHELL(DVG->VP[i].dag_menu), item);
    gtk_widget_show_all(DVG->VP[i].dag_menu);
    g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(on_management_window_viewport_dag_menu_item_toggled), (void *) &DVG->VP[i]);
  }  

  /* toolbar's settings button's dag menu */
  if (GUI->dag_menu) {
    GtkWidget * dag_menu = GUI->dag_menu;
    GtkWidget * item = gtk_menu_item_new_with_label(D->name);
    gtk_menu_shell_append(GTK_MENU_SHELL(dag_menu), item);
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_dag_menu_item_activated), (void *) NULL);
    gtk_widget_show_all(dag_menu);
  }

  /* workers sidebar's dag menu */
  if (GUI->replay.dag_menu) {
    GtkWidget * dag_menu = GUI->replay.dag_menu;
    GtkWidget * item = gtk_check_menu_item_new_with_label(D->name);
    gtk_menu_shell_append(GTK_MENU_SHELL(dag_menu), item);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
    g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(on_replay_sidebox_dag_toggled), (void *) D);
    gtk_widget_show_all(dag_menu);
  }

  return D;
}

dm_pidag_t *
dv_create_new_pidag(char * filename) {
  if (!filename) return NULL;
  dm_pidag_t * P = dm_pidag_read_new_file(filename);
  if (!P) return NULL;
  P->g = (void *) dv_malloc(sizeof(dv_pidag_t));
  memset(P->g, 0, sizeof(dv_pidag_t));

  P->name = malloc( sizeof(char) * 20 );
  sprintf(P->name, "DAG file %ld", P - DMG->P);

  /* Update GUI widgets */
  
  /* DAG management window */
  GtkWidget * mini_frame = ((dv_pidag_t *) P->g)->mini_frame = gtk_frame_new(P->name);
  g_object_ref(G_OBJECT(mini_frame));
  gtk_container_set_border_width(GTK_CONTAINER(mini_frame), 5);
  if (GUI->management_window) {
    GtkWidget * hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(GUI->dag_file_scrolled_box), hbox, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), ((dv_pidag_t *) P->g)->mini_frame, TRUE, TRUE, 0);
  }
  GtkWidget * mini_frame_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  g_object_ref(G_OBJECT(mini_frame_box));
  gtk_container_add(GTK_CONTAINER(mini_frame), mini_frame_box);
  gtk_container_set_border_width(GTK_CONTAINER(mini_frame_box), 5);

  GtkWidget * box_1;
  box_1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_pack_start(GTK_BOX(mini_frame_box), box_1, FALSE, FALSE, 0);
  GtkWidget * label;
  char s[DV_STRING_LENGTH];
  sprintf(s, "File name: %s", P->fn);
  label = gtk_label_new(s);
  gtk_box_pack_start(GTK_BOX(box_1), label, FALSE, FALSE, 0);

  box_1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_pack_start(GTK_BOX(mini_frame_box), box_1, FALSE, FALSE, 0);
  sprintf(s, "%ld nodes, %ld edges, %ld workers, size %ld", P->n, P->m, P->num_workers, P->sz);
  label = gtk_label_new(s);
  gtk_box_pack_start(GTK_BOX(box_1), label, FALSE, FALSE, 0);

  box_1 = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_pack_start(GTK_BOX(mini_frame_box), box_1, FALSE, FALSE, 0);
  GtkWidget * button;
  button = gtk_button_new_with_label("Open DR's Stat");
  gtk_box_pack_start(GTK_BOX(box_1), button, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_management_window_pidag_open_stat_button_clicked), (void *) P);
  button = gtk_button_new_with_label("Open DR's PP");
  gtk_box_pack_start(GTK_BOX(box_1), button, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_management_window_pidag_open_pp_button_clicked), (void *) P);
  gtk_box_pack_start(GTK_BOX(box_1), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 4);

  /* append to create-new-DAG menu */
  if (GUI->create_dag_menu) {
    sprintf(s, "%s: %s", P->name, P->fn);
    GtkWidget * menu_item = gtk_menu_item_new_with_label(s);
    gtk_menu_shell_append(GTK_MENU_SHELL(GUI->create_dag_menu), menu_item);
    g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(on_management_window_add_new_dag_activated), (void *) P);
    gtk_widget_show_all(GUI->create_dag_menu);
  }
  
  return P;
}

void
dv_gui_init(dv_gui_t * gui) {
  memset(gui, 0, sizeof(dv_gui_t));
#if 0  
  gui->builder = NULL;
  gui->window = NULL;
  gui->window_box = NULL;
  gui->menubar = NULL;
  gui->main_box = NULL;
  gui->accel_group = NULL;
  gui->context_menu = NULL;
  gui->left_sidebar = NULL;
  gui->toolbar = NULL;
  gui->dag_menu = NULL;
  gui->division_menu = NULL;
  gui->statusbar1 = NULL;
  gui->statusbar2 = NULL;
  gui->statusbar3 = NULL;
  gui->management_window = NULL;
  gui->notebook = NULL;
  gui->scrolled_box = NULL;
  gui->dag_file_scrolled_box = NULL;
  gui->create_dag_menu = NULL;
  
  gui->replay.sidebox = NULL;
  gui->replay.enable = NULL;
  gui->replay.scale = NULL;
  gui->replay.entry = NULL;
  gui->replay.time_step_entry = NULL;
  gui->replay.dag_menu = NULL;
  int i;
  for (i = 0; i < DM_MAX_DAG; i++)
    gui->replay.mD[i] = 0;

  gui->nodeinfo.sidebox = NULL;
#endif  
}

static void
dv_gui_build_management_window(dv_gui_t * gui) {
  /* Build management window */
  gui->management_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  GtkWidget * window = gui->management_window;
  char s[DV_STRING_LENGTH];
  sprintf(s, "Management");
  gtk_window_set_title(GTK_WINDOW(window), s);
  gtk_window_set_default_size(GTK_WINDOW(window), 850, 600);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_MOUSE);
  gtk_window_set_modal(GTK_WINDOW(window), 0);
  gtk_window_set_transient_for(GTK_WINDOW(window), GTK_WINDOW(GUI->window));
  g_signal_connect(G_OBJECT(window), "delete-event", G_CALLBACK(gtk_widget_hide_on_delete), NULL);
  GtkWidget * window_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(window), window_box);
  GtkWidget * notebook = gui->notebook = gtk_notebook_new();
  gtk_box_pack_start(GTK_BOX(window_box), notebook, TRUE, TRUE, 0);
  
  GtkWidget * tab_label;
  GtkWidget * tab_box;

  /* Build tab "Viewports" */
  {
    tab_label = gtk_label_new("Viewports");
    tab_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_box, tab_label); 
    gtk_box_pack_start(GTK_BOX(tab_box), DVG->VP->mini_frame, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(tab_box), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);

    GtkWidget * right = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(tab_box), right, FALSE, FALSE, 0);
    gtk_widget_set_size_request(GTK_WIDGET(right), 315, 0);
    GtkWidget * right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(right), right_box);
    gtk_box_pack_start(GTK_BOX(right_box), DVG->VP->mini_frame_2, FALSE, FALSE, 0);
  }

  /* Build tab "DAGs" */
  {
    tab_label = gtk_label_new("DAGs");
    tab_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_box, tab_label);

    GtkWidget * hbox;
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
    GtkWidget * label = gtk_label_new("Add a new DAG with ");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    GtkWidget * menubutton = gtk_menu_button_new();
    gtk_box_pack_start(GTK_BOX(hbox), menubutton, FALSE, FALSE, 0);
    GtkWidget * menu = gui->create_dag_menu = gtk_menu_new();
    gtk_menu_button_set_popup(GTK_MENU_BUTTON(menubutton), menu);
    int i;
    for (i = 0; i < DMG->nP; i++) {
      sprintf(s, "%s: %s", DMG->P[i].name, DMG->P[i].fn);
      GtkWidget * menu_item = gtk_menu_item_new_with_label(s);
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), menu_item);
      g_signal_connect(G_OBJECT(menu_item), "activate", G_CALLBACK(on_management_window_add_new_dag_activated), (void *) &DMG->P[i]);
    }
    gtk_widget_show_all(menu);
    
    gtk_box_pack_start(GTK_BOX(tab_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

    GtkWidget * scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(tab_box), scrolled, TRUE, TRUE, 0);
    //gtk_widget_set_size_request(GTK_WIDGET(scrolled), 315, 0);
    GtkWidget * scrolled_box = gui->scrolled_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(scrolled), scrolled_box);

    for (i = 0; i < DMG->nD; i++) {
      hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start(GTK_BOX(scrolled_box), hbox, FALSE, FALSE, 0);
      gtk_box_pack_start(GTK_BOX(hbox), ((dv_dag_t*)DMG->D[i].g)->mini_frame, TRUE, TRUE, 0);
    }
  }

  /* Build tab "DAG files" */
  {
    tab_label = gtk_label_new("DAG files");
    tab_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_notebook_append_page(GTK_NOTEBOOK(notebook), tab_box, tab_label);
    
    GtkWidget * hbox;
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    gtk_box_pack_start(GTK_BOX(tab_box), hbox, FALSE, FALSE, 0);
    gtk_container_set_border_width(GTK_CONTAINER(hbox), 5);
    GtkWidget * label = gtk_label_new("Add a new DAG file");
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    GtkWidget * button = gtk_button_new_with_label("+");
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_management_window_add_new_dag_file_clicked), (void *) NULL);
    
    gtk_box_pack_start(GTK_BOX(tab_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);

    GtkWidget * scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_box_pack_start(GTK_BOX(tab_box), scrolled, TRUE, TRUE, 0);
    //gtk_widget_set_size_request(GTK_WIDGET(scrolled), 315, 0);
    GtkWidget * scrolled_box = gui->dag_file_scrolled_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_container_add(GTK_CONTAINER(scrolled), scrolled_box);

    int i;
    for (i = 0; i < DMG->nP; i++) {
      hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
      gtk_box_pack_start(GTK_BOX(scrolled_box), hbox, FALSE, FALSE, 0);
      gtk_box_pack_start(GTK_BOX(hbox), ((dv_pidag_t *) DMG->P[i].g)->mini_frame, TRUE, TRUE, 0);
    }
  }

}

GtkWidget *
dv_gui_get_management_window(dv_gui_t * gui) {
  if (!gui->management_window)
    dv_gui_build_management_window(gui);
  return gui->management_window;
}

static void
dv_gui_build_main_window(dv_gui_t * gui, _unused_ GtkApplication * app) {
  /* Window */
  //if (app) gui->window = gtk_application_window_new(app); else
  GtkWidget * window = gui->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  //gtk_window_fullscreen(GTK_WINDOW(window));
  //gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
  gtk_window_maximize(GTK_WINDOW(window));
  //gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_title(GTK_WINDOW(window), "DAGViz");
  //gtk_window_set_icon_name(GTK_WINDOW(window), "applications-graphics");
  g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(G_OBJECT(GUI->window), "key-press-event", G_CALLBACK(on_window_key_event), NULL);

  /* Icons */
  /*
  gtk_window_set_icon_name(GTK_WINDOW(window), "dagviz");
  */
  char icon_filename[DV_STRING_LENGTH];
  GError * error = 0;
  GdkPixbuf * pixbuf;
  sprintf(icon_filename, "%s/%s", DVG->exe_path, "gui/dagviz_icon.svg");
  pixbuf = gdk_pixbuf_new_from_file(icon_filename, &error);
  if (!pixbuf) {
    dm_perror("cannot set icon %s: %s", icon_filename, error->message);
  } else {
    gtk_window_set_icon(GTK_WINDOW(window), pixbuf);
  }
  sprintf(icon_filename, "%s/%s", DVG->exe_path, "gui/viewport_division_icon.svg");
  GtkWidget * viewport_division_icon = gtk_image_new_from_file(icon_filename);
  
  /* Accelerator group */
  GtkAccelGroup * accel_group = gui->accel_group = gtk_accel_group_new();
  gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);
  
  /* Window_Box */
  gui->window_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
  gtk_container_add(GTK_CONTAINER(gui->window), gui->window_box);
  
  /* Menubar */
  {
    GtkBuilder * builder = gui->builder = gtk_builder_new();
    GError * gerr = NULL;
    //gtk_builder_add_from_file(builder, "gui/menubar.ui", &gerr);
    gtk_builder_add_from_resource(builder, "/org/utokyo/dagviz/gui/menubar.ui", &gerr);
    if (gerr) {
      g_error("ERROR: %s\n", gerr->message);
      exit(1);
    }
    gtk_builder_connect_signals(builder, NULL);
    GtkWidget * menubar = gui->menubar = (GtkWidget *) gtk_builder_get_object(builder, "menubar");
    gtk_box_pack_start(GTK_BOX(gui->window_box), menubar, FALSE, FALSE, 0);
    gtk_widget_show_all(menubar);

    GtkWidget * item;
    item = GTK_WIDGET(gtk_builder_get_object(builder, "export_viewport"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_e, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
    item = GTK_WIDGET(gtk_builder_get_object(builder, "export_all"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_e, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
    
    item = GTK_WIDGET(gtk_builder_get_object(builder, "change_focused_view"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_Tab, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
    item = GTK_WIDGET(gtk_builder_get_object(builder, "expand_dag"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_x, 0, GTK_ACCEL_VISIBLE);
    item = GTK_WIDGET(gtk_builder_get_object(builder, "contract_dag"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_c, 0, GTK_ACCEL_VISIBLE); 

    item = GTK_WIDGET(gtk_builder_get_object(builder, "manage_viewports"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_v, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE); 
    item = GTK_WIDGET(gtk_builder_get_object(builder, "manage_dags"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_d, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE); 
    item = GTK_WIDGET(gtk_builder_get_object(builder, "manage_dag_files"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_f, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE); 

    item = GTK_WIDGET(gtk_builder_get_object(builder, "layout_dag"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_1, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
    item = GTK_WIDGET(gtk_builder_get_object(builder, "layout_dag_boxes"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_2, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
    item = GTK_WIDGET(gtk_builder_get_object(builder, "layout_timeline"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_3, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
    item = GTK_WIDGET(gtk_builder_get_object(builder, "layout_timeline_ver"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_4, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
    item = GTK_WIDGET(gtk_builder_get_object(builder, "layout_paraprof"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_5, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
    item = GTK_WIDGET(gtk_builder_get_object(builder, "layout_criticalpath"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_6, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 

    item = GTK_WIDGET(gtk_builder_get_object(builder, "toolbox"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_t, GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE); 
    item = GTK_WIDGET(gtk_builder_get_object(builder, "replay"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_r, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE); 
    item = GTK_WIDGET(gtk_builder_get_object(builder, "nodeinfo"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_n, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE); 

    item = GTK_WIDGET(gtk_builder_get_object(builder, "zoomfit_full"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_f, 0, GTK_ACCEL_VISIBLE); 
    item = GTK_WIDGET(gtk_builder_get_object(builder, "zoomfit_hor"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_h, 0, GTK_ACCEL_VISIBLE); 
    item = GTK_WIDGET(gtk_builder_get_object(builder, "zoomfit_ver"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_v, 0, GTK_ACCEL_VISIBLE); 

    item = GTK_WIDGET(gtk_builder_get_object(builder, "statistics"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_s, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE); 
    item = GTK_WIDGET(gtk_builder_get_object(builder, "samples"));
    gtk_widget_add_accelerator(item, "activate", accel_group, GDK_KEY_b, GDK_CONTROL_MASK | GDK_SHIFT_MASK, GTK_ACCEL_VISIBLE);
  }
  
  /* Toolbar */
  {
    GtkWidget * toolbar = gui->toolbar = gtk_toolbar_new();
    gtk_box_pack_start(GTK_BOX(gui->window_box), toolbar, FALSE, FALSE, 0);

    /* divisions */
    {
      GtkToolItem * btn_divisions = gtk_menu_tool_button_new(NULL, NULL);
      gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_divisions, -1);
      gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);
      //gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_divisions), "video-display");
      //gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_divisions), "dagviz-viewport-division");
      gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(btn_divisions), viewport_division_icon);
      gtk_widget_set_tooltip_text(GTK_WIDGET(btn_divisions), "Manual screen division (Shift+Ctrl+V)");
      g_signal_connect(G_OBJECT(btn_divisions), "clicked", G_CALLBACK(on_menubar_manage_viewports_activated), NULL);
      gtk_menu_tool_button_set_arrow_tooltip_text(GTK_MENU_TOOL_BUTTON(btn_divisions), "Ready-made screen divisions");

      GtkWidget * menu = gui->division_menu = gtk_menu_new();
      gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(btn_divisions), menu);

      GtkWidget * item;
      GtkWidget * submenu;
      
      if (1) {//DMG->nP >= 1) {
        /* Divisions for 1 DAG: D , T , D|T , D/T , D|B, (D|B)/T */
        item = gtk_menu_item_new_with_mnemonic("Screen divisions for _1 DAG");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        submenu = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

        item = gtk_menu_item_new_with_label("DAG");
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);        
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_division_menu_onedag_activated), (void *) 1);

        item = gtk_menu_item_new_with_label("Timelines");
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);        
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_division_menu_onedag_activated), (void *) 2);

        item = gtk_menu_item_new_with_label("DAG | Timelines");
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);        
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_division_menu_onedag_activated), (void *) 3);
        
        item = gtk_menu_item_new_with_label("DAG / Timelines");
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);        
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_division_menu_onedag_activated), (void *) 4);

        item = gtk_menu_item_new_with_label("DAG | DAG (log)");
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);        
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_division_menu_onedag_activated), (void *) 6);
        
        item = gtk_menu_item_new_with_label("( DAG | DAG (log) ) / Timelines");
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_division_menu_onedag_activated), (void *) 5);

        item = gtk_menu_item_new_with_label("( DAG | DAG (linear) ) / Timelines");
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_division_menu_onedag_activated), (void *) 7);
      }
      
      if (1) {//DMG->nP >= 2) {
        /* Divisions for 2 DAGs: D|D , T|T , T/T , (D/T)|(D/T) , (D|T)/(D|T) , (D|B)/T | (D|B)/T , D|T */
        item = gtk_menu_item_new_with_mnemonic("Screen divisions for _2 DAGs");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        submenu = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

        item = gtk_menu_item_new_with_label("DAG | DAG");
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);        
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_division_menu_twodags_activated), (void *) 1);

        item = gtk_menu_item_new_with_label("Timelines | Timelines");
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);        
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_division_menu_twodags_activated), (void *) 2);
        
        item = gtk_menu_item_new_with_label("Timelines / Timelines");
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);        
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_division_menu_twodags_activated), (void *) 3);

        item = gtk_menu_item_new_with_label("DAG | Timelines");
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);        
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_division_menu_twodags_activated), (void *) 7);

        item = gtk_menu_item_new_with_label("(DAG / Timelines) | (DAG / Timelines)");
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_division_menu_twodags_activated), (void *) 4);

        item = gtk_menu_item_new_with_label("(DAG | Timelines) / (DAG | Timelines)");
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_division_menu_twodags_activated), (void *) 5);

        item = gtk_menu_item_new_with_label("( ( DAG | DAG (log) ) / Timelines ) | ( ( DAG | DAG (log) ) / Timelines )");
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_division_menu_twodags_activated), (void *) 6);
      }

      if (1) {//DMG->nP >= 3) {
        /* Divisions for 3 DAGs: D|D|D */
        item = gtk_menu_item_new_with_mnemonic("Screen divisions for _3 DAGs");
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
        submenu = gtk_menu_new();
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
        
        item = gtk_menu_item_new_with_label("DAG | DAG | DAG");
        gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
        g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_division_menu_threedags_activated), (void *) 1);
      }
      
      gtk_widget_show_all(menu);
    }

    /* settings button */
    {
      GtkToolItem * btn_settings = gtk_menu_tool_button_new(NULL, NULL);
      gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_settings, -1);
      char icon_filename[DV_STRING_LENGTH];
      sprintf(icon_filename, "%s/%s", DVG->exe_path, "gui/view_settings_icon.svg");
      GtkWidget * icon = gtk_image_new_from_file(icon_filename);
      gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(btn_settings), icon);
      //gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_settings), "preferences-system-symbolic");
      //gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_settings), "dagviz-view-settings");
      gtk_widget_set_tooltip_text(GTK_WIDGET(btn_settings), "Open toolbox for focused DAG (Ctrl+T)");
      g_signal_connect(G_OBJECT(btn_settings), "clicked", G_CALLBACK(on_toolbar_settings_button_clicked), NULL);
      gtk_menu_tool_button_set_arrow_tooltip_text(GTK_MENU_TOOL_BUTTON(btn_settings), "Open toolbox for");

      GtkWidget * menu = gui->dag_menu = gtk_menu_new();
      gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(btn_settings), menu);
    
      int i, j;
      for (i = 0; i < DMG->nD; i++) {
        dm_dag_t * D = &DMG->D[i];
        int c = 0;
        for (j = 0; j < DVG->nV; j++)
          if (((dv_dag_t*)D->g)->mV[j]) c++;
        if (c == 1) {
          GtkWidget * item = gtk_menu_item_new_with_label(D->name);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
          g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_dag_menu_item_activated), (void *) NULL);
        } else if (c > 1) {
          GtkWidget * item = gtk_menu_item_new_with_label(D->name);
          gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
          GtkWidget * submenu = gtk_menu_new();
          gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
          for (j = 0; j < DVG->nV; j++)
            if (((dv_dag_t*)D->g)->mV[j]) {
              item = gtk_menu_item_new_with_label(DVG->V[j].name);
              gtk_menu_shell_append(GTK_MENU_SHELL(submenu), item);
              g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_dag_menu_item_activated), (void *) &DVG->V[j]);
            }
        }
      }
      
      gtk_widget_show_all(menu);
    }

    /* buttons for switching layout types */
    {
      gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);
      GtkToolItem * button;
      GtkWidget * icon;
      
      button = gtk_tool_button_new(NULL, NULL);
      gtk_toolbar_insert(GTK_TOOLBAR(toolbar), button, -1);
      sprintf(icon_filename, "%s/%s", DVG->exe_path, "gui/dag_icon.svg");
      icon = gtk_image_new_from_file(icon_filename);
      gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(button), icon);
      //gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(button), "dagviz-dag");
      gtk_widget_set_tooltip_text(GTK_WIDGET(button), "DAG layout (Ctrl+1)");
      g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_toolbar_dag_layout_buttons_clicked), (void *) DV_LAYOUT_TYPE_DAG);
      
      button = gtk_menu_tool_button_new(NULL, NULL);
      gtk_toolbar_insert(GTK_TOOLBAR(toolbar), button, -1);
      sprintf(icon_filename, "%s/%s", DVG->exe_path, "gui/dag_boxes_icon.svg");
      icon = gtk_image_new_from_file(icon_filename);
      gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(button), icon);
      //gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(button), "dagviz-dag-boxes");
      gtk_widget_set_tooltip_text(GTK_WIDGET(button), "DAG-with-boxes layout (Ctrl+2)");
      g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_toolbar_dag_layout_buttons_clicked), (void *) DV_LAYOUT_TYPE_DAG_BOX_LOG);
      gtk_menu_tool_button_set_arrow_tooltip_text(GTK_MENU_TOOL_BUTTON(button), "Select scale type");
      
      GtkWidget * menu = gtk_menu_new();
      gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(button), menu);
      GtkWidget * item;
      item= gtk_menu_item_new_with_label("Log");
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
      g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_dag_layout_buttons_clicked), (void *) DV_LAYOUT_TYPE_DAG_BOX_LOG);
      item= gtk_menu_item_new_with_label("Power");
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
      g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_dag_layout_buttons_clicked), (void *) DV_LAYOUT_TYPE_DAG_BOX_POWER);
      item= gtk_menu_item_new_with_label("Linear");
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
      g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_dag_layout_buttons_clicked), (void *) DV_LAYOUT_TYPE_DAG_BOX_LINEAR);
      gtk_widget_show_all(menu);

      button = gtk_tool_button_new(NULL, NULL);
      gtk_toolbar_insert(GTK_TOOLBAR(toolbar), button, -1);
      sprintf(icon_filename, "%s/%s", DVG->exe_path, "gui/paraprof_icon.svg");
      icon = gtk_image_new_from_file(icon_filename);
      gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(button), icon);
      //gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(button), "dagviz-paraprof");
      gtk_widget_set_tooltip_text(GTK_WIDGET(button), "Parallelism profile layout (Ctrl+5)");
      g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_toolbar_dag_layout_buttons_clicked), (void *) DV_LAYOUT_TYPE_PARAPROF);

      button = gtk_tool_button_new(NULL, NULL);
      gtk_toolbar_insert(GTK_TOOLBAR(toolbar), button, -1);
      sprintf(icon_filename, "%s/%s", DVG->exe_path, "gui/critical_path_icon.svg");
      icon = gtk_image_new_from_file(icon_filename);
      gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(button), icon);
      //gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(button), "dagviz-critical-path");
      gtk_widget_set_tooltip_text(GTK_WIDGET(button), "Critical-path layout (Ctrl+6)");
      g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_toolbar_dag_layout_buttons_clicked), (void *) DV_LAYOUT_TYPE_CRITICAL_PATH);
    }

    /* zoomfit */
    {      
      gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);
      GtkWidget * icon;
      
      GtkToolItem * btn_zoomfit = gtk_menu_tool_button_new(NULL, NULL);
      gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoomfit, -1);
      sprintf(icon_filename, "%s/%s", DVG->exe_path, "gui/fit_button_icon.svg");
      icon = gtk_image_new_from_file(icon_filename);
      gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(btn_zoomfit), icon);
      //gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_zoomfit), "zoom-fit-best");
      //gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_zoomfit), "dagviz-fit-button");
      gtk_widget_set_tooltip_text(GTK_WIDGET(btn_zoomfit), "Fit fully (F)");
      g_signal_connect(G_OBJECT(btn_zoomfit), "clicked", G_CALLBACK(on_toolbar_zoomfit_button_clicked), (void *) 0);

      GtkWidget * menu = gtk_menu_new();
      gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(btn_zoomfit), menu);
      GtkWidget * item;
      
      item= gtk_menu_item_new_with_label("Fit width (H)");
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
      g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_zoomfit_button_clicked), (void *) 1);
      
      item= gtk_menu_item_new_with_label("Fit height (V)");
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
      g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_zoomfit_button_clicked), (void *) 2);

      gtk_widget_show_all(menu);
    }

    /* shrink, expand */
    GtkToolItem * btn_shrink = gtk_tool_button_new(NULL, NULL);
    GtkToolItem * btn_expand = gtk_tool_button_new(NULL, NULL);
    GtkWidget * icon;
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_shrink, -1);
    gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_expand, -1);
    //gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_shrink), "zoom-out");
    //gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_expand), "zoom-in");
    sprintf(icon_filename, "%s/%s", DVG->exe_path, "gui/minus_button_icon.svg");
    icon = gtk_image_new_from_file(icon_filename);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(btn_shrink), icon);
    //gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_shrink), "dagviz-minus-button");
    sprintf(icon_filename, "%s/%s", DVG->exe_path, "gui/plus_button_icon.svg");
    icon = gtk_image_new_from_file(icon_filename);
    gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(btn_expand), icon);
    //gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_expand), "dagviz-plus-button");
    gtk_widget_set_tooltip_text(GTK_WIDGET(btn_shrink), "Collapse one depth (C)"); 
    gtk_widget_set_tooltip_text(GTK_WIDGET(btn_expand), "Expand one depth (X)");
    g_signal_connect(G_OBJECT(btn_shrink), "clicked", G_CALLBACK(on_toolbar_shrink_button_clicked), NULL);  
    g_signal_connect(G_OBJECT(btn_expand), "clicked", G_CALLBACK(on_toolbar_expand_button_clicked), NULL);

    /* Critical path buttons */
    {
      gtk_toolbar_insert(GTK_TOOLBAR(toolbar), gtk_separator_tool_item_new(), -1);
      //GtkToolItem * btn_cp_compute = gtk_tool_button_new(gtk_label_new("Compute critical paths"), NULL);
      GtkToolItem * btn_cp_compute = gtk_menu_tool_button_new(NULL, NULL);
      gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_cp_compute, -1);
      sprintf(icon_filename, "%s/%s", DVG->exe_path, "gui/compute_critical_path_icon.svg");
      GtkWidget * icon = gtk_image_new_from_file(icon_filename);
      gtk_tool_button_set_icon_widget(GTK_TOOL_BUTTON(btn_cp_compute), icon);
      //gtk_tool_button_set_icon_name(GTK_TOOL_BUTTON(btn_cp_compute), "dagviz-critical-path-compute");
      gtk_widget_set_tooltip_text(GTK_WIDGET(btn_cp_compute), "Compute all critical paths so that they can be shown"); 
      g_signal_connect(G_OBJECT(btn_cp_compute), "clicked", G_CALLBACK(on_toolbar_critical_path_compute_button_clicked), NULL);

      GtkWidget * menu = gtk_menu_new();
      gtk_menu_tool_button_set_menu(GTK_MENU_TOOL_BUTTON(btn_cp_compute), menu);
      GtkWidget * item;
      
      item = gtk_menu_item_new_with_label("With largest work");
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
      gtk_widget_set_tooltip_text(GTK_WIDGET(item), "Show critical path of the most work"); 
      g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_critical_path_button_clicked), (void *) 0);
      
      item = gtk_menu_item_new_with_label("Ready path");
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
      gtk_widget_set_tooltip_text(GTK_WIDGET(item), "Show critical path of last finished child nodes");
      g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_critical_path_button_clicked), (void *) 1);

      item = gtk_menu_item_new_with_label("Weighted work & delay");
      gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
      gtk_widget_set_tooltip_text(GTK_WIDGET(item), "Show critical path of the most weighted work & delay");
      g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_toolbar_critical_path_button_clicked), (void *) 2);

      gtk_widget_show_all(menu);
    }
    
  }


  /* Main box */
  {
    GtkWidget * main_box = gui->main_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(gui->window_box), main_box, TRUE, TRUE, 0);
    //g_signal_connect(G_OBJECT(main_box), "key-press-event", G_CALLBACK(on_window_key_event), NULL);
    GtkWidget * left_sidebar = gui->left_sidebar = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_box_pack_start(GTK_BOX(gui->main_box), left_sidebar, FALSE, FALSE, 0);
  }


  /* Statusbars */
  GtkWidget * statusbar_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
  gtk_box_pack_end(GTK_BOX(gui->window_box), statusbar_box, FALSE, FALSE, 0);
  
  gui->statusbar2 = gtk_statusbar_new();
  gtk_box_pack_start(GTK_BOX(statusbar_box), gui->statusbar2, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text(GTK_WIDGET(gui->statusbar2), "Selection status");
  gtk_box_pack_start(GTK_BOX(statusbar_box), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);
  
  gui->statusbar3 = gtk_statusbar_new();
  gtk_box_pack_start(GTK_BOX(statusbar_box), gui->statusbar3, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text(GTK_WIDGET(gui->statusbar3), "Memory pool status");
  gtk_box_pack_start(GTK_BOX(statusbar_box), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);

  GtkWidget * statusbar_grid = gtk_grid_new();
  gtk_box_pack_end(GTK_BOX(statusbar_box), statusbar_grid, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text(GTK_WIDGET(statusbar_grid), "Mouse pointer's coordinates and zoom ratios");
  gtk_grid_set_row_spacing(GTK_GRID(statusbar_grid), 0);
  gtk_grid_set_column_spacing(GTK_GRID(statusbar_grid), 2);
  gtk_box_pack_end(GTK_BOX(statusbar_box), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 0);
  
  gui->status_x = gtk_label_new(NULL);
  gtk_grid_attach(GTK_GRID(statusbar_grid), gui->status_x, 0, 0, 1, 1);
  gui->status_zx = gtk_label_new(NULL);
  gtk_grid_attach(GTK_GRID(statusbar_grid), gui->status_zx, 1, 0, 1, 1);
  gui->status_y = gtk_label_new(NULL);
  gtk_grid_attach(GTK_GRID(statusbar_grid), gui->status_y, 0, 1, 1, 1);
  gui->status_zy = gtk_label_new(NULL);
  gtk_grid_attach(GTK_GRID(statusbar_grid), gui->status_zy, 1, 1, 1, 1);
  
  gui->statusbar1 = gtk_statusbar_new();
  gtk_box_pack_end(GTK_BOX(statusbar_box), gui->statusbar1, FALSE, FALSE, 0);
  gtk_widget_set_tooltip_text(GTK_WIDGET(gui->statusbar1), "Interaction status");

  /* Context menu */
  {
    GtkWidget * menu = gui->context_menu = gtk_menu_new();
    GtkWidget * item;
    item = gtk_menu_item_new_with_label("Display details on info sidebox");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_context_menu_gui_infobox_activated), (void *) NULL);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), gtk_separator_menu_item_new());
    item = gtk_menu_item_new_with_label("Always show on-site info box");
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    g_signal_connect(G_OBJECT(item), "activate", G_CALLBACK(on_context_menu_viewport_infobox_activated), (void *) NULL);
    gtk_widget_show_all(menu);
  }
}

GtkWidget *
dv_gui_get_main_window(dv_gui_t * gui, _unused_ GtkApplication * app) {
  if (!gui->window)
    dv_gui_build_main_window(gui, app);
  return gui->window;
}

  
void
dv_dag_set_time_step(dm_dag_t * D, double time_step) {
  D->time_step = time_step;
  char s[DV_STRING_LENGTH];
  sprintf(s, "%0.0lf", time_step);
  gtk_entry_set_text(GTK_ENTRY(GUI->replay.time_step_entry), s);
}

void
dv_dag_set_current_time(dm_dag_t * D, double current_time) {
  if (current_time < 0
      || current_time > (D->et - D->bt)
      || current_time == D->current_time)
    return;
  D->current_time = current_time;
  gtk_range_set_value(GTK_RANGE(GUI->replay.scale), current_time);
  char s[DV_STRING_LENGTH];
  sprintf(s, "%0.0lf", current_time);
  gtk_entry_set_text(GTK_ENTRY(GUI->replay.entry), s);
  dv_queue_draw_dag(D);
}

void
dv_dag_set_draw_current_time_active(dm_dag_t * D, int active) {
  if (!active) {
    D->draw_with_current_time = 0;
  } else {
    D->draw_with_current_time = 1;
    double current_time = gtk_range_get_value(GTK_RANGE(GUI->replay.scale));
    dv_dag_set_current_time(D, current_time);
    double time_step = atof(gtk_entry_get_text(GTK_ENTRY(GUI->replay.time_step_entry)));
    dv_dag_set_time_step(D, time_step);
  }
  dv_queue_draw_dag(D);
}

void
dv_gui_replay_sidebox_set_dag(dm_dag_t * D, int set) {
  if (!set) {
    GUI->replay.mD[D - DMG->D] = 0;
    dv_dag_set_draw_current_time_active(D, 0);
    return;
  }
  GUI->replay.mD[D - DMG->D] = 1;
  gtk_range_set_range(GTK_RANGE(GUI->replay.scale), 0, D->et - D->bt);
  dv_dag_set_current_time(D, D->current_time);
  dv_dag_set_time_step(D, D->time_step);
  gboolean active = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(GUI->replay.enable));
  dv_dag_set_draw_current_time_active(D, active);
}

static void
dv_gui_build_replay_sidebox(dv_gui_t * gui) {
  GtkWidget * sidebox = gui->replay.sidebox = gtk_frame_new("Replay");
  gtk_container_set_border_width(GTK_CONTAINER(sidebox), 5);
  g_object_ref(sidebox);

  GtkWidget * sidebox_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(sidebox), sidebox_box);
  gtk_container_set_border_width(GTK_CONTAINER(sidebox_box), 5);

  GtkWidget * box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(sidebox_box), box, FALSE, FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(box), 5);

  GtkWidget * enable = gui->replay.enable = gtk_check_button_new_with_label("Enable");
  gtk_box_pack_start(GTK_BOX(box), enable, FALSE, FALSE, 0);
  gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(enable), FALSE);
  g_signal_connect(G_OBJECT(enable), "toggled", G_CALLBACK(on_replay_sidebox_enable_toggled), (void *) NULL);
  
  GtkWidget * menu_button = gtk_menu_button_new();
  gtk_box_pack_start(GTK_BOX(box), menu_button, FALSE, FALSE, 0);
  GtkWidget * menu = gui->replay.dag_menu = gtk_menu_new();
  gtk_menu_button_set_popup(GTK_MENU_BUTTON(menu_button), menu);
  int i;
  for (i = 0; i < DMG->nD; i++) {
    dm_dag_t * D = &DMG->D[i];
    GtkWidget * item = gtk_check_menu_item_new_with_label(D->name);
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_check_menu_item_set_active(GTK_CHECK_MENU_ITEM(item), FALSE);
    g_signal_connect(G_OBJECT(item), "toggled", G_CALLBACK(on_replay_sidebox_dag_toggled), (void *) D);
  }
  gtk_widget_show_all(menu);
  
  //GtkWidget * play = gtk_button_new_with_label("Play");
  //gtk_box_pack_end(GTK_BOX(box), play, FALSE, FALSE, 0);

  GtkWidget * scale = gui->replay.scale = gtk_scale_new(GTK_ORIENTATION_HORIZONTAL, NULL);
  gtk_box_pack_start(GTK_BOX(sidebox_box), scale, FALSE, FALSE, 0);
  gtk_scale_set_draw_value(GTK_SCALE(scale), FALSE);
  gtk_range_set_range(GTK_RANGE(GUI->replay.scale), 0.0, 100.0);
  g_signal_connect(G_OBJECT(scale), "value-changed", G_CALLBACK(on_replay_sidebox_scale_value_changed), (void *) NULL);
  
  GtkWidget * entry_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 3);
  gtk_box_pack_start(GTK_BOX(sidebox_box), entry_box, FALSE, FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(entry_box), 5);
  GtkWidget * entry;
  entry= gui->replay.entry = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(entry_box), entry, FALSE, FALSE, 0);
  gtk_entry_set_width_chars(GTK_ENTRY(entry), 12);
  gtk_entry_set_text(GTK_ENTRY(entry), "0");
  gtk_widget_set_tooltip_text(entry, "Current time");
  g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_replay_sidebox_entry_activated), (void *) NULL);
  entry = gui->replay.time_step_entry = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(entry_box), entry, FALSE, FALSE, 0);
  gtk_entry_set_width_chars(GTK_ENTRY(entry), 7);
  gtk_widget_set_tooltip_text(entry, "Time step");
  g_signal_connect(G_OBJECT(entry), "activate", G_CALLBACK(on_replay_sidebox_time_step_entry_activated), (void *) NULL);
  
  GtkWidget * play_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
  gtk_box_pack_start(GTK_BOX(sidebox_box), play_box, FALSE, FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(play_box), 5);
  GtkWidget * button;
  button = gtk_button_new_with_label("Play");
  gtk_box_pack_end(GTK_BOX(play_box), button, FALSE, FALSE, 0);
  button = gtk_button_new_with_label("Next");
  gtk_box_pack_end(GTK_BOX(play_box), button, FALSE, FALSE, 0);
  //g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_replay_sidebox_next_button_clicked), (void *) NULL);
  g_signal_connect(G_OBJECT(button), "button-press-event", G_CALLBACK(on_replay_sidebox_next_button_pressed), (void *) NULL);
  button = gtk_button_new_with_label("Prev");
  gtk_box_pack_end(GTK_BOX(play_box), button, FALSE, FALSE, 0);
  //g_signal_connect(G_OBJECT(button), "clicked", G_CALLBACK(on_replay_sidebox_prev_button_clicked), (void *) NULL);
  g_signal_connect(G_OBJECT(button), "button-press-event", G_CALLBACK(on_replay_sidebox_prev_button_pressed), (void *) NULL);
  
  /*
  gtk_box_pack_start(GTK_BOX(sidebox_box), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
  if (DVG->activeV) {
    dm_dag_t * D = DVG->activeV->D;
    GtkWidget * scrolled = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(sidebox_box), scrolled, TRUE, TRUE, 0);
    GtkWidget * listbox = gtk_list_box_new();
    gtk_container_add(GTK_CONTAINER(scrolled), listbox);
    //gtk_list_box_set_sort_func(GTK_LIST_BOX(listbox), (GtkListBoxSortFunc) gtk_message_row_sort, listbox, NULL);
    //gtk_list_box_set_activate_on_single_click(GTK_LIST_BOX (listbox), FALSE);
    //g_signal_connect(G_OBJECT(listbox), "row-selected", G_CALLBACK(on_replay_sidebox_row_selected), NULL);  
    //g_signal_connect(G_OBJECT(listbox), "row-activated", G_CALLBACK(on_replay_sidebox_row_activated), NULL);
    //gtk_list_box_row_set_activatable(GTK_LIST_BOX(listbox), FALSE);
    //gtk_list_box_row_set_selectable(GTK_LIST_BOX(listbox), FALSE);

    for (i = 0; i < D->P->num_workers; i++) {
      char s[DV_STRING_LENGTH];
      sprintf(s, "Worker %d:", i);
      GtkWidget * row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
      gtk_container_add(GTK_CONTAINER(listbox), row);
      gtk_container_set_border_width(GTK_CONTAINER(row), 5);
      GtkWidget * hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
      gtk_container_add(GTK_CONTAINER(row), hbox);
      GtkWidget * label = gtk_label_new(s);
      gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
      GtkWidget * button = gtk_button_new_with_label("Jump to");
      gtk_box_pack_end(GTK_BOX(hbox), button, FALSE, FALSE, 0);
      GtkWidget * checkbox = gtk_check_button_new_with_label("watch");
      gtk_box_pack_start(GTK_BOX(row), checkbox, FALSE, FALSE, 0);
    
      gtk_box_pack_start(GTK_BOX(row), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 0);
      gtk_widget_show(GTK_WIDGET(row));
    }
  }
  */

  if (DVG->activeV)
    dv_gui_replay_sidebox_set_dag(DVG->activeV->D, 1);
    
  gtk_widget_show_all(sidebox);
}

GtkWidget *
dv_gui_get_replay_sidebox(dv_gui_t * gui) {
  if (!gui->replay.sidebox)
    dv_gui_build_replay_sidebox(gui);
  return gui->replay.sidebox;
}

void
dv_gui_nodeinfo_set_node(dv_gui_t * gui, dm_dag_node_t * node, dm_dag_t * D) {
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, node);
  char s[DV_STRING_LENGTH];

  sprintf(s, "%s",
          dv_get_node_kind_name(pi->info.kind));
  gtk_label_set_text(GTK_LABEL(gui->nodeinfo.type), s);
  sprintf(s, "%ld",
          pi - D->P->T);
  gtk_label_set_text(GTK_LABEL(gui->nodeinfo.offset), s);
  sprintf(s, "%d",
          node->d);
  gtk_label_set_text(GTK_LABEL(gui->nodeinfo.depth), s);
  sprintf(s, "%ld/%ld/%ld",
          pi->info.cur_node_count,
          pi->info.min_node_count,
          pi->info.n_child_create_tasks);
  gtk_label_set_text(GTK_LABEL(gui->nodeinfo.cur_node_count), s);
  
  sprintf(s, "%11.0lf",
          pi->info.start.t - D->bt);
  gtk_label_set_text(GTK_LABEL(gui->nodeinfo.start_time), s);
  sprintf(s, "%11.0lf",
          pi->info.end.t - D->bt);
  gtk_label_set_text(GTK_LABEL(gui->nodeinfo.end_time), s);
  sprintf(s, "%llu",
          pi->info.end.t - pi->info.start.t);
  gtk_label_set_text(GTK_LABEL(gui->nodeinfo.duration), s);

  sprintf(s, "%llu",
          pi->info.first_ready_t);
  gtk_label_set_text(GTK_LABEL(gui->nodeinfo.first_ready_t), s);
  sprintf(s, "%llu",
          pi->info.est);
  gtk_label_set_text(GTK_LABEL(gui->nodeinfo.est), s);

  sprintf(s, "%llu",
          pi->info.t_1);
  gtk_label_set_text(GTK_LABEL(gui->nodeinfo.t1), s);
  sprintf(s, "%llu",
          pi->info.t_inf);
  gtk_label_set_text(GTK_LABEL(gui->nodeinfo.tinf), s);

  sprintf(s, " ");
  int i;
  for (i = 0; i < dr_max_counters - 1; i++) {
    sprintf(s, "%s%lld/",
            s,
            pi->info.counters_1[i]);
  }
  sprintf(s, "%s%lld",
          s,
          pi->info.counters_1[i]);
  gtk_label_set_text(GTK_LABEL(gui->nodeinfo.counters), s);

  sprintf(s, "%d",
          pi->info.worker);
  gtk_label_set_text(GTK_LABEL(gui->nodeinfo.worker), s);
  sprintf(s, "%d",
          pi->info.cpu);
  gtk_label_set_text(GTK_LABEL(gui->nodeinfo.cpu), s);

  const char * ss = D->P->S->C + D->P->S->I[pi->info.start.pos.file_idx];
  sprintf(s, "%s:%ld",
          ss,
          pi->info.start.pos.line);
  gtk_label_set_text(GTK_LABEL(gui->nodeinfo.start_pos), s);
  ss = D->P->S->C + D->P->S->I[pi->info.end.pos.file_idx];
  sprintf(s, "%s:%ld",
          ss,
          pi->info.end.pos.line);  
  gtk_label_set_text(GTK_LABEL(gui->nodeinfo.end_pos), s);

  gtk_widget_queue_draw(gui->nodeinfo.sidebox);
}

static void
dv_gui_build_nodeinfo_sidebox(dv_gui_t * gui) {
  GtkWidget * sidebox = gui->nodeinfo.sidebox = gtk_frame_new("Node Info");
  gtk_container_set_border_width(GTK_CONTAINER(sidebox), 5);
  g_object_ref(sidebox);

  GtkWidget * scrolled_window = gtk_scrolled_window_new(NULL, NULL);
  gtk_container_add(GTK_CONTAINER(sidebox), scrolled_window);
  gtk_widget_set_size_request(GTK_WIDGET(scrolled_window), 230, 450);

  GtkWidget * sidebox_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(scrolled_window), sidebox_box);
  gtk_container_set_border_width(GTK_CONTAINER(sidebox_box), 5);

  GtkWidget * grid = gtk_grid_new();
  gtk_box_pack_start(GTK_BOX(sidebox_box), grid, FALSE, FALSE, 0);
  gtk_grid_set_row_spacing(GTK_GRID(grid), 5);
  gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
  
  int num;
  GtkWidget * label;
  GtkWidget * widget;

  num = 0;
  label = gtk_label_new("Type:");
  widget = gui->nodeinfo.type = gtk_label_new("<none>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_label_set_selectable(GTK_LABEL(widget), TRUE);
  
  num++;
  label = gtk_label_new("Offset:");
  widget = gui->nodeinfo.offset = gtk_label_new("<none>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_label_set_selectable(GTK_LABEL(widget), TRUE);
  
  num++;
  label = gtk_label_new("Depth:");
  widget = gui->nodeinfo.depth = gtk_label_new("<none>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_label_set_selectable(GTK_LABEL(widget), TRUE);

  num++;
  label = gtk_label_new("Nodes:");
  widget = gui->nodeinfo.cur_node_count = gtk_label_new("<none>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  gtk_widget_set_tooltip_text(widget, "cur/min/create_node_count");
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_label_set_selectable(GTK_LABEL(widget), TRUE);

  num++;
  label = gtk_label_new("Start time:");
  widget = gui->nodeinfo.start_time = gtk_label_new("<none>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_label_set_selectable(GTK_LABEL(widget), TRUE);

  num++;
  label = gtk_label_new("End time:");
  widget = gui->nodeinfo.end_time = gtk_label_new("<none>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_label_set_selectable(GTK_LABEL(widget), TRUE);

  num++;
  label = gtk_label_new("Duration:");
  widget = gui->nodeinfo.duration = gtk_label_new("<none>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_label_set_selectable(GTK_LABEL(widget), TRUE);

  num++;
  label = gtk_label_new("First ready:");
  widget = gui->nodeinfo.first_ready_t = gtk_label_new("<none>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_label_set_selectable(GTK_LABEL(widget), TRUE);

  num++;
  label = gtk_label_new("Est:");
  widget = gui->nodeinfo.est = gtk_label_new("<none>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_label_set_selectable(GTK_LABEL(widget), TRUE);

  num++;
  label = gtk_label_new("t_1:");
  widget = gui->nodeinfo.t1 = gtk_label_new("<none>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_label_set_selectable(GTK_LABEL(widget), TRUE);

  num++;
  label = gtk_label_new("t_inf:");
  widget = gui->nodeinfo.tinf = gtk_label_new("<none>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_label_set_selectable(GTK_LABEL(widget), TRUE);

  num++;
  label = gtk_label_new("Counters:");
  widget = gui->nodeinfo.counters = gtk_label_new("<none>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_label_set_selectable(GTK_LABEL(widget), TRUE);

  num++;
  label = gtk_label_new("Worker:");
  widget = gui->nodeinfo.worker = gtk_label_new("<none>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_label_set_selectable(GTK_LABEL(widget), TRUE);

  num++;
  label = gtk_label_new("CPU:");
  widget = gui->nodeinfo.cpu = gtk_label_new("<none>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_label_set_selectable(GTK_LABEL(widget), TRUE);

  num++;
  label = gtk_label_new("Start code:");
  widget = gui->nodeinfo.start_pos = gtk_label_new("<none>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_label_set_selectable(GTK_LABEL(widget), TRUE);

  num++;
  label = gtk_label_new("End code:");
  widget = gui->nodeinfo.end_pos = gtk_label_new("<none>");
  gtk_grid_attach(GTK_GRID(grid), label, 0, num, 1, 1);
  gtk_grid_attach(GTK_GRID(grid), widget, 1, num, 1, 1);
  gtk_widget_set_halign(label, GTK_ALIGN_END);
  gtk_label_set_selectable(GTK_LABEL(widget), TRUE);

  gtk_widget_show_all(sidebox);
}

GtkWidget *
dv_gui_get_nodeinfo_sidebox(dv_gui_t * gui) {
  if (!gui->nodeinfo.sidebox)
    dv_gui_build_nodeinfo_sidebox(gui);
  return gui->nodeinfo.sidebox;
}


_static_unused_ void
dv_open_gui_1(_unused_ int argc, _unused_ char * argv[]) {
  GtkBuilder * builder = gtk_builder_new_from_file("gui/dagviz.ui");
  
  /* Window */
  GtkWidget * window = (GtkWidget *) gtk_builder_get_object(builder, "window");
  gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
  gtk_window_maximize(GTK_WINDOW(window));
  g_signal_connect(G_OBJECT(window), "destroy", G_CALLBACK(gtk_main_quit), NULL);
  g_signal_connect(G_OBJECT(GUI->window), "key-press-event", G_CALLBACK(on_window_key_event), NULL);

  /* Run main loop */
  gtk_widget_show_all(window);
}

_static_unused_ void
dv_open_gui(_unused_ int argc, _unused_ char * argv[], GtkApplication * app) {
  /* Main window */
  GtkWidget * window = dv_gui_get_main_window(GUI, app);

  /* Viewports */
  dv_viewport_t * VP = DVG->VP;
  gtk_box_pack_end(GTK_BOX(GUI->main_box), VP->frame, TRUE, TRUE, 0);
  dv_viewport_show(VP);

  /* Status bars */
  dv_statusbar_update_selection_status();
  dv_statusbar_update_pool_status();
  dv_statusbar_update_pointer_status();

  /* Run main loop */
  gtk_widget_show_all(window);
}


_static_unused_ void
dv_alarm_set() {
  alarm(3);
}

_unused_ void
dv_signal_handler(int signo) {
  if (signo == SIGALRM) {
    fprintf(stderr, "received SIGALRM\n");
    dv_get_callpath_by_libunwind();
    dv_get_callpath_by_backtrace();
  } else
    fprintf(stderr, "received unknown signal\n");
  dv_alarm_set();
}

_static_unused_ void
dv_alarm_init() {
  if (signal(SIGALRM, dv_signal_handler) == SIG_ERR)
    fprintf(stderr, "cannot catch SIGALRM\n");
  else
    dv_alarm_set();
}


/*-----------------BTSAMPLE_VIEWER's functions-----------*/

void dv_btsample_viewer_init(dv_btsample_viewer_t * btviewer) {
  btviewer->label_dag_file_name = gtk_label_new(0);
  btviewer->entry_bt_file_name = gtk_entry_new();
  btviewer->entry_binary_file_name = gtk_entry_new();
  btviewer->combobox_worker = gtk_combo_box_text_new();
  btviewer->entry_time_from = gtk_entry_new();
  btviewer->entry_time_to = gtk_entry_new();
  btviewer->text_view = gtk_text_view_new();
  btviewer->entry_node_id = gtk_entry_new();
  g_object_ref(btviewer->label_dag_file_name);
  g_object_ref(btviewer->entry_bt_file_name);
  g_object_ref(btviewer->entry_binary_file_name);
  g_object_ref(btviewer->combobox_worker);
  g_object_ref(btviewer->entry_time_from);
  g_object_ref(btviewer->entry_time_to);
  g_object_ref(btviewer->text_view);
  g_object_ref(btviewer->entry_node_id);
}

#ifdef DV_ENABLE_BFD
static asymbol **syms;           /* Symbol table.  */
static bfd_vma pc;
static const char *filename;
static const char *functionname;
static unsigned int line;
static bfd_boolean found;

static void slurp_symtab(bfd *);
static void find_address_in_section(bfd *, asection *, void *);
static void translate_addresses(int, bfd *, bt_sample_t *, dv_btsample_viewer_t *);
static void process_one_sample(int, const char *, const char *, bt_sample_t *, dv_btsample_viewer_t *);

/* Read in the symbol table.  */
static void
slurp_symtab(bfd *abfd) {
  long symcount;
  unsigned int size;

  if ((bfd_get_file_flags(abfd) & HAS_SYMS) == 0)
    return;

  symcount = bfd_read_minisymbols(abfd, FALSE, (void *) &syms, &size);
  if (symcount == 0)
    symcount = bfd_read_minisymbols(abfd, TRUE /* dynamic */, (void *) &syms, &size);

  if (symcount < 0) {
    //bfd_fatal(bfd_get_filename(abfd));
    perror(bfd_get_filename(abfd));
    exit(1);
  }
}

/* Look for an address in a section.  This is called via
   bfd_map_over_sections.  */
static void
find_address_in_section(bfd *abfd, asection *section, void *data ATTRIBUTE_UNUSED) {
  bfd_vma vma;
  bfd_size_type size;

  if (found)
    return;

  if ((bfd_get_section_flags (abfd, section) & SEC_ALLOC) == 0)
    return;

  vma = bfd_get_section_vma (abfd, section);
  if (pc < vma)
    return;

  size = bfd_get_section_size (section);
  if (pc >= vma + size)
    return;

  found = bfd_find_nearest_line (abfd, section, syms, pc - vma,
                                 &filename, &functionname, &line);
}

/* Read hexadecimal addresses from stdin, translate into
   file_name:line_number and optionally function name.  */
static void
translate_addresses(int c, bfd *abfd, bt_sample_t *s, dv_btsample_viewer_t *btviewer) {
  char str_frame[200];
  GtkTextBuffer *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(btviewer->text_view));
  
  char addr[30];
  int i;
  for (i=0; i<s->n; i++) {
    sprintf(addr, "%p", s->frames[i]);
    pc = bfd_scan_vma(addr, NULL, 16);
    
    found = FALSE;
    bfd_map_over_sections(abfd, find_address_in_section, NULL);
    
    if (!found) {
      sprintf(str_frame, "[%d] : Worker %d : Clock %llu : Frame %d : Addr %s : ?? : ?? : ??\n",
              c,
              s->worker,
              s->tsc - btviewer->P->start_clock,
              i,
              addr);
    } else {
      sprintf(str_frame, "[%d] : Worker %d : Clock %llu : Frame %d : Addr %s : %s : %s : %d\n",
              c,
              s->worker,
              s->tsc - btviewer->P->start_clock,
              i,
              addr,
              functionname ? functionname : "??",
              filename ? filename : "??",
              line);
    }
    gtk_text_buffer_insert_at_cursor(buffer, str_frame, -1);
  }
  sprintf(str_frame, "\n");
  gtk_text_buffer_insert_at_cursor(buffer, str_frame, -1);
}

static void
process_one_sample(int c, const char *bin_file, const char *target, bt_sample_t *s, dv_btsample_viewer_t *btviewer) {
  bfd *abfd;
  char **matching;

  abfd = bfd_openr(bin_file, target);
  if (abfd == NULL) {
    //bfd_fatal(bin_file);
    perror(bin_file);
    exit(1);
  }

  if (bfd_check_format(abfd, bfd_archive)) {
    //fatal (_("%s: can not get addresses from archive"), bin_file);
    perror("can not get addresses from archive");
    exit(1);
  }

  if (!bfd_check_format_matches(abfd, bfd_object, &matching)) {
    //bfd_nonfatal(bfd_get_filename(abfd));
    perror(bfd_get_filename(abfd));
    if (bfd_get_error() == bfd_error_file_ambiguously_recognized) {
      //list_matching_formats(matching);
      free(matching);
    }
    exit(1);
  }

  slurp_symtab(abfd);

  translate_addresses(c, abfd, s, btviewer);

  if (syms != NULL) {
    free (syms);
    syms = NULL;
  }

  bfd_close(abfd);
}
#endif /* DV_ENABLE_BFD */

int
dv_btsample_viewer_extract_interval(dv_btsample_viewer_t * btviewer, _unused_ int worker, _unused_ unsigned long long from, _unused_ unsigned long long to) {
#ifdef DV_ENABLE_BFD  
  const char * binary_file = gtk_entry_get_text(GTK_ENTRY(btviewer->entry_binary_file_name));
  const char * btsample_file = gtk_entry_get_text(GTK_ENTRY(btviewer->entry_bt_file_name));

  bfd_init();
  //set_default_bfd_target();

  FILE * fi = fopen(btsample_file, "r");
  if (!fi) {
    perror("fopen btsample_file");
    return 0;
  }

  int c = 0;
  bt_sample_t s[1];
  while (fread(s, sizeof(bt_sample_t), 1, fi))
    if (s->worker == worker && s->tsc >= from && s->tsc <= to)
      process_one_sample(c++, binary_file, NULL, s, btviewer);

  fclose(fi);
#else  
  GtkTextBuffer * buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(btviewer->text_view));
  gtk_text_buffer_insert_at_cursor(buffer, "Error: BFD is not enabled.", -1);
#endif /* DV_ENABLE_BFD */
  return 0;
}

/*-----------------end of BTSAMPLE_VIEWER's functions-----------*/

