#include "dagviz.h"

/*-----------Main layout functions-------------------------*/

void dv_view_layout(dv_view_t *V) {
  dv_view_status_t *S = V->S;

  dv_view_layout_glike(V);
  dv_view_layout_bbox(V);
  dv_view_layout_timeline(V);
  /*
  if (S->lt == 0)
    dv_view_layout_glike(V);
  else if (S->lt == 1)
    dv_view_layout_bbox(V);
  else if (S->lt == 2)
    dv_view_layout_timeline(V);
  */

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



