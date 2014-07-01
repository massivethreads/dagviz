#include "dagviz.h"

/*-----Common functions-----*/

double dv_layout_calculate_gap(dv_dag_node_t *node) {
  double gap = 1.0;
  if (!node) return gap;
  double ratio = (dv_get_time() - node->started) / S->a->duration;
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

/*-----end of Common functions-----*/

/*-----------Animation functions----------------------*/

double dv_get_time() {
  /* millisecond */
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1.0E3 + ((double)tv.tv_usec) / 1.0E3;
}

void dv_animation_init(dv_animation_t *a) {
  a->on = 0;
  a->duration = DV_ANIMATION_DURATION;
  a->step = DV_ANIMATION_STEP;
  dv_llist_init(a->movings);
}

static gboolean dv_animation_tick(gpointer data) {
  dv_animation_t *a = (dv_animation_t *) data;
  dv_check(a->on);
  double cur = dv_get_time();
  dv_llist_iterate_init(a->movings);
  dv_dag_node_t *node;
  // iterate moving nodes
  while (node = (dv_dag_node_t *) dv_llist_iterate_next(a->movings)) {
    dv_check(cur >= node->started);
    if (cur - node->started >= a->duration) {
      // Stop this node from animation
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
  dv_layout_dvdag(G);
  gtk_widget_queue_draw(darea);
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
  node->started = 2 * cur - a->duration - node->started;
  dv_llist_add(a->movings, node);
}

/*-----------end of Animation functions----------------------*/


/*-----------Main layout functions-------------------------*/

void dv_layout_dvdag(dv_dag_t *G) {
  
  if (S->lt == 0)
    dv_layout_glike_dvdag(G);
  else if (S->lt == 1)
    dv_layout_bbox_dvdag(G);
  else if (S->lt == 2)
    dv_layout_timeline_dvdag(G);

}

/*-----------end of Main layout functions-------------------------*/

