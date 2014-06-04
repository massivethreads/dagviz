#include "dagviz.h"

/*-----Common functions-----*/

double dv_layout_calculate_gap(dv_dag_node_t *node) {
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

/*-----end of Common functions-----*/

/*-----------------BBox layout functions-----------*/

static double dv_layout_calculate_hgap(dv_dag_node_t *node) {
  double gap = dv_layout_calculate_gap(node);
  double hgap = gap * DV_HDIS;
  return hgap;
}

double dv_layout_calculate_hsize(dv_dag_node_t *node) {
  double gap = dv_layout_calculate_gap(node->parent);
  double hsize = gap * DV_RADIUS;
  return hsize;
}

static double dv_layout_calculate_vresize(double val) {
  double ret;
  switch (S->sdt) {
  case 0:
    ret = log(val) / log(S->log_radix);
    break;
  case 1:
    ret = pow(val, S->power_radix);
    break;
  default:
    dv_check(0);
    break;
  }
  return ret;
}

static double dv_layout_calculate_vgap(dv_dag_node_t *parent, dv_dag_node_t *node1, dv_dag_node_t *node2) {
  double gap = dv_layout_calculate_gap(parent);
  double vgap;
  if (!S->frombt) {
    // begin - end
    double time_gap = dv_layout_calculate_vresize(node2->pi->info.start.t - node1->pi->info.end.t);
    vgap = gap * time_gap;
  } else {
    // from beginning
    double time1 = dv_layout_calculate_vresize(node1->pi->info.end.t - G->bt);
    double time2 = dv_layout_calculate_vresize(node2->pi->info.start.t - G->bt);
    vgap = gap * (time2 - time1);
  }
  return vgap;
}

double dv_layout_calculate_vsize(dv_dag_node_t *node) {
  double gap = dv_layout_calculate_gap(node->parent);
  double vsize;
  if (!S->frombt) {
    // begin - end
    double time_gap = dv_layout_calculate_vresize(node->pi->info.end.t - node->pi->info.start.t);
    vsize = gap * time_gap;
  } else {
    // from beginning
    double time1 = dv_layout_calculate_vresize(node->pi->info.start.t - G->bt);
    double time2 = dv_layout_calculate_vresize(node->pi->info.end.t - G->bt);
    vsize = gap * (time2 - time1);
  }
  return vsize;
}

double dv_layout_calculate_vsize_pure(dv_dag_node_t *node) {
  double time = node->pi->info.end.t - node->pi->info.start.t;
  double vsize = log(time)/log(DV_VLOG);
	/*double time1 = log(node->pi->info.start.t - G->bt) / log(DV_VLOG);
	double time2 = log(node->pi->info.end.t - G->bt) / log(DV_VLOG);
	double vsize = (time2 - time1);
	printf("vsize %0.1lf -> %0.1lf = %0.1lf\n", time1, time2, vsize);*/
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

static double dv_layout_node_get_node_xp(dv_dag_node_t *node) {
  dv_dag_node_t * u = node;
  double dis = 0.0;
  while (u) {
    dis += u->xpre;
    u = u->pre;
  }
  return dis;
}

static double dv_layout_node_get_last_tail_xp_r(dv_dag_node_t *node) {
  dv_check(node);
  double ret = 0.0;
  dv_dag_node_t * last_tail = 0;
  if (!dv_is_single(node))
    last_tail = dv_layout_node_get_last_tail(node);
  if (last_tail) {
    ret += dv_layout_node_get_node_xp(last_tail);
    ret += dv_layout_node_get_last_tail_xp_r(last_tail);
  }
  return ret;
}

static void dv_layout_bbox_node(dv_dag_node_t *node) {
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
    node->head->xpre = 0.0;
    node->head->y = node->y;
    // Recursive call
    dv_layout_bbox_node(node->head);
    // node's inward
    node->lw = node->head->link_lw;
    node->rw = node->head->link_rw;
    node->dw = node->head->link_dw;
		node->avoid_inward = 0;
		// avoid shrinking too small
		double comp = dv_layout_calculate_vsize_pure(node);
		if (node->lw < DV_RADIUS
				&& node->rw < DV_RADIUS
				&& node->dw < comp) {
			node->lw = DV_RADIUS;
			node->rw = DV_RADIUS;
			node->dw = comp;
			node->avoid_inward = 1;
		}
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
    gap = dv_layout_calculate_vgap(node->parent, node, u);
    // node's linked u's outward    
    u->xpre = dv_layout_node_get_last_tail_xp_r(node);
    u->y = node->y + node->dw + gap;
    // Recursive call
    dv_layout_bbox_node(u);
    // node's link-along
    node->link_lw = dv_max(node->lw, u->link_lw - u->xpre);
    node->link_rw = dv_max(node->rw, u->link_rw + u->xpre);
    node->link_dw = node->dw + gap + u->link_dw;
    break;
  case 2:
    u = (dv_dag_node_t *) node->links->top->item; // cont node
    v = (dv_dag_node_t *) node->links->top->next->item; // task node
    // node & u,v's gap
    ugap = dv_layout_calculate_vgap(node->parent, node, u);
    vgap = dv_layout_calculate_vgap(node->parent, node, v);
    // node's linked u,v's outward
    u->y = node->y + node->dw + ugap;
    v->y = node->y + node->dw + vgap;
    // Recursive call
    dv_layout_bbox_node(u);
    dv_layout_bbox_node(v);
    // node's linked u,v's outward
    u->xpre = dv_layout_calculate_hgap(u->parent) + u->link_lw;
    v->xpre = - dv_layout_calculate_hgap(v->parent) - v->link_rw;
    // node's link-along
    node->link_lw = dv_max(node->lw, - v->xpre + v->link_lw);
    node->link_rw = dv_max(node->rw, u->xpre + u->link_rw);
    node->link_dw = node->dw + dv_max(ugap + u->link_dw, vgap + v->link_dw);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

static void dv_layout_bbox_node_2nd(dv_dag_node_t *node) {
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
    dv_layout_bbox_node_2nd(node->head);
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
    u->xp = u->xpre + node->xp;
    u->x = u->xp + u->parent->x;
    // Recursive call
    dv_layout_bbox_node_2nd(u);
    break;
  case 2:
    u = (dv_dag_node_t *) node->links->top->item; // cont node
    v = (dv_dag_node_t *) node->links->top->next->item; // task node
    // node's linked u,v's outward
    u->xp = u->xpre + node->xp;
    u->x = u->xp + u->parent->x;
    v->xp = v->xpre + node->xp;
    v->x = v->xp + v->parent->x;
    // Recursive call
    dv_layout_bbox_node_2nd(u);
    dv_layout_bbox_node_2nd(v);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

void dv_layout_bbox_dvdag(dv_dag_t *G) {

  // Relative coord
  G->rt->xpre = 0.0; // pre-based
  G->rt->y = 0.0;
  dv_layout_bbox_node(G->rt);

  // Absolute coord
  G->rt->xp = 0.0; // parent-based
  G->rt->x = 0.0;
  dv_layout_bbox_node_2nd(G->rt);

  // Check
  //print_layout(G);  

}

/*-----------------end of BBox layout functions-----------*/


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
  a->duration = DV_ANIMATION_DURATION;
  a->step = DV_ANIMATION_STEP;
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


/*-----------Timeline layout functions----------------------*/

static void dv_layout_timeline_node(dv_dag_node_t *node) {
  /* Calculate inward */
  int is_single_node = 1;
  switch (node->pi->info.kind) {
  case dr_dag_node_kind_wait_tasks:
  case dr_dag_node_kind_end_task:
  case dr_dag_node_kind_create_task:
    break;
  case dr_dag_node_kind_section:
  case dr_dag_node_kind_task:
    if (dv_is_union(node))
      is_single_node = 0;
    break;
  default:
    dv_check(0);
    break;
  }
  // node's inward
  node->lw = dv_layout_calculate_hsize(node);
  node->rw = dv_layout_calculate_hsize(node);
  node->dw = dv_layout_calculate_vresize(node->pi->info.end.t - G->bt) - dv_layout_calculate_vresize(node->pi->info.start.t - G->bt);
  // node's outward
  int worker = node->pi->info.worker;
  node->x = DV_RADIUS + worker * (2 * DV_RADIUS + DV_HDIS);
  node->y = dv_layout_calculate_vresize(node->pi->info.start.t - G->bt);
  if (!is_single_node) {
    // Recursive call
    if (!dv_is_shrinked(node))
      dv_layout_timeline_node(node->head);
  }
    
  /* Calculate link-along */
  int n_links = dv_llist_size(node->links);
  dv_dag_node_t * u; // linked node 1
  dv_dag_node_t * v; // linked node 2
  double time_gap, gap, ugap, vgap;
  switch (n_links) {
  case 0:
    break;
  case 1:
    u = (dv_dag_node_t *) node->links->top->item;
    // Recursive call
    dv_layout_timeline_node(u);
    break;
  case 2:
    u = (dv_dag_node_t *) node->links->top->item; // cont node
    v = (dv_dag_node_t *) node->links->top->next->item; // task node
    // Recursive call
    dv_layout_timeline_node(u);
    dv_layout_timeline_node(v);
    break;
  default:
    dv_check(0);
    break;
  }
  
}

static void dv_layout_timeline_dvdag(dv_dag_t *G) {

  // Absolute coord
  dv_layout_timeline_node(G->rt);

  // Check
  //print_layout(G);  
  
}

/*-----------end of Timeline layout functions----------------------*/


/*-----------Main layout functions-------------------------*/

void dv_layout_dvdag(dv_dag_t *G) {
	
	if (S->lt == 0)
		dv_layout_glike_dvdag(G);
	else if (S->lt == 1)
		dv_layout_bbox_dvdag(G);
	else if (S->lt == 2)
		dv_layout_timeline_dvdag(G);

}

void dv_relayout_dvdag(dv_dag_t *G) {

	if (S->lt == 0)
		dv_relayout_glike_dvdag(G);
	else if (S->lt == 1)
		dv_layout_bbox_dvdag(G);
	else if (S->lt == 2)
		dv_layout_timeline_dvdag(G);
  
}

/*-----------end of Main layout functions-------------------------*/

