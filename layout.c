#include "dagviz.h"


/*-----------------Layout functions-----------*/

static double dv_layout_calculate_gap(dv_dag_node_t *node) {
  double ratio = S->a->ratio;
  double gap = 1.0;
  if (!node)
    return gap;
  if (dv_is_shrinking(node)) {
    //gap = 1.0 - ratio;
    gap = (1.0 - ratio) * (1.0 - ratio);
  } else if (dv_is_expanding(node)) {
    //gap = ratio;
    gap = 1.0 - (1.0 - ratio) * (1.0 - ratio);
  }
  return gap;
}

static double dv_layout_calculate_hgap(dv_dag_node_t *node) {
  double gap = dv_layout_calculate_gap(node);
  double hgap = gap * DV_HDIS;
  return hgap;
}

static double dv_layout_calculate_vgap(dv_dag_node_t *node, double time_gap) {
  double gap = dv_layout_calculate_gap(node);
  double vgap = gap * time_gap * DV_TIME_FACTOR;
  return vgap;
}

double dv_layout_calculate_hsize(dv_dag_node_t *node) {
  return DV_RADIUS;
}

double dv_layout_calculate_vsize(dv_dag_node_t *node) {
  double gap = dv_layout_calculate_gap(node);
  double time = node->pi->info.end.t - node->pi->info.start.t;
  double vsize = gap * time * DV_TIME_FACTOR;
  printf("vsize: %0.1lf\n", vsize);
  return vsize;
}

static dv_dag_node_t * dv_layout_node_get_last_tail(dv_dag_node_t *node) {
  dv_dag_node_t * ret = 0;
  dv_dag_node_t * tail;
  dv_llist_iterate_init(node->tails);
  while (tail = (dv_dag_node_t *) dv_llist_iterate_next(node->tails)) {
    if (!ret || ret->pi->info.end.t < tail->pi->info.end.t)
      ret = tail;
  }
  return ret;
}

static double dv_layout_node_get_displacement(dv_dag_node_t *node) {
  dv_dag_node_t * u = node;
  double dis = 0.0;
  while (u) {
    dis += u->x;
    u = u->pre;
  }
  return dis;
}

static void dv_layout_node(dv_dag_node_t *node) {
  /* Calculate inward */
  int is_single_node = 1;
  switch (node->pi->info.kind) {
  case dr_dag_node_kind_wait_tasks:
  case dr_dag_node_kind_end_task:
  case dr_dag_node_kind_create_task:
    break;
  case dr_dag_node_kind_section:
  case dr_dag_node_kind_task:
    if (dv_is_union(node)
        && ( !dv_is_shrinked(node) || dv_is_expanding(node) )) {
      dv_check(node->head);
      is_single_node = 0;
    }
    break;
  default:
    dv_check(0);
    break;
  }
  if (is_single_node) {
    // node's inward
    node->lw = dv_layout_calculate_hsize(node);
    node->rw = dv_layout_calculate_hsize(node);
    node->dw = dv_layout_calculate_vsize(node);
  } else {
    // node's head's outward
    node->head->xp = 0.0;
    node->head->y = 0.0;
    // Recursive call
    dv_layout_node(node->head);
    // node's inward
    node->lw = node->head->link_lw;
    node->rw = node->head->link_rw;
    node->dw = node->head->link_dw;
  }
    
  /* Calculate link-along */
  int n_links = dv_llist_size(node->links);
  dv_dag_node_t * u; // linked node 1
  dv_dag_node_t * v; // linked node 2
  double time_gap, gap, ugap, vgap;
  switch (n_links) {
  case 0:
    // node's link-along
    node->link_lw = node->lw;
    node->link_rw = node->rw;
    node->link_dw = node->dw;
    break;
  case 1:
    u = (dv_dag_node_t *) node->links->top->item;
    // node & u's gap
    time_gap = (double) u->pi->info.start.t - node->pi->info.end.t;
    gap = dv_layout_calculate_vgap(node->parent, time_gap);
    // node's linked u's outward
    dv_dag_node_t * last_tail = dv_layout_node_get_last_tail(node);
    if (last_tail)
      u->xp = dv_layout_node_get_displacement(last_tail);
    else
      u->xp = 0.0;
    u->y = node->y + node->dw + gap;
    // Recursive call
    dv_layout_node(u);
    // node's link-along
    node->link_lw = dv_max(node->lw, u->link_lw - u->x);
    node->link_rw = dv_max(node->rw, u->link_rw + u->x);
    node->link_dw = node->dw + gap + u->link_dw;
    break;
  case 2:
    u = (dv_dag_node_t *) node->links->top->item; // cont node
    v = (dv_dag_node_t *) node->links->top->next->item; // task node
    // node & u,v's gap
    time_gap = (double) u->pi->info.start.t - node->pi->info.end.t;
    ugap = dv_layout_calculate_vgap(node->parent, time_gap);
    time_gap = (double) v->pi->info.start.t - node->pi->info.end.t;
    vgap = dv_layout_calculate_vgap(node->parent, time_gap);
    // node's linked u,v's outward
    u->y = node->y + node->dw + ugap;
    v->y = node->y + node->dw + vgap;
    // Recursive call
    dv_layout_node(u);
    dv_layout_node(v);
    // node's linked u,v's outward
    u->xp = dv_layout_calculate_hgap(u->parent) + u->link_lw;
    v->xp = - dv_layout_calculate_hgap(v->parent) - v->link_rw;
    // node's link-along
    node->link_lw = dv_max(node->lw, - v->x + v->link_lw);
    node->link_rw = dv_max(node->rw, u->x + u->link_rw);
    node->link_dw = node->dw + dv_max(ugap + u->link_dw, vgap + v->link_dw);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

static void dv_layout_reset_coord(dv_dag_node_t *node) {
  /* Calculate inward */
  int is_single_node = 1;
  switch (node->pi->info.kind) {
  case dr_dag_node_kind_wait_tasks:
  case dr_dag_node_kind_end_task:
  case dr_dag_node_kind_create_task:
    break;
  case dr_dag_node_kind_section:
  case dr_dag_node_kind_task:
    if (dv_is_union(node)
        && ( !dv_is_shrinked(node) || dv_is_expanding(node) )) {
      dv_check(node->head);
      is_single_node = 0;
    }
    break;
  default:
    dv_check(0);
    break;
  }
  if (!is_single_node) {
    // node's head's outward
    node->head->xp = 0.0;
    node->head->x = node->x;
    // Recursive call
    dv_layout_node(node->head);
  }
    
  /* Calculate link-along */
  int n_links = dv_llist_size(node->links);
  dv_dag_node_t * u; // linked node 1
  dv_dag_node_t * v; // linked node 2
  switch (n_links) {
  case 0:
    break;
  case 1:
    u = (dv_dag_node_t *) node->links->top->item;
    // node's linked u's outward
    u->xp += node->xp;
    u->x = u->xp + u->parent->x;
    // Recursive call
    dv_layout_node(u);
    break;
  case 2:
    u = (dv_dag_node_t *) node->links->top->item; // cont node
    v = (dv_dag_node_t *) node->links->top->next->item; // task node
    // node's linked u,v's outward
    u->xp += node->xp;
    u->x = u->xp + u->parent->x;
    v->xp += node->xp;
    v->x = v->xp + v->parent->x;
    // Recursive call
    dv_layout_node(u);
    dv_layout_node(v);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

void dv_layout_dvdag(dv_dag_t *G) {
  printf("dv_layout_dvdag() begins ..\n");

  // Relative coord
  G->rt->xp = 0.0; // pre-based
  G->rt->y = 0.0;
  dv_layout_node(G->rt);

  // Absolute coord
  G->rt->xp = 0.0; // parent-based
  G->rt->x = 0.0;
  dv_layout_reset_coord(G->rt);

  printf("layout ended.\n");
}

void dv_relayout_dvdag(dv_dag_t *G) {
  
  // Relative coord
  G->rt->xp = 0.0; // pre-based
  G->rt->y = 0.0;
  dv_layout_node(G->rt);

  // Absolute coord
  G->rt->xp = 0.0; // parent-based
  G->rt->x = 0.0;
  dv_layout_reset_coord(G->rt);
  
}

/*-----------------end of Layout functions-----------*/


/*-----------Animation functions----------------------*/

static void dv_animation_set_moving_nodes(dv_dag_t *G, dv_animation_t *a) {
  dv_dag_node_t *node;
  int i;
  for (i=0; i<G->n; i++) {
    node = G->T + i;
    if (dv_is_union(node)) {
      
      if (node->d >= a->new_d && !dv_is_shrinked(node)) {
        // Need to be shrinked
        dv_node_flag_set(node->f, DV_NODE_FLAG_SHRINKING);
      } else if (node->d < a->new_d && dv_is_shrinked(node)) {
        // Need to be expanded
        dv_node_flag_set(node->f, DV_NODE_FLAG_EXPANDING);
      }
      
    }
  }
}

double dv_get_time()
{
  struct timeval tv;
  gettimeofday(&tv, NULL);
  // return in milliseconds
  return tv.tv_sec * 1.0E3 + ((double)tv.tv_usec) / 1.0E3;
}

void dv_animation_init(dv_animation_t *a) {
  a->on = 0;
  a->duration = 600; // milliseconds
  a->step = 50; // milliseconds
  a->started = 0.0;
  a->new_d = 0;
  a->ratio = 0.0;
}

static gboolean dv_animation_tick(gpointer data) {
  dv_animation_t *a = (dv_animation_t *) data;
  dv_check(a->on);
  a->ratio = (dv_get_time() - a->started) / a->duration;
  dv_check(a->ratio >= 0);
  if (a->ratio >= 1.0) {
    dv_animation_stop(a);
    gtk_widget_queue_draw(darea);
    return 0;
  }
  dv_relayout_dvdag(G);
  gtk_widget_queue_draw(darea);
  return 1;
}

void dv_animation_start(dv_animation_t *a) {
  dv_animation_set_moving_nodes(G, a);
  a->on = 1;
  a->ratio = 0.0;
  a->started = dv_get_time();
  g_timeout_add(a->step, dv_animation_tick, a);
}

void dv_animation_stop(dv_animation_t *a) {
  a->on = 0;
  S->cur_d = a->new_d;
  
  dv_dag_node_t *node;
  int i;
  for (i=0; i<G->n; i++) {
    node = G->T + i;
    if (dv_node_flag_check(node->f, DV_NODE_FLAG_SHRINKING)) {
      dv_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKING);
      dv_node_flag_set(node->f, DV_NODE_FLAG_SHRINKED);
    } else if (dv_node_flag_check(node->f, DV_NODE_FLAG_EXPANDING)) {
      dv_node_flag_remove(node->f, DV_NODE_FLAG_EXPANDING);
      dv_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKED);
    }
  }
  dv_relayout_dvdag(G);
  gtk_widget_queue_draw(darea);
}

/*-----------end of Animation functions----------------------*/
