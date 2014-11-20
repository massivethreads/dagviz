#include "dagviz.h"

/*-----------Main layout functions-------------------------*/

static void
dv_view_layout_with_type(dv_view_t * V, int lt) {
  switch (lt) {
  case 0:
    dv_view_layout_glike(V);
    break;
  case 1:
    dv_view_layout_bbox(V);
    break;
  case 2:
    dv_view_layout_timeline(V);
    break;
  case 3:
    dv_view_layout_timeline2(V);
    break;
  case 4:
    if (V->D->H) {
      dv_histogram_reset(V->D->H);
      double start = dv_get_time();
      dv_view_layout_paraprof(V);
      double end = dv_get_time();
      fprintf(stderr, "layout time: %lf\n", end - start);
    } else {
      fprintf(stderr, "Warning: trying to lay out type 4 without H.\n");
    }
    break;
  default:
    dv_check(0);
  }  
}

void
dv_view_layout(dv_view_t * V) {
  /*
  dv_view_layout_glike(V);
  dv_view_layout_bbox(V);
  dv_view_layout_timeline(V);
  dv_view_layout_timeline2(V);
  if (V - CS->V == 0) {
    dv_histogram_init(CS->H);
    CS->H->V = V;
    dv_view_layout_paraprof(V, CS->H);
  } else {
    dv_view_layout_paraprof(V, NULL);
  }
  */

  dv_view_layout_with_type(V, V->S->lt);
  int lt;
  for (lt=0; lt<DV_NUM_LAYOUT_TYPES; lt++)
    if (lt != V->S->lt && V->D->tolayout[lt])
      dv_view_layout_with_type(V, lt);
}

/*-----------end of Main layout functions-------------------------*/

/*-----Common functions-----*/

double dv_view_calculate_gap(dv_view_t *V, dv_dag_node_t *node) {
  double gap = 1.0;
  if (!node) return gap;
  double ratio = (dv_get_time() - node->started) / V->S->a->duration;
  if (ratio > 1.0)
    ratio = 1.0;
  if (dv_is_shrinking(node)) {
    //gap = 1.0 - ratio;
    gap = (1.0 - ratio) * (1.0 - ratio);
  } else if (dv_is_expanding(node)) {
    //gap = ratio;
    gap = 1.0 - (1.0 - ratio) * (1.0 - ratio);
  }
  return gap;
}

double dv_view_calculate_reverse_ratio(dv_view_t *V, dv_dag_node_t *node) {
  double ret = 0.0;
  if (!node) return ret;
  double gap = dv_view_calculate_gap(V, node);
  if (dv_is_shrinking(node)) {
    ret = 1.0 - sqrt(1.0 - gap);
  } else if (dv_is_expanding(node)) {
    ret = 1.0 - sqrt(gap);
  }
  return ret;
}

/*-----end of Common functions-----*/

/*-----------Animation functions----------------------*/

double dv_get_time() {
  /* millisecond */
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1.0E3 + ((double)tv.tv_usec) / 1.0E3;
}

void dv_animation_init(dv_view_t *V, dv_animation_t *a) {
  a->on = 0;
  a->duration = DV_ANIMATION_DURATION;
  a->step = DV_ANIMATION_STEP;
  dv_llist_init(a->movings);
  a->V = V;
}

static gboolean dv_animation_tick(gpointer data) {
  dv_animation_t *a = (dv_animation_t *) data;
  dv_check(a->on);
  double cur = dv_get_time();
  dv_dag_node_t *node = NULL;
  // iterate moving nodes
  dv_llist_cell_t *c = a->movings->top;
  while (c) {
    node = (dv_dag_node_t *) c->item;
    c = c->next;
    if (cur - node->started >= a->duration) {
      dv_animation_remove(a, node);
      if (dv_is_shrinking(node)) {
        dv_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKING);
        dv_node_flag_set(node->f, DV_NODE_FLAG_SHRINKED);
      } else if (dv_is_expanding(node)) {
        dv_node_flag_remove(node->f, DV_NODE_FLAG_EXPANDING);
        dv_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKED);
      }
    }
  }
  /*
  while (node = (dv_dag_node_t *) dv_llist_iterate_next(a->movings, node)) {
    if (cur - node->started >= a->duration) {
      dv_llist_remove(a->movings, node);
      if (dv_is_shrinking(node)) {
        dv_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKING);
        dv_node_flag_set(node->f, DV_NODE_FLAG_SHRINKED);
      } else if (dv_is_expanding(node)) {
        dv_node_flag_remove(node->f, DV_NODE_FLAG_EXPANDING);
        dv_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKED);
      }
    }
  }
  */
  dv_view_layout(a->V);
  dv_queue_draw_d(a->V);
  // stop timer when there is no moving node 
  if (dv_llist_size(a->movings) == 0) {
    dv_animation_stop(a);
    return 0;
  } else {
    return 1;
  }
}

void dv_animation_start(dv_animation_t *a) {
  a->on = 1;
  g_timeout_add(a->step, dv_animation_tick, a);
}

void dv_animation_stop(dv_animation_t *a) {
  a->on = 0;
}

void dv_animation_add(dv_animation_t *a, dv_dag_node_t *node) {
  double cur = dv_get_time();
  node->started = cur;
  dv_llist_add(a->movings, node);
  if (!a->on)
    dv_animation_start(a);
}

void dv_animation_remove(dv_animation_t *a, dv_dag_node_t *node) {
  dv_llist_remove(a->movings, node);
}

void dv_animation_reverse(dv_animation_t *a, dv_dag_node_t *node) {
  dv_llist_remove(a->movings, node);
  double cur = dv_get_time();
  //node->started = 2 * cur - a->duration - node->started;
  node->started = cur - a->duration * dv_view_calculate_reverse_ratio(a->V, node);
  dv_llist_add(a->movings, node);
}

/*-----------end of Animation functions----------------------*/



/*-----------Motion functions----------------------*/

static void
dv_motion_reset(dv_motion_t * m) {
  m->target_pii = -1;
  m->xfrom = m->yfrom = 0.0;
  m->xto = m->yto = 0.0;
  m->zrxfrom = m->zrxto = 0.0;
  m->zryfrom = m->zryto = 0.0;
  m->start_t = 0.0;
}

void
dv_motion_init(dv_motion_t * m, dv_view_t * V) {
  m->on = 0;
  m->duration = DV_ANIMATION_DURATION;
  m->step = DV_ANIMATION_STEP;
  m->V = V;
  dv_motion_reset(m);
}

static gboolean
dv_motion_tick(gpointer data) {
  dv_motion_t * m = (dv_motion_t *) data;
  dv_check(m->on);
  double cur = dv_get_time();
  dv_check(cur > m->start_t);
  if (cur - m->start_t < m->duration) {
    double ratio = (cur - m->start_t) / m->duration;
    m->V->S->x = m->xfrom + ratio * (m->xto - m->xfrom);
    m->V->S->y = m->yfrom + ratio * (m->yto - m->yfrom);
    m->V->S->zoom_ratio_x = m->zrxfrom + ratio * (m->zrxto - m->zrxfrom);
    m->V->S->zoom_ratio_y = m->zryfrom + ratio * (m->zryto - m->zryfrom);
    dv_queue_draw(m->V);
    return 1;
  } else {
    dv_motion_stop(m);
    return 0;
  }
}

void
dv_motion_reset_target(dv_motion_t * m, long pii, double xto, double yto, double zrxto, double zryto) {
  m->target_pii = pii;
  m->xfrom = m->V->S->x;
  m->yfrom = m->V->S->y;
  m->zrxfrom = m->V->S->zoom_ratio_x;
  m->zryfrom = m->V->S->zoom_ratio_y;
  m->xto = xto;
  m->yto = yto;
  m->zrxto = zrxto;
  m->zryto = zryto;
  m->start_t = dv_get_time();
}

void
dv_motion_start(dv_motion_t * m, long pii, double xto, double yto, double zrxto, double zryto) {
  m->on = 1;
  dv_motion_reset_target(m, pii, xto, yto, zrxto, zryto);
  g_timeout_add(m->step, dv_motion_tick, m);
}

void
dv_motion_stop(dv_motion_t * m) {
  m->on = 0;
  dv_motion_reset(m);
}

/*-----------Motion functions----------------------*/
