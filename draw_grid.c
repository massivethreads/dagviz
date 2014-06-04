#include "dagviz.h"


/*-----------------Gridlike Drawing functions-----------*/

static void draw_dvdag_node_1(cairo_t *cr, dv_dag_node_t *node) {
  // Count node drawn
  S->nd++;
  // Node color
  double x = node->vl->c;
  double y = node->c;
  double c[4];
  dv_lookup_color(node, c, c+1, c+2, c+3);
  // Alpha
  double alpha = 1.0;
  // Draw path
  cairo_save(cr);
  cairo_new_path(cr);
  if (dv_is_union(node)) {

    double xx, yy, w, h;
    if (dv_is_expanding(node) || dv_is_shrinking(node)) {

      double margin = 1.0;
      if (dv_is_expanding(node)) {
        // Fading out
        alpha = dv_get_alpha_fading_out();
        margin = dv_get_alpha_fading_in();
      } else {
        // Fading in
        alpha = dv_get_alpha_fading_in();
        margin = dv_get_alpha_fading_out();
      }
      // Large-sized box
      margin *= DV_UNION_NODE_MARGIN;
      xx = x - node->lc * DV_HDIS - DV_RADIUS - margin;
      yy = y - DV_RADIUS - margin;
      dv_dag_node_t * hd = node->head;
      h = hd->dc * DV_VDIS + 2 * (DV_RADIUS + margin);
      w = (node->lc + node->rc) * DV_HDIS + 2 * (DV_RADIUS + margin);
      
    } else {
      
      // Normal-sized box
      xx = x - DV_RADIUS;
      yy = y - DV_RADIUS;
      w = 2 * DV_RADIUS;
      h = 2 * DV_RADIUS;
      if (node->d > S->a->new_d) {
        // Fading out
        alpha = dv_get_alpha_fading_out();
      } else if (node->d > S->cur_d) {
        // Fading in
        alpha = dv_get_alpha_fading_in();
      }
      
    }

    // Box
    cairo_move_to(cr, xx, yy);
    cairo_line_to(cr, xx + w, yy);
    cairo_line_to(cr, xx + w, yy + h);
    cairo_line_to(cr, xx, yy + h);
    cairo_close_path(cr);
    
  } else {
    
    // Normal-sized circle
    cairo_arc(cr, x, y, DV_RADIUS, 0.0, 2*M_PI);
    if (node->d > S->a->new_d) {
      // Fading out
      alpha = dv_get_alpha_fading_out();
    } else if (node->d > S->cur_d) {
      // Fading in
      alpha = dv_get_alpha_fading_in();
    }
    
  }
  
  // Draw node
  cairo_set_source_rgba(cr, c[0], c[1], c[2], c[3] * alpha);
  cairo_fill_preserve(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
  cairo_stroke(cr);
  cairo_restore(cr);
}

static void draw_grid_vl(cairo_t *cr, dv_grid_line_t *l, double y1, double y2) {
  double x;
  x = l->c;
  cairo_save(cr);
  cairo_new_path(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0 ,0.8);
  cairo_move_to(cr, x, y1);
  cairo_line_to(cr, x, y2);
  cairo_stroke(cr);
  cairo_restore(cr);
}

static void draw_grid_hl(cairo_t *cr, dv_grid_line_t *l, double x1, double x2) {
  double y;
  y = 0.0;
  dv_dag_node_t *node;
  dv_llist_iterate_init(l->L);
  node = dv_llist_iterate_next(l->L);
  if (node)
    y = node->c;
  cairo_save(cr);
  cairo_new_path(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0 ,0.8);
  cairo_move_to(cr, x1, y);
  cairo_line_to(cr, x2, y);
  cairo_stroke(cr);
  cairo_restore(cr);
}

static void draw_grid(cairo_t *cr, dv_dag_node_t *node) {
  // VL
  double y1, y2;
  y1 = node->c - DV_RADIUS - DV_UNION_NODE_MARGIN;
  y2 = node->c + DV_RADIUS + DV_UNION_NODE_MARGIN;
  dv_dag_node_t *nn;
  if (nn = node->head) {
    y2 += nn->dc * DV_VDIS;
  }
  dv_grid_line_t *l;
  l = node->grid->vl;
  draw_grid_vl(cr, l, y1, y2);
  while (l->l) {
    l = l->l;
    draw_grid_vl(cr, l, y1, y2);
  }
  l = node->grid->vl;
  while (l->r) {
    l = l->r;
    draw_grid_vl(cr, l, y1, y2);
  }
  // HL
  double x1, x2;
  x1 = node->vl->c - node->vl->lc * DV_HDIS - DV_RADIUS - DV_UNION_NODE_MARGIN;
  x2 = node->vl->c + node->vl->rc * DV_HDIS + DV_RADIUS + DV_UNION_NODE_MARGIN;
  l = node->grid->hl;
  draw_grid_hl(cr, l, x1, x2);
  while (l->l) {
    l = l->l;
    draw_grid_hl(cr, l, x1, x2);
  }
  l = node->grid->hl;
  while (l->r) {
    l = l->r;
    draw_grid_hl(cr, l, x1, x2);
  }
}

static void draw_dvdag_node_r(cairo_t *cr, dv_dag_node_t *u) {
  if (!u) return;
  int call_head = 0;
  if (!dv_is_union(u)
      || dv_is_shrinked(u) || dv_is_shrinking(u)) {
    draw_dvdag_node_1(cr, u);
  }
  // Iterate links
  dv_dag_node_t * v;
  dv_llist_iterate_init(u->links);
  while (v = (dv_dag_node_t *) dv_llist_iterate_next(u->links)) {
    draw_dvdag_node_r(cr, v);    
  }
  // Call head
  if (dv_is_union(u)
      && ( !dv_is_shrinked(u) || dv_is_expanding(u) )) {
		draw_dvdag_node_r(cr, u->head);
    // Draw grid
    //draw_grid(cr, u);
  }
}

static void draw_dvdag_edge_1(cairo_t *cr, dv_dag_node_t *u, dv_dag_node_t *v) {
  if (u->c + DV_RADIUS > v->c - DV_RADIUS)
    return;
  double alpha = 1.0;
  if (u->d > S->a->new_d && v->d > S->a->new_d)
    alpha = dv_get_alpha_fading_out();
  else if (u->d > S->cur_d && v->d > S->cur_d
           && u->d <= S->a->new_d
           && v->d <= S->a->new_d)
    alpha = dv_get_alpha_fading_in();
  cairo_save(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
  cairo_move_to(cr, u->vl->c, u->c + DV_RADIUS);
  cairo_line_to(cr, v->vl->c, v->c - DV_RADIUS);
  cairo_stroke(cr);
  cairo_restore(cr);
}

static dv_dag_node_t * dv_dag_node_get_first(dv_dag_node_t *u) {
  dv_check(u);
  while (dv_node_flag_check(u->f, DV_NODE_FLAG_UNION)
         && (!dv_node_flag_check(u->f, DV_NODE_FLAG_SHRINKED)
             || dv_node_flag_check(u->f, DV_NODE_FLAG_EXPANDING))) {
    dv_check(u->head);
    u = u->head;
  }
  return u;
}

static dv_dag_node_t * dv_dag_node_get_last(dv_dag_node_t *u) {
  dv_check(u);
  while (dv_node_flag_check(u->f, DV_NODE_FLAG_UNION)
         && (!dv_node_flag_check(u->f, DV_NODE_FLAG_SHRINKED)
             || dv_node_flag_check(u->f, DV_NODE_FLAG_EXPANDING))) {
    dv_check(u->tails->top);
    u = (dv_dag_node_t *) u->tails->top->item;
    dv_check(u);
  }
  return u;
}

static void draw_dvdag_edge_r(cairo_t *cr, dv_dag_node_t *u) {
  if (!u) return;
  // Iterate links
  dv_dag_node_t * v;
  dv_llist_iterate_init(u->links);
  while (v = (dv_dag_node_t *) dv_llist_iterate_next(u->links)) {

    dv_dag_node_t *u_tail, *v_head;
    if (!dv_is_union(u)
        || (dv_is_shrinked(u) && !dv_is_expanding(u))) {
      
      if (!dv_is_union(v)
          || (dv_is_shrinked(v) && !dv_is_expanding(v))) {
        
        draw_dvdag_edge_1(cr, u, v);
        
      } else {
        
        v_head = v->head;
				dv_dag_node_t * v_first = dv_dag_node_get_first(v_head);
				draw_dvdag_edge_1(cr, u, v_first);
        
      }
      
    } else {
      
      dv_llist_iterate_init(u->tails);
      while (u_tail = (dv_dag_node_t *) dv_llist_iterate_next(u->tails)) {
        dv_dag_node_t * u_last = dv_dag_node_get_last(u_tail);
        
        if (!dv_is_union(v)
            || (dv_is_shrinked(v) && !dv_is_expanding(v))) {
          draw_dvdag_edge_1(cr, u_last, v);
        } else {
          
          v_head = v->head;
					dv_dag_node_t * v_first = dv_dag_node_get_first(v_head);
					draw_dvdag_edge_1(cr, u_last, v_first);
          
        }
        
      }
      
    }
    draw_dvdag_edge_r(cr, v);
    
  }

  // Call head
  if (dv_is_union(u)
      && ( !dv_is_shrinked(u) || dv_is_expanding(u) )) {
		draw_dvdag_edge_r(cr, u->head);
  }
  
}

void dv_draw_glike_dvdag(cairo_t *cr, dv_dag_t *G) {
  cairo_set_line_width(cr, 2.0);
  int i;
  // Draw nodes
  S->nd = 0;
  draw_dvdag_node_r(cr, G->rt);
  // Draw edges
  draw_dvdag_edge_r(cr, G->rt);
}

/*-----------------end of Gridlike DAG drawing functions-----------*/

