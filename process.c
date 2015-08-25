#include "dagviz.h"

/******************Statistical Process**************************************/

static char *
dv_filename_get_short_name(char * fn) {
  char * p_from = strrchr(fn, '/');
  char * p_to = strrchr(fn, '.');
  if (!p_from)
    p_from = fn;
  else
    p_from++;
  if (!p_to) p_to = fn + strlen(fn);
  int n = p_to - p_from;
  char * ret = (char *) dv_malloc( sizeof(char) * n + 1 );
  strncpy(ret, p_from, n);
  return ret;
}

static void
dv_dag_collect_delays_r(dv_dag_t * D, dv_dag_node_t * node, FILE * out, dv_stat_distribution_entry_t * e) {
  if (!node)
    dv_check(node);

  if (dv_is_union(node) && dv_is_inner_loaded(node)) {
    dv_dag_collect_delays_r(D, node->head, out, e);
  } else {
    dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
    if (pi->info.kind == dr_dag_node_kind_create_task) {
      dr_pi_dag_node * pi_;
      if (e->type == 0)
        pi_ = dv_pidag_get_node_by_dag_node(D->P, node->spawn);
      else
        pi_ = dv_pidag_get_node_by_dag_node(D->P, node->next);
      dv_check(pi_);
      if (!e->stolen || pi_->info.worker != pi->info.worker)
        fprintf(out, "%lld\n", pi_->info.start.t - pi->info.end.t);
    }
  }

  dv_dag_node_t * next = NULL;
  while ( (next = dv_dag_node_traverse_nexts(node, next)) ) {
    dv_dag_collect_delays_r(D, next, out, e);
  }
}

static void
dv_dag_collect_sync_delays_r(dv_dag_t * D, dv_dag_node_t * node, FILE * out, dv_stat_distribution_entry_t * e) {
  if (!node)
    dv_check(node);

  if (dv_is_union(node) && dv_is_inner_loaded(node)) {
    dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
    if (pi->info.kind == dr_dag_node_kind_section) {
      dr_pi_dag_node * pi_ = dv_pidag_get_node_by_dag_node(D->P, node->next);
      if (pi_) {
        dr_clock_t last_t = 0;
        dv_dag_node_t * x = NULL;
        while ( (x = dv_dag_node_traverse_tails(node, x)) ) {
          dr_pi_dag_node * x_pi = dv_pidag_get_node_by_dag_node(D->P, x);
          if (x_pi->info.end.t > last_t)
            last_t = x_pi->info.end.t;
        }
        dv_check(last_t);
        fprintf(out, "%lld\n", pi_->info.start.t - last_t);
      }
    }
    dv_dag_collect_sync_delays_r(D, node->head, out, e);
  }

  dv_dag_node_t * next = NULL;
  while ( (next = dv_dag_node_traverse_nexts(node, next)) ) {
    dv_dag_collect_sync_delays_r(D, next, out, e);
  }
}

static void
dv_dag_collect_intervals_r(dv_dag_t * D, dv_dag_node_t * node, FILE * out, dv_stat_distribution_entry_t * e) {
  if (!node)
    dv_check(node);

  if (dv_is_union(node) && dv_is_inner_loaded(node)) {
    dv_dag_collect_intervals_r(D, node->head, out, e);
  } else if (!dv_is_union(node) || !dv_is_inner_loaded(node)) {
    dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
    fprintf(out, "%lld\n", pi->info.end.t - pi->info.start.t);
  }

  dv_dag_node_t * next = NULL;
  while ( (next = dv_dag_node_traverse_nexts(node, next)) ) {
    dv_dag_collect_intervals_r(D, next, out, e);
  }
}

static void
dv_dag_expand_implicitly_r(dv_dag_t * D, dv_dag_node_t * node) {
  if (!dv_is_set(node))
    dv_dag_node_set(D, node);
  
  if (dv_is_union(node)) {

    /* Build inner */
    if ( !dv_is_inner_loaded(node) ) {
      if (dv_dag_build_node_inner(D, node) != DV_OK) {
        fprintf(stderr, "error in dv_dag_build_node_inner\n");
        return;
      }
    }
    /* Call inward */
    dv_check(node->head);
    dv_dag_expand_implicitly_r(D, node->head);
    
  }
  
  /* Call link-along */
  dv_dag_node_t * next = NULL;
  while ( (next = dv_dag_node_traverse_nexts(node, next)) ) {
    dv_dag_expand_implicitly_r(D, next);
  }
}

static void
dv_dag_expand_implicitly(dv_dag_t * D) {
  dv_dag_expand_implicitly_r(D, D->rt);
}

static void
dv_dag_set_status_label(dv_dag_t * D, GtkWidget * label) {
  char str[100];
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, D->rt);
  sprintf(str, " nodes: %ld/%ld (%ldMB), elasped: %llu clocks", D->n, D->P->n, D->P->sz / (1 << 20), pi->info.end.t - pi->info.start.t);
  gtk_label_set_text(GTK_LABEL(label), str);
}

/******************end of Statistical Process**************************************/

static void
dv_dag_node_pool_set_status_label(dv_dag_node_pool_t * pool, GtkWidget * label) {
  char str[100];
  sprintf(str, "Node Pool: %ld / %ld (%ldMB)", pool->N - pool->n, pool->N, pool->sz / (1 << 20) );
  gtk_label_set_text(GTK_LABEL(label), str);
}

static void
dv_histogram_entry_pool_set_status_label(dv_histogram_entry_pool_t * pool, GtkWidget * label) {
  char str[100];
  sprintf(str, "Entry Pool: %ld / %ld (%ldMB)", pool->N - pool->n, pool->N, pool->sz / (1 << 20) );
  gtk_label_set_text(GTK_LABEL(label), str);
}

static void
dv_do_scrolling(dv_view_t * V, GdkEventScroll * event) {
  if (V->S->adjust_auto_zoomfit)
    dv_view_change_azf(V, 0);
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

static void
dv_do_button_event(dv_view_t * V, GdkEventButton * event) {
  if (V->S->adjust_auto_zoomfit)
    dv_view_change_azf(V, 0);
  dv_dag_t * D = V->D;
  dv_llist_t * itl = D->P->itl;
  dv_view_status_t * S = V->S;
  if (event->button == GDK_BUTTON_PRIMARY) { /* left mouse button */
    
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
            /* None */
            break;
          case 1:
            /* Info box */
            if (!dv_llist_remove(itl, (void *) node->pii)) {
              dv_llist_add(itl, (void *) node->pii);
            }
            dv_queue_draw_d_p(V);
            break;
          case 2:
            /* Expand/Collapse */
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

  } else if (event->button == GDK_BUTTON_SECONDARY) { /* right mouse button */

    if (event->type == GDK_BUTTON_RELEASE) {
      double ox = (event->x - S->basex - S->x) / S->zoom_ratio_x;
      double oy = (event->y - S->basey - S->y) / S->zoom_ratio_y;
      dv_dag_node_t * node = dv_do_finding_clicked_node(V, ox, oy);
      if (node) {
        /* Info box */
        if (!dv_llist_remove(itl, (void *) node->pii)) {
          dv_llist_add(itl, (void *) node->pii);
        }
        dv_queue_draw_d_p(V);
      }
    }

  }
}

static void
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
    dv_queue_draw_d(V);
  }
  /* Hovering */
  double ox = (event->x - S->basex - S->x) / S->zoom_ratio_x;
  double oy = (event->y - S->basey - S->y) / S->zoom_ratio_y;
  dv_dag_node_t * node = dv_do_finding_clicked_node(V, ox, oy);
  if (node
      && node != S->last_hovered_node
      && !dv_is_shrinking(node->parent)
      && !dv_is_expanding(node->parent)) {
    switch (S->hm) {
    case 0:
      break;
    case 1:
      /* Info */
      if (!dv_llist_remove(V->D->P->itl, (void *) node->pii)) {
        dv_llist_add(V->D->P->itl, (void *) node->pii);
      }
      dv_queue_draw_d_p(V);
      break;
    case 2:
      /* Expand */
      if (dv_is_union(node)) {
        if ((!dv_is_inner_loaded(node) || dv_is_shrinked(node) || dv_is_shrinking(node))
            && !dv_is_expanding(node))
          dv_do_expanding_one_1(V, node);
      }
      break;
    case 3:
      /* Collapse */
      if (!dv_is_union(node)) {
        dv_do_collapsing_one_r(V, node->parent);
      }
      break;
    case 4:
      /* Expand/Collapse */
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
    S->last_hovered_node = node;
  }
  if (!node)
    S->last_hovered_node = NULL;
}

static dv_dag_node_t * dv_find_node_with_pii_r(dv_view_t *V, long pii, dv_dag_node_t *node) {
  if (node->pii == pii)
    return node;
  dv_dag_node_t * ret = NULL;
  /* Call inward */
  if (dv_is_union(node) && dv_is_inner_loaded(node)
      && !dv_is_shrinking(node)
      && (dv_is_expanded(node) || dv_is_expanding(node))) {
    ret = dv_find_node_with_pii_r(V, pii, node->head);
    if (ret)
      return ret;
  }
  /* Call link-along */
  dv_dag_node_t * x = NULL;
  while ( (x = dv_dag_node_traverse_nexts(node, x)) ) {
    ret = dv_find_node_with_pii_r(V, pii, x);
    if (ret)
      return ret;
  }
  return NULL;
}

void
dv_view_change_azf(dv_view_t * V, int new_azf) {
  V->S->auto_zoomfit = new_azf;
  gtk_combo_box_set_active(GTK_COMBO_BOX(V->T->combobox_azf), new_azf);
}

void
dv_view_status_set_coord(dv_view_status_t * S) {
  switch (S->lt) {
  case 0:
    S->coord = 0;
    break;
  case 1:
    S->coord = 1;
    break;
  case 2:
    S->coord = 2;
    break;
  case 3:
    S->coord = 3;
    break;
  case 4:
    S->coord = 3;
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
}



