#include "dagviz-gtk.h"

/******************Statistical Process**************************************/

char *
dv_filename_get_short_name(char * fn) {
  char * p_from = strrchr(fn, '/');
  char * p_to = strrchr(fn, '.');
  if (!p_from)
    p_from = fn;
  else
    p_from++;
  if (!p_to) p_to = fn + strlen(fn);
  int n = p_to - p_from;
  char * ret = (char *) dv_malloc( sizeof(char) * (n + 1) );
  strncpy(ret, p_from, n);
  //int k;
  //  for (k = 0; k < n; k++)
  //      ret[k] = p_from[k];  
  ret[n] = '\0';
  return ret;
}

void
dv_dag_collect_delays_r(dm_dag_t * D, dm_dag_node_t * node, FILE * out, dv_stat_distribution_entry_t * e) {
  if (!node)
    dv_check(node);

  if (dm_is_union(node) && dm_is_inner_loaded(node)) {
    dv_dag_collect_delays_r(D, node->head, out, e);
  } else {
    dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, node);
    if (pi->info.kind == dr_dag_node_kind_create_task) {
      dr_pi_dag_node * pi_;
      if (e->type == 0)
        pi_ = dm_pidag_get_node_by_dag_node(D->P, node->spawn);
      else
        pi_ = dm_pidag_get_node_by_dag_node(D->P, node->next);
      dv_check(pi_);
      if (!e->stolen || pi_->info.worker != pi->info.worker)
        fprintf(out, "%lld\n", pi_->info.start.t - pi->info.end.t);
    }
  }

  dm_dag_node_t * next = NULL;
  while ( (next = dm_dag_node_traverse_nexts(node, next)) ) {
    dv_dag_collect_delays_r(D, next, out, e);
  }
}

void
dv_dag_collect_sync_delays_r(dm_dag_t * D, dm_dag_node_t * node, FILE * out, dv_stat_distribution_entry_t * e) {
  if (!node)
    dv_check(node);

  if (dm_is_union(node) && dm_is_inner_loaded(node)) {
    dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, node);
    if (pi->info.kind == dr_dag_node_kind_section) {
      dr_pi_dag_node * pi_ = dm_pidag_get_node_by_dag_node(D->P, node->next);
      if (pi_) {
        dr_clock_t last_t = 0;
        dm_dag_node_t * x = NULL;
        while ( (x = dm_dag_node_traverse_tails(node, x)) ) {
          dr_pi_dag_node * x_pi = dm_pidag_get_node_by_dag_node(D->P, x);
          if (x_pi->info.end.t > last_t)
            last_t = x_pi->info.end.t;
        }
        dv_check(last_t);
        fprintf(out, "%lld\n", pi_->info.start.t - last_t);
      }
    }
    dv_dag_collect_sync_delays_r(D, node->head, out, e);
  }

  dm_dag_node_t * next = NULL;
  while ( (next = dm_dag_node_traverse_nexts(node, next)) ) {
    dv_dag_collect_sync_delays_r(D, next, out, e);
  }
}

void
dv_dag_collect_intervals_r(dm_dag_t * D, dm_dag_node_t * node, FILE * out, dv_stat_distribution_entry_t * e) {
  if (!node)
    dv_check(node);

  if (dm_is_union(node) && dm_is_inner_loaded(node)) {
    dv_dag_collect_intervals_r(D, node->head, out, e);
  } else if (!dm_is_union(node) || !dm_is_inner_loaded(node)) {
    dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, node);
    fprintf(out, "%lld\n", pi->info.end.t - pi->info.start.t);
  }

  dm_dag_node_t * next = NULL;
  while ( (next = dm_dag_node_traverse_nexts(node, next)) ) {
    dv_dag_collect_intervals_r(D, next, out, e);
  }
}

static void
dv_dag_expand_implicitly_r(dm_dag_t * D, dm_dag_node_t * node) {
  if (!dm_is_set(node))
    dm_dag_node_set(D, node);
  
  if (dm_is_union(node)) {

    /* Build inner */
    if ( !dm_is_inner_loaded(node) ) {
      if (dm_dag_build_node_inner(D, node) != DV_OK) {
        fprintf(stderr, "error in dm_dag_build_node_inner\n");
        return;
      }
    }
    /* Call inward */
    dv_check(node->head);
    dv_dag_expand_implicitly_r(D, node->head);
    
  }
  
  /* Call link-along */
  dm_dag_node_t * next = NULL;
  while ( (next = dm_dag_node_traverse_nexts(node, next)) ) {
    dv_dag_expand_implicitly_r(D, next);
  }
}

void
dv_dag_expand_implicitly(dm_dag_t * D) {
  dv_dag_expand_implicitly_r(D, D->rt);
}

void
dv_dag_update_status_label(dm_dag_t * D) {
  char s[DV_STRING_LENGTH];
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, D->rt);
  sprintf(s, "nodes: %ld/%ld (%ldMB), elasped: %llu clocks", D->n, D->P->n, D->P->sz / (1 << 20), pi->info.end.t - pi->info.start.t);
  dv_dag_t * g = (dv_dag_t *) D->g;
  if (g->status_label)
    gtk_label_set_text(GTK_LABEL(g->status_label), s);
  else
    g->status_label = gtk_label_new(s);
}

/******************end of Statistical Process**************************************/



void
dv_dag_node_pool_set_status_label(dm_dag_node_pool_t * pool, GtkWidget * label) {
  char str[100];
  sprintf(str, "Node Pool: %ld / %ld (%ldMB)", pool->N - pool->n, pool->N, pool->sz / (1 << 20) );
  gtk_label_set_text(GTK_LABEL(label), str);
}

void
dv_histogram_entry_pool_set_status_label(dm_histogram_entry_pool_t * pool, GtkWidget * label) {
  char str[100];
  sprintf(str, "Entry Pool: %ld / %ld (%ldMB)", pool->N - pool->n, pool->N, pool->sz / (1 << 20) );
  gtk_label_set_text(GTK_LABEL(label), str);
}




/****************** VIEW **************************************/

void
dv_view_get_zoomfit_hor(dv_view_t * V, double * zrx, double * zry, double * myx, double * myy) {
  dm_dag_t * D = V->D;
  dv_view_status_t * S = V->S;
  double w = S->vpw;
  double h = S->vph;
  double zoom_ratio = 1.0;
  double x = 0.0;
  double y = 0.0;
  double d1, d2, dw;
  dm_node_coordinate_t * rtco = &D->rt->c[S->coord];
  switch (S->lt) {
  case DV_LAYOUT_TYPE_DAG:
    // DAG
    d1 = rtco->lw + rtco->rw;
    d2 = w - 2 * DVG->opts.zoom_to_fit_margin;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    x -= (rtco->rw - rtco->lw) * 0.5 * zoom_ratio;
    break;
  case DV_LAYOUT_TYPE_DAG_BOX_LOG:
  case DV_LAYOUT_TYPE_DAG_BOX_POWER:
  case DV_LAYOUT_TYPE_DAG_BOX_LINEAR:
    // DAG with Boxes
    d1 = rtco->lw + rtco->rw;
    d2 = w - 2 * DVG->opts.zoom_to_fit_margin;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    x -= (rtco->rw - rtco->lw) * 0.5 * zoom_ratio;
    break;
  case DV_LAYOUT_TYPE_TIMELINE_VER:
    // Vertical Timeline
    d1 = 2 * D->radius + (D->P->num_workers - 1) * DVG->opts.hnd;
    d2 = w - 2 * DVG->opts.zoom_to_fit_margin;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    break;
  case DV_LAYOUT_TYPE_TIMELINE:
    // Horizontal Timeline
    d1 = 10 + rtco->rw;
    d2 = w - 2 * DVG->opts.zoom_to_fit_margin;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    dw = D->P->num_workers * (D->radius * 2);
    y += (h - dw * zoom_ratio) * 0.4;
    break;
  case DV_LAYOUT_TYPE_PARAPROF: {
    // Parallelism profile
    d1 = dv_dag_scale_down_linear(V->D, V->D->et - V->D->bt);
    d2 = w - 2 * DVG->opts.paraprof_zoom_to_fit_margin;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    y -= V->D->P->num_workers * 2 * V->D->radius * zoom_ratio;
    double max_h = dm_histogram_get_max_height(D->H);
    double dy = (h - DVG->opts.paraprof_zoom_to_fit_margin_bottom
                 - DVG->opts.paraprof_zoom_to_fit_margin
                 - zoom_ratio
                 * (D->P->num_workers * 2 * D->radius
                    + max_h)
                 ) / 2.0;
    if (dy > 0)
      y -= dy;
    break;
  }
  case DV_LAYOUT_TYPE_CRITICAL_PATH: {
    // Critical path
    d1 = dv_dag_scale_down_linear(V->D, V->D->et - V->D->bt);
    d2 = w - 2 * DVG->opts.paraprof_zoom_to_fit_margin;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    y -= V->D->P->num_workers * 2 * V->D->radius * zoom_ratio;
    double max_h = dm_histogram_get_max_height(D->H);
    double dy = (h - DVG->opts.paraprof_zoom_to_fit_margin_bottom - DVG->opts.paraprof_zoom_to_fit_margin
                 - zoom_ratio * (D->P->num_workers * 2 * D->radius + max_h)) / 2.0;
    if (dy > 0)
      y -= dy;
    break;
  }
  default:
    dv_check(0);
  }
  *zrx = *zry  = zoom_ratio;
  *myx = x;
  *myy = y;
}

void
dv_view_do_zoomfit_hor(dv_view_t * V) {
  if (V->S->adjust_auto_zoomfit)
    dv_view_change_azf(V, 1);
  dv_view_get_zoomfit_hor(V, &V->S->zoom_ratio_x, &V->S->zoom_ratio_y, &V->S->x, &V->S->y);
  dv_queue_draw(V);
}

void
dv_view_get_zoomfit_ver(dv_view_t * V, double * zrx, double * zry, double * myx, double * myy) {
  dm_dag_t * D = V->D;
  dv_view_status_t * S = V->S;
  double w = S->vpw;
  double h = S->vph;
  double zoom_ratio = 1.0;
  double x = 0.0;
  double y = 0.0;
  double d1, d2;
  dm_node_coordinate_t * rtco = &D->rt->c[S->coord];
  switch (S->lt) {
  case DV_LAYOUT_TYPE_DAG:
    d1 = rtco->dw;
    d2 = h - DVG->opts.zoom_to_fit_margin - DVG->opts.zoom_to_fit_margin_bottom;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    x -= (rtco->rw - rtco->lw) * 0.5 * zoom_ratio;
    break;
  case DV_LAYOUT_TYPE_DAG_BOX_LOG:
  case DV_LAYOUT_TYPE_DAG_BOX_POWER:
  case DV_LAYOUT_TYPE_DAG_BOX_LINEAR:
    d1 = rtco->dw;
    d2 = h - DVG->opts.zoom_to_fit_margin - DVG->opts.zoom_to_fit_margin_bottom;
    if (d1 > d2)
      zoom_ratio = d2 / d1;    
    x -= (rtco->rw - rtco->lw) * 0.5 * zoom_ratio;
    break;
  case DV_LAYOUT_TYPE_TIMELINE_VER:
    // Vertical Timeline
    d1 = 10 + rtco->dw;
    d2 = h - DVG->opts.zoom_to_fit_margin - DVG->opts.zoom_to_fit_margin_bottom;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    double lrw = 2 * D->radius + (D->P->num_workers - 1) * DVG->opts.hnd;
    x += (w - lrw * zoom_ratio) * 0.5;
    break;
  case DV_LAYOUT_TYPE_TIMELINE:
    // Horizontal Timeline
    d1 = D->P->num_workers * (D->radius * 2);
    d2 = h - DVG->opts.zoom_to_fit_margin - DVG->opts.zoom_to_fit_margin_bottom;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    break;
  case DV_LAYOUT_TYPE_PARAPROF: {
    // Parallelism profile
    double max_h = dm_histogram_get_max_height(D->H);
    d1 = D->P->num_workers * (2 * D->radius) + max_h;
    d2 = h - DVG->opts.paraprof_zoom_to_fit_margin - DVG->opts.paraprof_zoom_to_fit_margin_bottom;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    y -= D->P->num_workers * (2 * D->radius) * zoom_ratio;
    double dx = (w - 2 * DVG->opts.paraprof_zoom_to_fit_margin - zoom_ratio * dv_dag_scale_down_linear(V->D, V->D->et - V->D->bt)) / 2.0;
    if (dx > 0)
      x += dx;
    break;
  }
  case DV_LAYOUT_TYPE_CRITICAL_PATH: {
    // Critical path
    double max_h = dm_histogram_get_max_height(D->H);
    d1 = D->P->num_workers * (2 * D->radius) + max_h;
    d2 = h - DVG->opts.paraprof_zoom_to_fit_margin - DVG->opts.paraprof_zoom_to_fit_margin_bottom;
    if (d1 > d2)
      zoom_ratio = d2 / d1;
    y -= D->P->num_workers * (2 * D->radius) * zoom_ratio;
    double dx = (w - 2 * DVG->opts.paraprof_zoom_to_fit_margin - zoom_ratio * dv_dag_scale_down_linear(V->D, V->D->et - V->D->bt)) / 2.0;
    if (dx > 0)
      x += dx;
    break;
  }
  default:
    dv_check(0);
  }
  *zrx = *zry = zoom_ratio;
  *myx = x;
  *myy = y;
  S->zoom_ratio_x = S->zoom_ratio_y = zoom_ratio;
  S->x = x;
  S->y = y;
}

void
dv_view_do_zoomfit_ver(dv_view_t * V) {
  if (V->S->adjust_auto_zoomfit)
    dv_view_change_azf(V, 2);
  dv_view_get_zoomfit_ver(V, &V->S->zoom_ratio_x, &V->S->zoom_ratio_y, &V->S->x, &V->S->y);
  dv_queue_draw(V);
}

void
dv_view_do_zoomfit_based_on_lt(dv_view_t * V) {
  switch (V->S->lt) {
  case DV_LAYOUT_TYPE_DAG:
  case DV_LAYOUT_TYPE_DAG_BOX_LOG:
  case DV_LAYOUT_TYPE_DAG_BOX_POWER:
  case DV_LAYOUT_TYPE_DAG_BOX_LINEAR:
  case DV_LAYOUT_TYPE_TIMELINE_VER:
  case DV_LAYOUT_TYPE_TIMELINE:
  case DV_LAYOUT_TYPE_PARAPROF:
  case DV_LAYOUT_TYPE_CRITICAL_PATH:
    dv_view_do_zoomfit_full(V);
    break;
  default:
    dv_view_do_zoomfit_full(V);
    break;
  }
}

void
dv_view_do_zoomfit_full(dv_view_t * V) {
  if (V->S->adjust_auto_zoomfit)
    dv_view_change_azf(V, 4);
  double h_zrx, h_zry, h_x, h_y;
  dv_view_get_zoomfit_hor(V, &h_zrx, &h_zry, &h_x, &h_y);
  double v_zrx, v_zry, v_x, v_y;
  dv_view_get_zoomfit_ver(V, &v_zrx, &v_zry, &v_x, &v_y);
  if (v_zrx < h_zrx) {
    h_zrx = v_zrx;
    h_zry = v_zry;
    h_x = v_x;
    h_y = v_y;
  }
  V->S->zoom_ratio_x = h_zrx;
  V->S->zoom_ratio_y = h_zry;
  V->S->x = h_x;
  V->S->y = h_y;
  dv_queue_draw(V);
}

double
dv_view_get_radix(dv_view_t * V) {
  double radix = 0.0;
  switch (V->S->lt) {
  case DV_LAYOUT_TYPE_DAG_BOX_LOG:
    radix = V->D->log_radix;
    break;
  case DV_LAYOUT_TYPE_DAG_BOX_POWER:
    radix = V->D->power_radix;
    break;
  case DV_LAYOUT_TYPE_DAG_BOX_LINEAR:
  case DV_LAYOUT_TYPE_TIMELINE:
  case DV_LAYOUT_TYPE_TIMELINE_VER:
  case DV_LAYOUT_TYPE_PARAPROF:
    radix = V->D->linear_radix;
    break;
  }
  return radix;
}

static void
dv_view_set_radix(dv_view_t * V, double radix) {
  switch (V->S->lt) {
  case DV_LAYOUT_TYPE_DAG_BOX_LOG:
    V->D->log_radix = radix;
    break;
  case DV_LAYOUT_TYPE_DAG_BOX_POWER:
    V->D->power_radix = radix;
    break;
  case DV_LAYOUT_TYPE_DAG_BOX_LINEAR:
  case DV_LAYOUT_TYPE_TIMELINE:
  case DV_LAYOUT_TYPE_TIMELINE_VER:
  case DV_LAYOUT_TYPE_PARAPROF:
    V->D->linear_radix = radix;
    break;
  }
}

void
dv_view_change_radix(dv_view_t * V, double radix) {
  dv_view_set_radix(V, radix);
  dv_view_set_entry_radix_text(V);
}

void
dv_view_set_entry_radix_text(dv_view_t * V) {
  char str[DV_ENTRY_RADIX_MAX_LENGTH];
  double radix = dv_view_get_radix(V);
  sprintf(str, "%lf", radix);
  if (GTK_IS_WIDGET(V->T->entry_radix)) {
    gtk_entry_set_width_chars(GTK_ENTRY(V->T->entry_radix), strlen(str));
    gtk_entry_set_text(GTK_ENTRY(V->T->entry_radix), str);
  }
}

void
dv_view_set_entry_paraprof_resolution(dv_view_t * V) {
  if (V->D->H && V->T->entry_paraprof_min_interval) {
    char str[DV_STRING_LENGTH];
    sprintf(str, "%.0lf", V->D->H->min_entry_interval);
    gtk_entry_set_text(GTK_ENTRY(V->T->entry_paraprof_min_interval), str);
  }
}

void
dv_view_set_entry_remark_text(dv_view_t * V, char * str) {
  if (GTK_IS_WIDGET(V->T->entry_remark)) {
    gtk_entry_set_width_chars(GTK_ENTRY(V->T->entry_remark), strlen(str));
    gtk_entry_set_text(GTK_ENTRY(V->T->entry_remark), str);
  }
}

/*
void
dv_view_change_sdt(dv_view_t * V, int new_sdt) {
  if (new_sdt != V->D->sdt) {
    V->D->sdt = new_sdt;
    if (V->T->combobox_sdt)
      gtk_combo_box_set_active(GTK_COMBO_BOX(V->T->combobox_sdt), new_sdt);
    dv_view_set_entry_radix_text(V);
  }
}
*/

void
dv_view_change_eaffix(dv_view_t * V, int active) {
  if (active)
    V->S->edge_affix = DV_EDGE_AFFIX_LENGTH;
  else
    V->S->edge_affix = 0;
  if (GTK_IS_WIDGET(V->T->togg_eaffix)) {
    if (active)
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(V->T->togg_eaffix), TRUE);
    else
      gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(V->T->togg_eaffix), FALSE);
  }
}

void
dv_view_change_nc(dv_view_t * V, int new_nc) {
  int old_nc = V->S->nc;
  if (new_nc != old_nc) {
    V->S->nc = new_nc;
    if (GTK_IS_WIDGET(V->T->combobox_nc)) {
      gtk_combo_box_set_active(GTK_COMBO_BOX(V->T->combobox_nc), new_nc);
    }
  }
}

void
dv_view_change_lt(dv_view_t * V, int new_lt) {
  int old_lt = V->S->lt;
  
  if (new_lt != old_lt) {
    
    /* switching out from paraprof */
    if (old_lt == DV_LAYOUT_TYPE_PARAPROF) {
      int count = 0;
      int i;
      for (i = 0; i < DVG->nV; i++) {
        if (((dv_dag_t*)V->D->g)->mV[i] && DVG->V[i].S->lt == DV_LAYOUT_TYPE_PARAPROF && DVG->V[i].S->nviewports) {
          count++;
        }
      }
      if (!count) {
        dm_histogram_clean(V->D->H);
      }
    }
    
    /* edge affix, sdt */
    switch (new_lt) {
    case DV_LAYOUT_TYPE_DAG:
      dv_view_change_nc(V, 0);
      dv_view_change_eaffix(V, 0);
      break;
    case DV_LAYOUT_TYPE_DAG_BOX_LOG:
    case DV_LAYOUT_TYPE_DAG_BOX_POWER:
    case DV_LAYOUT_TYPE_DAG_BOX_LINEAR:
      dv_view_change_nc(V, 0);
      dv_view_change_eaffix(V, 0);
      //dv_view_change_sdt(V, 0);
      break;
    case DV_LAYOUT_TYPE_TIMELINE_VER:
    case DV_LAYOUT_TYPE_TIMELINE:
    case DV_LAYOUT_TYPE_PARAPROF:
      dv_view_change_nc(V, 2); // node kind
      //dv_view_change_sdt(V, 2);
      break;
    case DV_LAYOUT_TYPE_CRITICAL_PATH:
      dv_view_change_nc(V, 0);
      //dv_view_change_sdt(V, 2);
      break;
    default:
      break;
    }
    
    /* switching to paraprof */
    if (new_lt == DV_LAYOUT_TYPE_PARAPROF || new_lt == DV_LAYOUT_TYPE_CRITICAL_PATH) {
      if (!V->D->H) {
        V->D->H = dv_malloc( sizeof(dm_histogram_t) );
        dm_histogram_init(V->D->H);
        V->D->H->D = V->D;
        dm_histogram_reset(V->D->H);
        dv_view_set_entry_paraprof_resolution(V);
      } else if (!V->D->H->head_e) {
        dm_histogram_reset(V->D->H);
      }      
    }
    
    /* Change lt */
    dv_dag_t * g = (dv_dag_t *) V->D->g;
    g->nviews[old_lt]--;
    V->S->lt = new_lt;
    g->nviews[new_lt]++;
    dv_view_status_set_coord(V->S);
    /* Update T */
    if (GTK_IS_WIDGET(V->T->combobox_lt)) {
      gtk_combo_box_set_active(GTK_COMBO_BOX(V->T->combobox_lt), new_lt);
      /*
      switch (new_lt) {
      case DV_LAYOUT_TYPE_DAG:
        gtk_combo_box_set_active(GTK_COMBO_BOX(V->T->combobox_lt), 0);
        break;
      case DV_LAYOUT_TYPE_DAG_BOX_LOG:
        gtk_combo_box_set_active(GTK_COMBO_BOX(V->T->combobox_lt), 1);
        break;
      case DV_LAYOUT_TYPE_DAG_BOX_POWER:
        gtk_combo_box_set_active(GTK_COMBO_BOX(V->T->combobox_lt), 2);
        break;
      case DV_LAYOUT_TYPE_DAG_BOX_LINEAR:
        gtk_combo_box_set_active(GTK_COMBO_BOX(V->T->combobox_lt), 3);
        break;
      case DV_LAYOUT_TYPE_TIMELINE_VER:
        gtk_combo_box_set_active(GTK_COMBO_BOX(V->T->combobox_lt), 4);
        break;
      case DV_LAYOUT_TYPE_TIMELINE:
        gtk_combo_box_set_active(GTK_COMBO_BOX(V->T->combobox_lt), 5);
        break;
      case DV_LAYOUT_TYPE_PARAPROF:
        gtk_combo_box_set_active(GTK_COMBO_BOX(V->T->combobox_lt), 6);
        break;
      case DV_LAYOUT_TYPE_CRITICAL_PATH:
        gtk_combo_box_set_active(GTK_COMBO_BOX(V->T->combobox_lt), 7);
        break;
      default:
        dv_check(0);
      }
      */
    }
    dv_view_set_entry_radix_text(V);
    /* Layout */
    dv_view_layout(V);
    dv_view_auto_zoomfit(V);
    dv_queue_draw_d(V);
    
  }
}

void
dv_view_change_azf(dv_view_t * V, int new_azf) {
  V->S->auto_zoomfit = new_azf;
  if (V->T->combobox_azf)
    gtk_combo_box_set_active(GTK_COMBO_BOX(V->T->combobox_azf), new_azf);
}

void
dv_view_auto_zoomfit(dv_view_t * V) {
  switch (V->S->auto_zoomfit) {
  case 0:
    break;
  case 1:
    dv_view_do_zoomfit_hor(V);
    break;
  case 2:
    dv_view_do_zoomfit_ver(V);
    break;
  case 3:
    dv_view_do_zoomfit_based_on_lt(V);
    break;
  case 4:
    dv_view_do_zoomfit_full(V);
    break;
  default:
    break;
  }
}

void
dv_view_status_set_coord(dv_view_status_t * S) {
  switch (S->lt) {
  case DV_LAYOUT_TYPE_DAG:
    S->coord = 0;
    break;
  case DV_LAYOUT_TYPE_DAG_BOX_LOG:
    S->coord = 1;
    break;
  case DV_LAYOUT_TYPE_DAG_BOX_POWER:
    S->coord = 2;
    break;
  case DV_LAYOUT_TYPE_DAG_BOX_LINEAR:
    S->coord = 3;
    break;
  case DV_LAYOUT_TYPE_TIMELINE:
    S->coord = 4;
    break;
  case DV_LAYOUT_TYPE_TIMELINE_VER:
    S->coord = 5;
    break;
  case DV_LAYOUT_TYPE_PARAPROF:
    S->coord = 6;
    break;
  case DV_LAYOUT_TYPE_CRITICAL_PATH:
    S->coord = 7;
    break;
  default:
    dv_check(0);
  }  
}

void
dv_view_status_init(dv_view_t * V, dv_view_status_t * S) {
  S->drag_on = 0;
  S->pressx = S->pressy = 0.0;
  S->accdisx = S->accdisy = 0.0;
  S->nc = DV_NODE_COLOR_INIT;
  S->vpw = S->vph = 0.0;
  dv_animation_init(V, S->a);
  S->nd = 0;
  S->lt = DV_LAYOUT_TYPE_INIT;
  dv_view_status_set_coord(S);
  S->et = DV_EDGE_TYPE_INIT;
  S->edge_affix = DV_EDGE_AFFIX_LENGTH;
  S->cm = DV_CLICK_MODE_INIT;
  S->ndh = 0;
  S->focused = 0;
  S->nl = 0;
  S->ntr = 0;
  
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
  S->auto_zoomfit = DV_VIEW_AUTO_ZOOMFIT_INIT;
  S->adjust_auto_zoomfit = DV_VIEW_ADJUST_AUTO_ZOOMFIT_INIT;
  
  dv_motion_init(S->m, V);
  S->last_hovered_node = NULL;
  S->hm = DV_HOVER_MODE_INIT;
  S->show_legend = DV_SHOW_LEGEND_INIT;
  S->show_status = DV_SHOW_STATUS_INIT;
  S->remain_inner = DV_REMAIN_INNER_INIT;
  S->color_remarked_only = DV_COLOR_REMARKED_ONLY;

  S->nviewports = 0;
}

/****************** end of VIEW **************************************/


/*-----------------Remarked Workers-----------*/

static void
dv_view_scan_r(dv_view_t * V, dm_dag_node_t * node) {
  dm_dag_t * D = V->D;
  if (!dm_is_set(node))
    dm_dag_node_set(D, node);
  
  if (dm_is_union(node)) {

    /* Build inner */
    int is_inner_loaded = dm_is_inner_loaded(node);
    if (!is_inner_loaded) {
      if (dm_dag_build_node_inner(D, node) != DV_OK) {
        fprintf(stderr, "error in dm_dag_build_node_inner\n");
        return;
      }
    }
    /* Call inward */
    dv_check(node->head);
    dv_view_scan_r(V, node->head);
    /* Process single */
    node->r = node->head->link_r;
    /* Collapse inner */
    if (!is_inner_loaded && !V->S->remain_inner) {
      dm_dag_collapse_node_inner(D, node);
    }
    
  } else {
    node->r = 0;
    int v = dm_dag_node_lookup_value(D, node, V->S->nc);
    int i;
    for (i=0; i<((dv_dag_t*)V->D->g)->nr; i++)
      if (((dv_dag_t*)V->D->g)->ar[i] == v) break;
    if (i < ((dv_dag_t*)V->D->g)->nr)
      dv_set_bit(&node->r, i);
  }
  
  /* Call link-along */
  node->link_r = node->r;
  dm_dag_node_t * x = NULL;
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
    dv_view_scan_r(V, x);
    node->link_r |= x->link_r;
  }
}

void
dv_view_scan(dv_view_t * V) {
  dv_view_scan_r(V, V->D->rt);
}

void
dv_view_remark_similar_nodes(dv_view_t * V, dm_dag_node_t * node) {
  dm_dag_t * D = V->D;
  dv_dag_t * g = (dv_dag_t *) D->g;
  int val = dm_dag_node_lookup_value(D, node, V->S->nc);
  g->nr = 1;
  g->ar[0] = val;
  V->S->remain_inner = 1;
  V->S->color_remarked_only = 1;
  dv_view_scan(V);
  dv_queue_draw_dag(D);
}

/*-----------------end of Remarked Workers-----------*/



/****************** Statusbars **************************************/

void
dv_statusbar_update(int statusbar_id, int context_id, char * s) {
  GtkWidget * statusbar = NULL;
  switch (statusbar_id) {
  case 1:
    statusbar = GUI->statusbar1;
    break;
  case 2:
    statusbar = GUI->statusbar2;
    break;
  case 3:
    statusbar = GUI->statusbar3;
    break;
  default:
    break;
  }
  if (statusbar) {
    gtk_statusbar_push(GTK_STATUSBAR(statusbar), context_id, s);
  }
}

void
dv_statusbar_remove(int statusbar_id, int context_id) {
  GtkWidget * statusbar = NULL;
  switch (statusbar_id) {
  case 1:
    statusbar = GUI->statusbar1;
    break;
  case 2:
    statusbar = GUI->statusbar2;
    break;
  case 3:
    statusbar = GUI->statusbar3;
    break;
  default:
    break;
  }
  if (statusbar) {
    gtk_statusbar_pop(GTK_STATUSBAR(statusbar), context_id);
  }
}

void
dv_statusbar_update_selection_status() {
  if (!DVG->activeV) {
    dv_statusbar_remove(2, 0);
    return;
  }
  char s[DV_STRING_LENGTH];
  dv_view_t * V = DVG->activeV;
  sprintf(s, "DAG %ld: nodes=%ld/%ld/%ld (depth:%d/%d) - View %ld: size=(%.0lf,%.0lf), zoom=(%.3lf,%.3lf), radix=%.3lf, radius=%.0lf",
          V->D - DMG->D,
          V->S->nd, V->S->nl, V->D->P->n,
          V->D->cur_d, V->D->dmax,
          V - DVG->V,
          V->S->vpw, V->S->vph,
          V->S->zoom_ratio_x, V->S->zoom_ratio_y,
          (V->S->lt==DV_LAYOUT_TYPE_DAG_BOX_LOG)?V->D->log_radix:((V->S->lt==DV_LAYOUT_TYPE_DAG_BOX_POWER)?V->D->power_radix:((V->S->lt==DV_LAYOUT_TYPE_DAG_BOX_LINEAR)?V->D->linear_radix:V->S->lt)),
          V->D->radius);
  dv_statusbar_update(2, 0, s);
  //sprintf(s, "Currently focused DAG (change by tab key): nodes/hidden nodes/all nodes (depths) - viewport dimension; Cairo zoom ratios; scale-down radix; node radius");
  if (GUI->statusbar2)
    gtk_widget_set_tooltip_text(GTK_WIDGET(GUI->statusbar2), s);
}

void
dv_statusbar_update_pool_status() {
  char s[DV_STRING_LENGTH];
  sprintf(s, "Pools: nodes:%ld/%ld(%ldMB)",
          DMG->pool->N - DMG->pool->n,
          DMG->pool->N,
          DMG->pool->sz / (1 << 20));
  sprintf(s, "%s, entries:%ld/%ld(%ldMB)",
          s,
          DMG->epool->N - DMG->epool->n,
          DMG->epool->N,
          DMG->epool->sz / (1 << 20));
  dv_statusbar_update(3, 0, s);
}

void
dv_statusbar_update_pointer_status() {
  double x = 0.0;
  double y = 0.0;
  double zx = 0.0;
  double zy = 0.0;
  if (DVG->activeV) {
    x = dv_view_convert_viewport_x_to_graph_x(DVG->activeV, DVG->activeV->mainVP->x);
    y = dv_view_convert_viewport_y_to_graph_y(DVG->activeV, DVG->activeV->mainVP->y);
    zx = DVG->activeV->S->zoom_ratio_x;
    zy = DVG->activeV->S->zoom_ratio_y;
  }
  
  char s[30];
  
  sprintf(s, "X: %8.2lf", x);
  gtk_label_set_text(GTK_LABEL(GUI->status_x), s);
  sprintf(s, "(z: %5.2lf%%)", zx * 100);
  gtk_label_set_text(GTK_LABEL(GUI->status_zx), s);
  
  sprintf(s, "Y: %8.2lf", y);
  gtk_label_set_text(GTK_LABEL(GUI->status_y), s);
  sprintf(s, "(z: %5.2lf%%)", zy * 100);
  gtk_label_set_text(GTK_LABEL(GUI->status_zy), s);
}

/****************** end of Statusbars **************************************/


/******************** Export ******************************/

static void
dv_viewport_export_to_surface(dv_viewport_t * VP, cairo_surface_t * surface) {
  cairo_t * cr = cairo_create(surface);
  // Whiten background
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");
  cairo_set_source_rgba(cr, white->red, white->green, white->blue, white->alpha);
  cairo_paint(cr);
  /* move to avoid ruler areas */
  cairo_translate(cr, -DVG->opts.ruler_width, -DVG->opts.ruler_width);
  // Draw viewport
  dv_viewport_draw(VP, cr, 0);
  // Finish
  cairo_destroy(cr);
}

void
dv_export_viewport() {
  dv_view_t * V = DVG->activeV;
  if (!V) {
    fprintf(stderr, "Warning: there is no active V to export.\n");
    return;
  }
  dv_viewport_t * VP = V->mainVP;
  if (!VP) {
    fprintf(stderr, "Warning: there is no main VP for the active V.\n");
    return;
  }
  cairo_surface_t * surface;
  /* resize to remove ruler areas */
  double surface_width = VP->vpw - 2 * DVG->opts.ruler_width;
  double surface_height = VP->vph - 2 * DVG->opts.ruler_width;

  /* PNG */
  surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, surface_width, surface_height);
  dv_viewport_export_to_surface(VP, surface);
  cairo_surface_write_to_png(surface, "00dv.png");
  fprintf(stdout, "Exported viewport %ld to 00dv.png\n", VP - DVG->VP);
  cairo_surface_destroy(surface);

  /* EPS */
  /*
  surface = cairo_ps_surface_create("00dv.eps", surface_width, surface_height);
  cairo_ps_surface_set_eps(surface, TRUE);
  dv_viewport_export_to_surface(VP, surface);
  fprintf(stdout, "Exported viewport %ld to 00dv.eps\n", VP - DVG->VP);
  cairo_surface_destroy(surface);
  */

  /* SVG */
  surface = cairo_svg_surface_create("00dv.svg", surface_width, surface_height);
  dv_viewport_export_to_surface(VP, surface);
  fprintf(stdout, "Exported viewport %ld to 00dv.svg\n", VP - DVG->VP);
  cairo_surface_destroy(surface);

  /* PDF */
  surface = cairo_pdf_surface_create("00dv.pdf", surface_width, surface_height);
  dv_viewport_export_to_surface(VP, surface);
  fprintf(stdout, "Exported viewport %ld to 00dv.pdf\n", VP - DVG->VP);
  cairo_surface_destroy(surface);

  return;  
}

static void
dv_export_viewports_get_size_r(dv_viewport_t * VP, double * w, double * h) {
  if (!VP) {
    *w = 0.0;
    *h = 0.0;
  } else if (!VP->split) {
    *w = VP->vpw;// - 2 * DVG->opts.ruler_width;
    *h = VP->vph;// - 2 * DVG->opts.ruler_width;
  } else {
    double w1, h1, w2, h2;
    dv_export_viewports_get_size_r(VP->vp1, &w1, &h1);
    dv_export_viewports_get_size_r(VP->vp2, &w2, &h2);
    if (VP->orientation == GTK_ORIENTATION_HORIZONTAL) {
      *w = w1 + w2;
      *h = dv_max(h1, h2);
    } else {
      *w = dv_max(w1, w2);
      *h = h1 + h2;
    }
  }
}

static void
dv_export_viewports_to_img_r(dv_viewport_t * VP, cairo_surface_t * surface, double x, double y) {
  if (!VP) {
    return;
  } else if (!VP->split) {
    /* resize to remove ruler areas */
    double surface_width = VP->vpw;// - 2 * DVG->opts.ruler_width;
    double surface_height = VP->vph;// - 2 * DVG->opts.ruler_width;
    cairo_surface_t * surface_child = cairo_surface_create_for_rectangle(surface, x, y, surface_width, surface_height);
    dv_viewport_export_to_surface(VP, surface_child);
  } else {
    double w1, h1;
    dv_export_viewports_get_size_r(VP->vp1, &w1, &h1);
    if (VP->orientation == GTK_ORIENTATION_HORIZONTAL) {
      dv_export_viewports_to_img_r(VP->vp1, surface, x, y);
      dv_export_viewports_to_img_r(VP->vp2, surface, x + w1, y);
    } else {
      dv_export_viewports_to_img_r(VP->vp1, surface, x, y);
      dv_export_viewports_to_img_r(VP->vp2, surface, x, y + h1);
    }
  }
}

_static_unused_ void
dv_export_viewports_to_eps_r(dv_viewport_t * VP, cairo_t * cr, double x, double y) {
  if (!VP) {
    return;
  } else if (!VP->split) {
    cairo_save(cr);
    cairo_translate(cr, x, y);
    dv_viewport_draw(VP, cr, 0);
    cairo_restore(cr);
  } else {
    double w1, h1;
    dv_export_viewports_get_size_r(VP->vp1, &w1, &h1);
    if (VP->orientation == GTK_ORIENTATION_HORIZONTAL) {
      dv_export_viewports_to_eps_r(VP->vp1, cr, x, y);
      dv_export_viewports_to_eps_r(VP->vp2, cr, x + w1, y);
    } else {
      dv_export_viewports_to_eps_r(VP->vp1, cr, x, y);
      dv_export_viewports_to_eps_r(VP->vp2, cr, x, y + h1);
    }
  }
}

static void
dv_export_viewports_to_svg_r(dv_viewport_t * VP, cairo_t * cr, double x, double y) {
  if (!VP) {
    return;
  } else if (!VP->split) {
    cairo_save(cr);
    cairo_translate(cr, x, y);
    dv_viewport_draw(VP, cr, 0);
    cairo_restore(cr);
  } else {
    double w1, h1;
    dv_export_viewports_get_size_r(VP->vp1, &w1, &h1);
    if (VP->orientation == GTK_ORIENTATION_HORIZONTAL) {
      dv_export_viewports_to_svg_r(VP->vp1, cr, x, y);
      dv_export_viewports_to_svg_r(VP->vp2, cr, x + w1, y);
    } else {
      dv_export_viewports_to_svg_r(VP->vp1, cr, x, y);
      dv_export_viewports_to_svg_r(VP->vp2, cr, x, y + h1);
    }
  }
}

static void
dv_export_viewports_to_pdf_r(dv_viewport_t * VP, cairo_t * cr, double x, double y) {
  if (!VP) {
    return;
  } else if (!VP->split) {
    cairo_save(cr);
    cairo_translate(cr, x, y);
    dv_viewport_draw(VP, cr, 0);
    cairo_restore(cr);
  } else {
    double w1, h1;
    dv_export_viewports_get_size_r(VP->vp1, &w1, &h1);
    if (VP->orientation == GTK_ORIENTATION_HORIZONTAL) {
      dv_export_viewports_to_pdf_r(VP->vp1, cr, x, y);
      dv_export_viewports_to_pdf_r(VP->vp2, cr, x + w1, y);
    } else {
      dv_export_viewports_to_pdf_r(VP->vp1, cr, x, y);
      dv_export_viewports_to_pdf_r(VP->vp2, cr, x, y + h1);
    }
  }
}

void
dv_export_all_viewports() {
  double w, h;
  dv_export_viewports_get_size_r(DVG->VP, &w, &h);
  cairo_surface_t * surface;
  cairo_t * cr;
  
  /* PNG */
  surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, w, h);
  dv_export_viewports_to_img_r(DVG->VP, surface, 0.0, 0.0);
  cairo_surface_write_to_png(surface, "00dv.png");
  fprintf(stdout, "Exported all viewports to 00dv.png\n");
  cairo_surface_destroy(surface);
  
  GdkRGBA white[1];
  gdk_rgba_parse(white, "white");

  /* EPS */
  /*
  surface = cairo_ps_surface_create("00dv.eps", w, h);
  cairo_ps_surface_set_eps(surface, TRUE);
  cr = cairo_create(surface);
  // Whiten background
  cairo_set_source_rgba(cr, white->red, white->green, white->blue, white->alpha);
  cairo_paint(cr);
  dv_export_viewports_to_eps_r(DVG->VP, cr, 0.0, 0.0);
  fprintf(stdout, "Exported all viewports to 00dv.eps\n");
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  */
  
  /* SVG */
  surface = cairo_svg_surface_create("00dv.svg", w, h);
  cr = cairo_create(surface);
  // Whiten background
  cairo_set_source_rgba(cr, white->red, white->green, white->blue, white->alpha);
  cairo_paint(cr);
  dv_export_viewports_to_svg_r(DVG->VP, cr, 0.0, 0.0);
  fprintf(stdout, "Exported all viewports to 00dv.svg\n");
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  
  /* PDF */
  surface = cairo_pdf_surface_create("00dv.pdf", w, h);
  cr = cairo_create(surface);
  // Whiten background
  cairo_set_source_rgba(cr, white->red, white->green, white->blue, white->alpha);
  cairo_paint(cr);
  dv_export_viewports_to_pdf_r(DVG->VP, cr, 0.0, 0.0);
  fprintf(stdout, "Exported all viewports to 00dv.pdf\n");
  cairo_destroy(cr);
  cairo_surface_destroy(surface);
  
  return;
}

/******************** end of Export ******************************/

/*
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
*/

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

void
dv_do_scrolling(dv_view_t * V, GdkEventScroll * event) {
  double factor = 1.0;
  
  if (V->S->do_scale_radix || V->S->do_scale_radius) {
    
    if (V->S->do_scale_radix) {
      
      // Cal factor    
      factor = 1.0;
      if (event->direction == GDK_SCROLL_UP)
        factor *= DVG->opts.scale_step_ratio;
      else if (event->direction == GDK_SCROLL_DOWN)
        factor /= DVG->opts.scale_step_ratio;
      // Apply factor    
      double radix = dv_view_get_radix(V);
      radix *= factor;
      dv_view_change_radix(V, radix);
      
    }
    
    if (V->S->do_scale_radius) {
      
      // Cal factor
      factor = 1.0;
      if (event->direction == GDK_SCROLL_UP)
        factor *= DVG->opts.scale_step_ratio;
      else if (event->direction == GDK_SCROLL_DOWN)
        factor /= DVG->opts.scale_step_ratio;
      // Apply factor    
      V->D->radius *= factor;
      
    }
    
    dv_view_layout(V);
    dv_view_auto_zoomfit(V);
    dv_queue_draw_dag(V->D);

  } else {
    
    /* Turn auto zoomfit off whenever there is zooming-scrolling */
    if (V->S->adjust_auto_zoomfit)
      dv_view_change_azf(V, 0);

    /* ratios considering Ctrl/Alt */
    double ratio = DVG->opts.zoom_step_ratio;
    _unused_ GdkModifierType mod = gtk_accelerator_get_default_mod_mask();
    if ((event->state & mod) == GDK_CONTROL_MASK) {
      ratio = (ratio - 1.0) * 10.0 + 1.0;
    } else if ((event->state & mod) == GDK_MOD1_MASK) {
      ratio = (ratio - 1.0) / 10.0 + 1.0;
    } else if ((event->state & mod) == GDK_SHIFT_MASK) {
      ratio = (ratio - 1.0) / 10.0 + 1.0;
    }
    // Cal factor    
    factor = 1.0;
    if (event->direction == GDK_SCROLL_UP)
      factor *= ratio;
    else if (event->direction == GDK_SCROLL_DOWN)
      factor /= ratio;
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





static dm_dag_node_t *
dv_do_finding_clicked_node(dv_view_t * V, double x, double y) {
  double time = dm_get_time();
  if (DVG->verbose_level >= 2) {
    fprintf(stderr, "dv_do_finding_clicked_node()\n");
  }
  dm_dag_node_t * ret = NULL;
  switch (V->S->lt) {
  case DV_LAYOUT_TYPE_DAG:
  case DV_LAYOUT_TYPE_DAG_BOX_LOG:
  case DV_LAYOUT_TYPE_DAG_BOX_POWER:
  case DV_LAYOUT_TYPE_DAG_BOX_LINEAR:
    ret = dv_view_dag_find_clicked_node(V, x, y);
    break;
  case DV_LAYOUT_TYPE_TIMELINE:
  case DV_LAYOUT_TYPE_TIMELINE_VER:
  case DV_LAYOUT_TYPE_PARAPROF:
    ret = dv_timeline_find_clicked_node(V, x, y);
    break;
  case DV_LAYOUT_TYPE_CRITICAL_PATH:
    ret = dv_critical_path_find_clicked_node(V, x, y);
    break;
  default:
    dv_check(0);
  }
  if (ret) {
    char s[DV_STRING_LENGTH];
    sprintf(s, "Node %ld", ret->pii);
    dv_statusbar_update(1, 0, s);
  } else {
    dv_statusbar_remove(1, 0);
  }
  if (DVG->verbose_level >= 2) {
    fprintf(stderr, "... done dv_do_finding_clicked_node(): %lf\n", dm_get_time() - time);
  }
  return ret;
}

static void
dv_do_expanding_one_1(dv_view_t * V, dm_dag_node_t * node) {
  if (!dm_is_inner_loaded(node))
    if (dm_dag_build_node_inner(V->D, node) != DV_OK) return;
  dv_view_status_t * S = V->S;
  switch (S->lt) {
  case DV_LAYOUT_TYPE_DAG:
  case DV_LAYOUT_TYPE_DAG_BOX_LOG:
  case DV_LAYOUT_TYPE_DAG_BOX_POWER:
  case DV_LAYOUT_TYPE_DAG_BOX_LINEAR:
    // add to animation
    if (dm_is_shrinking(node)) {
      dm_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKING);
      dm_node_flag_set(node->f, DV_NODE_FLAG_SHRINKED);
      dm_node_flag_set(node->f, DV_NODE_FLAG_EXPANDING);
      dv_animation_reverse(S->a, node);
    } else {
      dm_node_flag_set(node->f, DV_NODE_FLAG_EXPANDING);
      dv_animation_add(S->a, node);
    }
    break;
  case DV_LAYOUT_TYPE_TIMELINE_VER:
  case DV_LAYOUT_TYPE_TIMELINE:
    if (dm_is_shrinking(node)) {
      dm_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKING);
    } else if (dm_is_expanding(node)) {
      dm_node_flag_remove(node->f, DV_NODE_FLAG_EXPANDING);
    }
    dm_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKED);
  case DV_LAYOUT_TYPE_PARAPROF:
  case DV_LAYOUT_TYPE_CRITICAL_PATH:
    if (dm_is_shrinking(node)) {
      dm_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKING);
    } else if (dm_is_expanding(node)) {
      dm_node_flag_remove(node->f, DV_NODE_FLAG_EXPANDING);
    }
    dm_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKED);
    break;
  default:
    dv_check(0);
  }
  /* Histogram */
  if (V->D->H && V->D->H->head_e) {
    dm_histogram_remove_node(V->D->H, node, NULL);
    dm_dag_node_t * x = NULL;
    while ( (x = dm_dag_node_traverse_children(node, x)) ) {
      dm_histogram_add_node(V->D->H, x, NULL);
    }
  }
}

static void
dv_do_expanding_one_r(dv_view_t * V, dm_dag_node_t * node) {
  V->S->ntr++;
  if (!dm_is_set(node))
    dm_dag_node_set(V->D, node);
  if (dm_is_union(node)) {
    if ((!dm_is_inner_loaded(node)
         || dm_is_shrinked(node)
         || dm_is_shrinking(node))
        && !dm_is_expanding(node)) {
      // expand node
      dv_do_expanding_one_1(V, node);
    } else {
      /* Call inward */
      dv_check(node->head);
      dv_do_expanding_one_r(V, node->head);
    }
  }
  
  /* Call link-along */
  dm_dag_node_t * x = NULL;
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
    dv_do_expanding_one_r(V, x);
  }
}

void
dv_do_expanding_one(dv_view_t * V) {
  V->S->ntr = 0;
  //double start = dm_get_time();
  dv_do_expanding_one_r(V, V->D->rt);
  //double end = dm_get_time();
  //fprintf(stderr, "traverse time: %lf\n", end - start);
  if (!V->S->a->on) {
    dv_view_layout(V);
    dv_queue_draw_d(V);
  }
}

static void
dv_do_collapsing_one_1(dv_view_t * V, dm_dag_node_t * node) {
  dv_view_status_t * S = V->S;
  switch (S->lt) {
  case DV_LAYOUT_TYPE_DAG:
  case DV_LAYOUT_TYPE_DAG_BOX_LOG:
  case DV_LAYOUT_TYPE_DAG_BOX_POWER:
  case DV_LAYOUT_TYPE_DAG_BOX_LINEAR:
    // add to animation
    if (dm_is_expanding(node)) {
      dm_node_flag_remove(node->f, DV_NODE_FLAG_EXPANDING);
      dm_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKED);
      dm_node_flag_set(node->f, DV_NODE_FLAG_SHRINKING);
      dv_animation_reverse(S->a, node);
    } else {
      dm_node_flag_set(node->f, DV_NODE_FLAG_SHRINKING);
      dv_animation_add(S->a, node);
    }
    break;
  case DV_LAYOUT_TYPE_TIMELINE_VER:
  case DV_LAYOUT_TYPE_TIMELINE:
    if (dm_is_expanding(node)) {
      dm_node_flag_remove(node->f, DV_NODE_FLAG_EXPANDING);
    } else if (dm_is_shrinking(node)) {
      dm_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKING);
    }
    dm_node_flag_set(node->f, DV_NODE_FLAG_SHRINKED);
  case DV_LAYOUT_TYPE_PARAPROF:
  case DV_LAYOUT_TYPE_CRITICAL_PATH:
    if (dm_is_expanding(node)) {
      dm_node_flag_remove(node->f, DV_NODE_FLAG_EXPANDING);
    } else if (dm_is_shrinking(node)) {
      dm_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKING);
    }
    dm_node_flag_set(node->f, DV_NODE_FLAG_SHRINKED);
    break;
  default:
    dv_check(0);
  }
  /* Histogram */
  if (V->D->H && V->D->H->head_e) {
    dm_dag_node_t * x = NULL;
    while ( (x = dm_dag_node_traverse_children(node, x)) ) {
      dm_histogram_remove_node(V->D->H, x, NULL);
    }
    dm_histogram_add_node(V->D->H, node, NULL);
  }
}

_static_unused_ void
dv_do_collapsing_one_r(dv_view_t * V, dm_dag_node_t * node) {
  if (!dm_is_set(node))
    return;
  if (dm_is_union(node) && dm_is_inner_loaded(node)
      && !dm_is_shrinking(node)
      && (dm_is_expanded(node) || dm_is_expanding(node))) {
    // check if node has expanded node, excluding shrinking nodes
    int has_expanded_node = 0;
    /* Traverse all children */
    dm_dag_node_t * x = NULL;
    while ( (x = dm_dag_node_traverse_children(node, x)) ) {
      if (dm_is_union(x) && dm_is_inner_loaded(x)
          && (dm_is_expanded(x) || dm_is_expanding(x))
          && !dm_is_shrinking(x)) {
        has_expanded_node = 1;
        break;
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
  dm_dag_node_t * x = NULL;
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
    dv_do_collapsing_one_r(V, x);
  }
}

static void
dv_do_collapsing_one_depth_r(dv_view_t * V, dm_dag_node_t * node, int depth) {
  if (!dm_is_set(node))
    return;
  if (dm_is_union(node) && dm_is_inner_loaded(node)
      && !dm_is_shrinking(node)
      && (dm_is_expanded(node) || dm_is_expanding(node))) {
    // check if node has expanded node, excluding shrinking nodes
    int has_expanded_node = 0;
    /* Traverse all children */
    dm_dag_node_t * x = NULL;
    while ( (x = dm_dag_node_traverse_children(node, x)) ) {
      if (dm_is_union(x) && dm_is_inner_loaded(x)
          && (dm_is_expanded(x) || dm_is_expanding(x))
          && !dm_is_shrinking(x)) {
        has_expanded_node = 1;
        break;
      }
    }
    if (!has_expanded_node && node->d >= depth) {
      // collapsing node's parent
      dv_do_collapsing_one_1(V, node);
    } else {
      /* Call inward */
      dv_do_collapsing_one_depth_r(V, node->head, depth);
    }
  }
  
  /* Call link-along */
  dm_dag_node_t * x = NULL;
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
    dv_do_collapsing_one_depth_r(V, x, depth);
  }
}

void
dv_do_collapsing_one(dv_view_t * V) {
  //double start = dm_get_time();
  //dv_do_collapsing_one_r(V, V->D->rt);
  dv_do_collapsing_one_depth_r(V, V->D->rt, V->D->collapsing_d - 1);
  //double end = dm_get_time();
  //fprintf(stderr, "traverse time: %lf\n", end - start);
  if (!V->S->a->on) {
    dv_view_layout(V);
    dv_queue_draw_d(V);
  }
}

void
dv_do_button_event(dv_view_t * V, GdkEventButton * event) {
  double time = dm_get_time();
  if (DVG->verbose_level >= 2) {
    fprintf(stderr, "dv_do_button_event()\n");
  }
  /* Turn auto zoomfit off whenever there is button_event */
  if (V->S->adjust_auto_zoomfit)
    dv_view_change_azf(V, 0);
  
  dm_dag_t * D = V->D;
  dm_llist_t * itl = D->P->itl;
  dv_view_status_t * S = V->S;
  if (event->button == GDK_BUTTON_PRIMARY) { /* left mouse button */
    
    if (event->type == GDK_BUTTON_PRESS) {
      /* Drag */
      S->drag_on = 1;
      S->pressx = event->x;
      S->pressy = event->y;
      S->accdisx = 0.0;
      S->accdisy = 0.0;
    }  else if (event->type == GDK_BUTTON_RELEASE) {
      /* Drag */
      S->drag_on = 0;
      /* Click */
      if (S->accdisx < DV_SAFE_CLICK_RANGE
          && S->accdisy < DV_SAFE_CLICK_RANGE) {
        double ox = (event->x - S->basex - S->x) / S->zoom_ratio_x;
        double oy = (event->y - S->basey - S->y) / S->zoom_ratio_y;
        dm_dag_node_t * node = dv_do_finding_clicked_node(V, ox, oy);
        if (node) {
          switch (S->cm) {
          case 0:
            /* None */
            break;
          case 1:
            /* Info box */
            if (!dm_llist_remove(itl, (void *) node->pii)) {
              dm_llist_add(itl, (void *) node->pii);
            }
            dv_queue_draw_d_p(V);
            break;
          case 2:
            /* Expand/Collapse */
            if (dm_is_union(node)) {
              if ((!dm_is_inner_loaded(node) || dm_is_shrinked(node) || dm_is_shrinking(node))
                  && !dm_is_expanding(node)) {
                dv_do_expanding_one_1(V, node);
                dv_view_layout(V);
              }
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

  } else if (event->button == GDK_BUTTON_SECONDARY) { /* right mouse button */

    /* show context menu */
    if (event->type == GDK_BUTTON_PRESS) {

      double ox = (event->x - S->basex - S->x) / S->zoom_ratio_x;
      double oy = (event->y - S->basey - S->y) / S->zoom_ratio_y;
      dm_dag_node_t * node = dv_do_finding_clicked_node(V, ox, oy);
      if (node) {
        DVG->context_view = V;
        DVG->context_node = node;
        gtk_menu_popup(GTK_MENU(GUI->context_menu), NULL, NULL, NULL, NULL, event->button, event->time);
      }
      
    } else if (event->type == GDK_BUTTON_RELEASE) {
      // nothing
    }

  }
  if (DVG->verbose_level >= 2) {
    fprintf(stderr, "... done dv_do_button_event(): %.0lf\n", dm_get_time() - time);
  }
}

void
dv_do_motion_event(dv_view_t * V, GdkEventMotion * event) {
  dv_view_status_t * S = V->S;
  
  /* Dragging */
  if (S->drag_on) {
    double deltax = event->x - S->pressx;
    double deltay = event->y - S->pressy;
    S->x += deltax;
    S->y += deltay;
    S->accdisx += deltax;
    S->accdisy += deltay;
    S->pressx = event->x;
    S->pressy = event->y;
    dv_queue_draw(V);
  }
  /* Hovering */
  double ox = (event->x - S->basex - S->x) / S->zoom_ratio_x;
  double oy = (event->y - S->basey - S->y) / S->zoom_ratio_y;
  dm_dag_node_t * node = dv_do_finding_clicked_node(V, ox, oy);
  if (node
      && node != S->last_hovered_node
      && !dm_is_shrinking(node->parent)
      && !dm_is_expanding(node->parent)) {

    /* User-preferred hover mode */
    switch (S->hm) {
    case 0:
      break;
    case 1:
      /* Info */
      if (!dm_llist_remove(V->D->P->itl, (void *) node->pii)) {
        dm_llist_add(V->D->P->itl, (void *) node->pii);
      }
      dv_queue_draw_pidag(V->D->P);
      break;
    case 2:
      /* Expand */
      if (dm_is_union(node)) {
        if ((!dm_is_inner_loaded(node) || dm_is_shrinked(node) || dm_is_shrinking(node))
            && !dm_is_expanding(node)) {
          dv_do_expanding_one_1(V, node);
          dv_view_layout(V);
        }
      }
      break;
    case 3:
      /* Collapse */
      if (!dm_is_union(node)) {
        dv_do_collapsing_one_r(V, node->parent);
      }
      break;
    case 4:
      /* Expand/Collapse */
      if (dm_is_union(node)) {
        if ((!dm_is_inner_loaded(node) || dm_is_shrinked(node) || dm_is_shrinking(node))
            && !dm_is_expanding(node))
          dv_do_expanding_one_1(V, node);
      } else {
        dv_do_collapsing_one_r(V, node->parent);
      }
      break;
    case 5:
      /* Remark similar nodes */
      dv_view_remark_similar_nodes(V, node);
      break;
    case 6:
      /* Highlight */
      if (S->last_hovered_node)
        S->last_hovered_node->highlight = 0;
      node->highlight = 1;
      break;
    default:
      dv_check(0);
    }

    /* Adjust nodeinfo sidebox if shown */
    GtkWidget * item = GTK_WIDGET(gtk_builder_get_object(GUI->builder, "nodeinfo"));  
    gboolean shown = gtk_check_menu_item_get_active(GTK_CHECK_MENU_ITEM(item));
    if (shown)
      dv_gui_nodeinfo_set_node(GUI, node, V->D);

    /* Mark last hovered node */
    S->last_hovered_node = node;
    dv_queue_draw(V);
  }
  if (!node) {
    if (S->last_hovered_node)
      S->last_hovered_node->highlight = 0;
    S->last_hovered_node = NULL;
    
    S->color_remarked_only = 0;
    dv_queue_draw(V);
  }
}

dm_dag_node_t *
dv_find_node_with_pii_r(dv_view_t * V, long pii, dm_dag_node_t * node) {
  if (node->pii == pii)
    return node;
  dm_dag_node_t * ret = NULL;
  /* Call inward */
  if (dm_is_union(node) && dm_is_inner_loaded(node)
      && !dm_is_shrinking(node)
      && (dm_is_expanded(node) || dm_is_expanding(node))) {
    ret = dv_find_node_with_pii_r(V, pii, node->head);
    if (ret)
      return ret;
  }
  /* Call link-along */
  dm_dag_node_t * x = NULL;
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
    ret = dv_find_node_with_pii_r(V, pii, x);
    if (ret)
      return ret;
  }
  return NULL;
}

static void
dv_dag_build_inner_all_r(dm_dag_t * D, dm_dag_node_t * node) {
  if (!dm_is_set(node))
    dm_dag_node_set(D, node);
  if (dm_is_union(node)) {
    if (!dm_is_inner_loaded(node)) {
      dm_dag_build_node_inner(D, node);
    }
    /* Call inward */
    dv_check(node->head);
    dv_dag_build_inner_all_r(D, node->head);
  }
  
  /* Call link-along */
  dm_dag_node_t * x = NULL;
  while ( (x = dm_dag_node_traverse_nexts(node, x)) ) {
    dv_dag_build_inner_all_r(D, x);
  }
}

void
dv_dag_build_inner_all(dm_dag_t * D) {
  dv_dag_build_inner_all_r(D, D->rt);
}
