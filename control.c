
/****************** GUI Callbacks **************************************/

static gboolean
on_draw_event(_unused_ GtkWidget * widget, cairo_t * cr, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  dv_viewport_draw(VP, cr);
  return FALSE;
}

static void
on_btn_zoomfit_full_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  dv_view_do_zoomfit_full(VP->mainV);
}

static void
on_btn_shrink_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  dv_do_collapsing_one(VP->mainV);
}

static void
on_btn_expand_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  dv_do_expanding_one(VP->mainV);
}

static gboolean
on_scroll_event(_unused_ GtkWidget * widget, GdkEventScroll * event, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  if ( VP->mV[ CS->activeV - CS->V ] ) {
    dv_do_scrolling(CS->activeV, event);
  } else {
    int i;
    for (i = 0; i < CS->nV; i++)
      if (VP->mV[i])
        dv_do_scrolling(&CS->V[i], event);
  }
  return TRUE;
}

static gboolean
on_button_event(_unused_ GtkWidget * widget, GdkEventButton * event, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  if ( VP->mV[ CS->activeV - CS->V ] ) {
    dv_do_button_event(CS->activeV, event);
  } else {
    int i;
    for (i = 0; i < CS->nV; i++)
      if ( VP->mV[i] )
        dv_do_button_event(&CS->V[i], event);
  }
  return TRUE;
}

static gboolean
on_motion_event(_unused_ GtkWidget * widget, GdkEventMotion * event, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  if ( VP->mV[ CS->activeV - CS->V ] ) {
    dv_do_motion_event(CS->activeV, event);
  } else {
    int i;
    for (i = 0; i < CS->nV; i++)
      if ( VP->mV[i] )
        dv_do_motion_event(&CS->V[i], event);
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

static gboolean
on_combobox_sdt_changed(GtkComboBox * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  dv_view_change_sdt(V, gtk_combo_box_get_active(GTK_COMBO_BOX(widget)));
  dv_view_layout(V);
  dv_view_auto_zoomfit(V);
  dv_queue_draw_d(V);
  return TRUE;
}

static gboolean
on_entry_radix_activate(GtkEntry * entry, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  double radix = atof(gtk_entry_get_text(GTK_ENTRY(entry)));
  dv_view_change_radix(V, radix);
  dv_view_layout(V);
  dv_queue_draw_d(V);
  return TRUE;
}

static gboolean
on_entry_remark_activate(GtkEntry * entry, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  /* Read string */
  const char * str = gtk_entry_get_text(GTK_ENTRY(entry));
  char str2[strlen(str) + 1];
  strcpy(str2, str);
  /* Read values in string to array */
  V->D->nr = 0;
  long v;
  char * p = str2;
  while (*p) {
    if (isdigit(*p)) {
      v = strtol(p, &p, 10);
      if (V->D->nr < DV_MAX_NUM_REMARKS)
        V->D->ar[V->D->nr++] = v;
    } else {
      p++;
    }
  }
  /* Convert read values back to string */
  char str3[V->D->nr * 3 + 1];
  str3[0] = 0;
  int i;
  for (i = 0; i < V->D->nr; i++) {
    v = V->D->ar[i];
    if (i == 0)
      sprintf(str2, "%ld", v);
    else
      sprintf(str2, ", %ld", v);
    strcat(str3, str2);
  }
  dv_view_set_entry_remark_text(V, str3);
  dv_queue_draw_d(V);
  return TRUE;
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
  dv_node_coordinate_t *co = &node->c[S->coord];
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
on_togg_eaffix_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget))) {
    dv_view_change_eaffix(V, 1);
  } else {
    dv_view_change_eaffix(V, 0);
  }
  dv_queue_draw(V);
}

/*
static void
on_togg_focused_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  int focused = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(widget));
  dv_do_set_focused_view(VP->mainV, focused);
}
*/

static gboolean
on_combobox_cm_changed(GtkComboBox * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->cm = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  return TRUE;
}

static gboolean
on_combobox_hm_changed(GtkComboBox * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->hm = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  return TRUE;
}

static gboolean
on_combobox_azf_changed(GtkComboBox * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  int new_val = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  dv_view_change_azf(V, new_val);
  return TRUE;
}

static gboolean
on_darea_configure_event(_unused_ GtkWidget * widget, GdkEventConfigure * event, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  VP->vpw = event->width;
  VP->vph = event->height;
  dv_view_t * V = NULL;
  int i;
  for (i=0; i<CS->nV; i++)
    if (VP->mV[i]) {
      V = CS->V + i;
      if (V->mainVP == VP) {
        V->S->vpw = event->width;
        V->S->vph = event->height;
      }
    }
  return TRUE;
}

static gboolean
on_window_key_event(_unused_ GtkWidget * widget, GdkEvent * event, _unused_ gpointer user_data) {
  dv_view_t * aV = dv_global_state_get_active_view();
  if (!aV)
    return FALSE;
  
  GdkModifierType modifiers = gtk_accelerator_get_default_mod_mask();
  
  GdkEventKey * e = (GdkEventKey *) event;
  int i;
  //printf("key: %d\n", e->keyval);
  switch (e->keyval) {
  case 120: /* X */
  case 43: /* + */
    dv_do_expanding_one(aV);    
    return TRUE;
  case 99: /* C */
  case 45: /* - */
    dv_do_collapsing_one(aV);
    return TRUE;
  case 104: /* H */
    if (aV->S->adjust_auto_zoomfit)
      dv_view_change_azf(aV, 1); // auto zoom fit horizontal
    dv_view_do_zoomfit_hor(aV);
    return TRUE;
  case 118: /* V */
    if (aV->S->adjust_auto_zoomfit)
      dv_view_change_azf(aV, 2); // auto zoom fit vertical
    dv_view_do_zoomfit_ver(aV);
    return TRUE;
  case 102: /* F */
    if (aV->S->adjust_auto_zoomfit)
      dv_view_change_azf(aV, 4); // auto zoom fit full
    dv_view_do_zoomfit_full(aV);
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
  case 65289: /* Tab */
    if (!aV)
      i = 0;
    else {
      i = aV - CS->V;
      int boo = 0;
      int count = 0;
      while (boo == 0 && count < CS->nV) {
        i = (i + 1) % CS->nV;
        int j;
        for (j = 0; j < CS->nVP; j++)
          if (CS->V[i].mVP[j])
            boo = 1;
        count++;
      }
    }
    dv_do_set_focused_view(CS->V + i, 1);
    break;
  case 65361: /* Left */
    aV->S->x += 15;
    dv_queue_draw(aV);
    return TRUE;
  case 65362: /* Up */
    aV->S->y += 15;
    dv_queue_draw(aV);
    return TRUE;
  case 65363: /* Right */
    aV->S->x -= 15;
    dv_queue_draw(aV);
    return TRUE;
  case 65364: /* Down */
    aV->S->y -= 15;
    dv_queue_draw(aV);
    return TRUE;
  case 116: /* T */
    fprintf(stderr, "open toolbox window of V %ld\n", aV - CS->V);
    dv_view_open_toolbox_window(aV);
    return TRUE;
  default:
    return FALSE;
  }
  return FALSE;
}

static void
on_viewport_tool_icon_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  dv_view_open_toolbox_window(VP->mainV);
}

static void
on_btn_choose_bt_file_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
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

static void
on_btn_choose_binary_file_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
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

static void
on_btn_find_node_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  dv_btsample_viewer_t * btviewer = (dv_btsample_viewer_t *) user_data;
  long pii = atol(gtk_entry_get_text(GTK_ENTRY(btviewer->entry_node_id)));
  //dv_dag_node_t *node = dv_find_node_with_pii_r(CS->activeV, pii, CS->activeV->D->rt);
  dr_pi_dag_node *pi = dv_pidag_get_node_by_id(btviewer->P, pii);
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

static void
on_btn_run_view_bt_samples_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
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
on_checkbox_xzoom_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->do_zoom_x = 1 - V->S->do_zoom_x;
}

static void
on_checkbox_yzoom_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->do_zoom_y = 1 - V->S->do_zoom_y;
}

static void
on_checkbox_scale_radix_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->do_scale_radix = 1 - V->S->do_scale_radix;
}

static void
on_checkbox_legend_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->show_legend = 1 - V->S->show_legend;
  dv_queue_draw(V);
}

static void
on_checkbox_status_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->show_status = 1 - V->S->show_status;
  dv_queue_draw(V);
}

static void
on_checkbox_remain_inner_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->remain_inner = 1 - V->S->remain_inner;
}

static void
on_checkbox_color_remarked_only_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->color_remarked_only = 1 - V->S->color_remarked_only;
  dv_queue_draw(V);
}

static void
on_checkbox_scale_radius_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->show_legend = 1 - V->S->show_legend;
}

static void
on_checkbox_azf_toggled(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  V->S->adjust_auto_zoomfit = 1 - V->S->adjust_auto_zoomfit;
}

static void
on_btn_run_dag_scan_clicked(_unused_ GtkButton * button, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  dv_view_scan(V);
  dv_queue_draw_d(V);
}

static void
on_btn_zoomfit_hor_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  dv_view_do_zoomfit_hor(V);
}

static void
on_btn_zoomfit_ver_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  dv_view_do_zoomfit_ver(V);
}

static void
on_toolbar_settings_button_clicked(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
  if (!CS->activeV) return;
  dv_view_open_toolbox_window(CS->activeV);
}

void
on_toolbar_dag_menu_item_activated(GtkMenuItem * menuitem, gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  if (V) {
    dv_view_open_toolbox_window(V);
    return;
  }
  /* Iterate D and V */
  const gchar * label_str = gtk_menu_item_get_label(GTK_MENU_ITEM(menuitem));
  int i, j;
  for (i = 0; i < CS->nD; i++) {
    dv_dag_t * D = &CS->D[i];
    if (strcmp(D->name, label_str) == 0) {
      dv_view_t * V = NULL;
      int c = 0;
      for (j = 0; j < CS->nV; j++)
        if (D->mV[j]) {
          V = &CS->V[j];
          c++;
        }
      if (c == 1)
        dv_view_open_toolbox_window(V);
      return;
    }
  }  
}

static void
on_toolbar_zoomfit_button_clicked(_unused_ GtkToolButton * toolbtn, gpointer user_data) {
  if (!CS->activeV) return;
  long i = (long) user_data;
  switch (i) {
  case 0:
    dv_view_do_zoomfit_full(CS->activeV);
    break;
  case 1:
    dv_view_do_zoomfit_hor(CS->activeV);
    break;
  case 2:
    dv_view_do_zoomfit_ver(CS->activeV);
    break;
  }
}

static void
on_toolbar_shrink_button_clicked(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
  if (!CS->activeV) return;
  dv_do_collapsing_one(CS->activeV);
}

static void
on_toolbar_expand_button_clicked(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
  if (!CS->activeV) return;
  dv_do_expanding_one(CS->activeV);
}

G_MODULE_EXPORT void
on_menubar_export_activated(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
  dv_export_viewport();
}

G_MODULE_EXPORT void
on_menubar_export_all_activated(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
  dv_export_all_viewports();
}

G_MODULE_EXPORT void
on_menubar_hotkeys_activated(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
  // Build dialog
  GtkWidget * dialog = gtk_dialog_new();
  gtk_window_set_title(GTK_WINDOW(dialog), "Hotkeys");
  gtk_window_set_default_size(GTK_WINDOW(dialog), 300, 170);
  GtkWidget * dialog_vbox = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

  // Label
  GtkWidget * label = gtk_label_new("\n"
                                    "Tab : switch control between VIEWs\n"
                                    "Ctrl + 1 : dag view\n"
                                    "Ctrl + 2 : dagbox view\n"
                                    "Ctrl + 3 : vertical timeline view\n"
                                    "Ctrl + 4 : horizontal timeline view\n"
                                    "X : expand\n"
                                    "C : collapse\n"
                                    "H : horizontal fit\n"
                                    "V : vertical fit\n"
                                    "A : open view configuration dialog\n"
                                    "Alt + [some key] : access menu\n"
                                    "Arrow Keys : move around\n");
  gtk_box_pack_start(GTK_BOX(dialog_vbox), label, TRUE, FALSE, 0);

  // Run
  gtk_widget_show_all(dialog_vbox);
  gtk_dialog_run(GTK_DIALOG(dialog));

  // Destroy
  gtk_widget_destroy(dialog);  
}

G_MODULE_EXPORT void
on_menubar_view_samples_activated(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
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

G_MODULE_EXPORT void
on_menubar_statistics_activated(_unused_ GtkMenuItem * menuitem, _unused_ gpointer user_data) {
  dv_open_statistics_dialog();
}

/*
static GtkWidget *
dv_viewport_create_frame(dv_viewport_t * VP) {
  char s[DV_STRING_LENGTH];
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
  gtk_box_pack_start(GTK_BOX(box), gtk_separator_new(GTK_ORIENTATION_VERTICAL), FALSE, FALSE, 4);

  // Right
  GtkWidget * right = gtk_scrolled_window_new(NULL, NULL);
  gtk_box_pack_start(GTK_BOX(box), right, FALSE, FALSE, 3);
  gtk_widget_set_size_request(GTK_WIDGET(right), 315, 0);
  GtkWidget * right_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
  gtk_container_add(GTK_CONTAINER(right), right_box);
  int i;
  for (i=0; i<CS->nVP; i++) {
    char s[DV_STRING_LENGTH];
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

G_MODULE_EXPORT void
on_menubar_manage_viewports_activated_old(_unused_ GtkMenuItem * menuitem, _unused_ gpointer user_data) {
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
*/

G_MODULE_EXPORT void
on_menubar_manage_viewports_activated(_unused_ GtkMenuItem * menuitem, _unused_ gpointer user_data) {
  GtkWidget * management_window = dv_gui_get_management_window(GUI);
  gtk_widget_show_all(management_window);
  gtk_notebook_set_current_page(GTK_NOTEBOOK(GUI->notebook), 0);
}

G_MODULE_EXPORT void
on_menubar_manage_dags_activated(_unused_ GtkMenuItem * menuitem, _unused_ gpointer user_data) {
  GtkWidget * management_window = dv_gui_get_management_window(GUI);
  gtk_widget_show_all(management_window);
  gtk_notebook_set_current_page(GTK_NOTEBOOK(GUI->notebook), 1); // effective only when being called after the window is shown for the first time
}

static gboolean
on_management_window_viewport_options_split_changed(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  int active = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  dv_viewport_change_split(VP, active);

  /* Redraw viewports */
  gtk_widget_show_all(GTK_WIDGET(VP->frame));
  gtk_widget_queue_draw(GTK_WIDGET(VP->frame));
  /* Redraw management window */
  gtk_widget_show_all(GTK_WIDGET(VP->mini_frame));
  gtk_widget_queue_draw(GTK_WIDGET(VP->mini_frame));
  gtk_widget_show_all(GTK_WIDGET(VP->mini_frame_2));
  gtk_widget_queue_draw(GTK_WIDGET(VP->mini_frame_2));
  
  return TRUE;
}

static gboolean
on_management_window_viewport_options_orientation_changed(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  int active = gtk_combo_box_get_active(GTK_COMBO_BOX(widget));
  GtkOrientation o = (active==0)?GTK_ORIENTATION_HORIZONTAL:GTK_ORIENTATION_VERTICAL;
  dv_viewport_change_orientation(VP, o);

  /* Redraw viewports */
  gtk_widget_show_all(GTK_WIDGET(VP->frame));
  gtk_widget_queue_draw(GTK_WIDGET(VP->frame));
  /* Redraw management window */
  gtk_widget_show_all(GTK_WIDGET(VP->mini_frame));
  gtk_widget_queue_draw(GTK_WIDGET(VP->mini_frame));
  gtk_widget_show_all(GTK_WIDGET(VP->mini_frame_2));
  gtk_widget_queue_draw(GTK_WIDGET(VP->mini_frame_2));
  
  return TRUE;
}

gboolean
on_management_window_open_stat_button_clicked(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_dag_t * D = (dv_dag_t *) user_data;
  int n = strlen(D->P->fn);
  char * filename = (char *) dv_malloc( sizeof(char) * (n + 2) );
  strcpy(filename, D->P->fn);
  if (strcmp(filename + n - 3, "dag") != 0)
    return FALSE;
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
  return TRUE;
}

gboolean
on_management_window_open_pp_button_clicked(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_dag_t * D = (dv_dag_t *) user_data;
  int n = strlen(D->P->fn);
  char * filename = (char *) dv_malloc( sizeof(char) * (n + 1) );
  strcpy(filename, D->P->fn);
  if (strcmp(filename + n - 3, "dag") != 0)
    return FALSE;
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
  return TRUE;
}

gboolean
on_management_window_expand_dag_button_clicked(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_dag_t * D = (dv_dag_t *) user_data;
  dv_dag_expand_implicitly(D);
  dv_dag_update_status_label(D);
  return TRUE;
}

void
on_management_window_add_new_view_clicked(_unused_ GtkWidget * widget, gpointer user_data) {
  dv_dag_t * D = (dv_dag_t *) user_data;
  dv_view_t * V = dv_create_new_view(D);
  if (V) {
    gtk_widget_show_all(GTK_WIDGET(D->mini_frame));
    gtk_widget_queue_draw(GTK_WIDGET(D->mini_frame));
  }
}

void
on_management_window_view_clicked(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
  dv_view_t * V = (dv_view_t *) user_data;
  if (V)
    dv_view_open_toolbox_window(V);
}

void
on_management_window_add_new_dag_activated(_unused_ GtkMenuItem * menuitem, _unused_ gpointer user_data) {
  dv_pidag_t * P = (dv_pidag_t *) user_data;
  dv_dag_t * D = dv_create_new_dag(P);
  if (D) {
    dv_view_t * V = dv_create_new_view(D);
    if (V) {
      dv_do_expanding_one(V);
      gtk_widget_show_all(GTK_WIDGET(D->mini_frame));
      gtk_widget_queue_draw(GTK_WIDGET(D->mini_frame));
    }
    gtk_widget_show_all(GTK_WIDGET(GUI->scrolled_box));
    gtk_widget_queue_draw(GTK_WIDGET(GUI->scrolled_box));
  }
}

void
on_management_window_viewport_dag_menu_item_toggled(GtkCheckMenuItem * checkmenuitem, gpointer user_data) {
  dv_viewport_t * VP = (dv_viewport_t *) user_data;
  const gchar * label_str = gtk_menu_item_get_label(GTK_MENU_ITEM(checkmenuitem));
  /* Iterate D and V to find the V */
  dv_view_t * V = NULL;
  int i, j;
  for (i = 0; i < CS->nD; i++) {
    dv_dag_t * D = &CS->D[i];
    if (strcmp(D->name, label_str) == 0)
      break;
  }
  if (i < CS->nD) {
    for (j = 0; j < CS->nV; j++)
      if (CS->D[i].mV[j]) {
        V = &CS->V[j];
        break;
      }    
  } else {
    for (j = 0; j < CS->nV; j++)
      if (strcmp(CS->V[j].name, label_str) == 0) {
        V = &CS->V[j];
        break;
      }
  }
  if (!V) {
    fprintf(stderr, "viewport_dag_menu_item_toggled: could not find view (%s)\n", label_str);
    return;
  }
  /* Actions */
  gboolean active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(checkmenuitem));
  if (active) {
    dv_view_add_viewport(V, VP);
  } else {
    dv_view_remove_viewport(V, VP);
  }
  gtk_widget_show_all(GTK_WIDGET(VP->frame));
  gtk_widget_queue_draw(GTK_WIDGET(VP->frame));
}

void
on_toolbar_division_menu_onedag_activated(_unused_ GtkMenuItem * menuitem, _unused_ gpointer user_data) {
  dv_viewport_t * VP = CS->VP;
  dv_dag_t * D = CS->D;
  long i = (long) user_data;
  switch (i) {
  case 1:
    dv_viewport_divide_onedag_1(VP, D);
    break;
  case 2:
    dv_viewport_divide_onedag_2(VP, D);
    break;
  case 3:
    dv_viewport_divide_onedag_3(VP, D);
    break;
  case 4:
    dv_viewport_divide_onedag_4(VP, D);
    break;
  case 5:
    dv_viewport_divide_onedag_5(VP, D);
    break;
  }
}

void
on_toolbar_division_menu_twodags_activated(_unused_ GtkMenuItem * menuitem, _unused_ gpointer user_data) {
  dv_viewport_t * VP = CS->VP;
  if (CS->nD < 2) return;
  dv_dag_t * D1 = CS->D;
  dv_dag_t * D2 = CS->D + 1;
  long i = (long) user_data;
  switch (i) {
  case 1:
    dv_viewport_divide_twodags_1(VP, D1, D2);
    break;
  case 2:
    dv_viewport_divide_twodags_2(VP, D1, D2);
    break;
  case 3:
    dv_viewport_divide_twodags_3(VP, D1, D2);
    break;
  case 4:
    dv_viewport_divide_twodags_4(VP, D1, D2);
    break;
  case 5:
    dv_viewport_divide_twodags_5(VP, D1, D2);
    break;
  case 6:
    dv_viewport_divide_twodags_6(VP, D1, D2);
    break;
  }
}

void
on_toolbar_division_menu_threedags_activated(_unused_ GtkMenuItem * menuitem, _unused_ gpointer user_data) {
  dv_viewport_t * VP = CS->VP;
  if (CS->nD < 3) return;
  dv_dag_t * D1 = CS->D;
  dv_dag_t * D2 = CS->D + 1;
  dv_dag_t * D3 = CS->D + 2;
  long i = (long) user_data;
  switch (i) {
  case 1:
    dv_viewport_divide_threedags_1(VP, D1, D2, D3);
    break;
  }
}

void
on_toolbar_dag_layout_buttons_clicked(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
  if (!CS->activeV) return;
  long i = (long) user_data;
  dv_view_change_lt(CS->activeV, i);
}

void
on_toolbar_dag_boxes_scale_type_menu_activated(_unused_ GtkToolButton * toolbtn, _unused_ gpointer user_data) {
  if (!CS->activeV) return;
  dv_view_t * V = CS->activeV;
  long i = (long) user_data;
  dv_view_change_lt(V, 1);
  dv_view_change_sdt(V, i);
  dv_view_layout(V);
  dv_view_auto_zoomfit(V);
  dv_queue_draw_d(V);
}

void
on_menubar_view_workers_sidebar_toggled(_unused_ GtkCheckMenuItem * checkmenuitem, _unused_ gpointer user_data) {
  printf("sidebar toggled \n");
  GtkWidget * sidebar = dv_gui_get_workers_sidebar(GUI);
  GList * children = gtk_container_get_children(GTK_CONTAINER(GUI->main_box));
  int shown = (children->data == sidebar);
  gboolean active = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(checkmenuitem));
  if (!shown && active)
    gtk_box_pack_start(GTK_BOX(GUI->main_box), sidebar, FALSE, FALSE, 0);
  else if (shown && !active)
    gtk_container_remove(GTK_CONTAINER(GUI->main_box), sidebar);
}

void
on_workers_sidebar_row_selected(_unused_ GtkListBox * box, _unused_ GtkListBoxRow * row, _unused_ gpointer user_data) {
  printf("row selected \n");
}

void
on_workers_sidebar_row_activated(_unused_ GtkListBox * box, _unused_ GtkListBoxRow * row, _unused_ gpointer user_data) {
  printf("row activated \n");
}


/****************** end of GUI Callbacks **************************************/

