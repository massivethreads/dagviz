#include "dagviz.h"

/*-----------------Common Drawing functions-----------*/

void dv_draw_text(cairo_t *cr) {
  cairo_select_font_face(cr, "Serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_NORMAL);
  cairo_set_font_size(cr, 13);

  const int n_glyphs = 20 * 35;
  cairo_glyph_t glyphs[n_glyphs];

  gint i = 0;  
  gint x, y;
  
  for (y=0; y<20; y++) {
      for (x=0; x<35; x++) {
          glyphs[i] = (cairo_glyph_t) {i, x*15 + 20, y*18 + 20};
          i++;
      }
  }
  
  cairo_show_glyphs(cr, glyphs, n_glyphs);
}

void dv_draw_rounded_rectangle(cairo_t *cr, double x, double y, double width, double height)
{
  double aspect = 1.0;
  double corner_radius = height / 10.0;

  double radius = corner_radius / aspect;
  double degrees = M_PI / 180.0;

  cairo_new_sub_path(cr);
  cairo_arc(cr, x+width-radius, y+radius, radius, -90*degrees, 0*degrees);
  cairo_arc(cr, x+width-radius, y+height-radius, radius, 0*degrees, 90*degrees);
  cairo_arc(cr, x+radius, y+height-radius, radius, 90*degrees, 180*degrees);
  cairo_arc(cr, x+radius, y+radius, radius, 180*degrees, 270*degrees);
  cairo_close_path(cr);

  cairo_set_source_rgba(cr, 0.1, 0.1, 0.1, 0.6);
  cairo_fill_preserve(cr);
  cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
  cairo_set_line_width(cr, 1.0);
  cairo_stroke(cr);
}

static int dv_get_color_pool_index(int t, int v0, int v1, int v2, int v3) {
  int n = S->CP_sizes[t];
  int i;
  for (i=0; i<n; i++) {
    if (S->CP[t][i][0] == v0 && S->CP[t][i][1] == v1
        && S->CP[t][i][2] == v2 && S->CP[t][i][3] == v3)
      return i;
  }
  dv_check(n < DV_COLOR_POOL_SIZE);
  S->CP[t][n][0] = v0;
  S->CP[t][n][1] = v1;
  S->CP[t][n][2] = v2;
  S->CP[t][n][3] = v3;
  S->CP_sizes[t]++;
  return n;
}

static void dv_lookup_color_value(int v, double *r, double *g, double *b, double *a) {
  GdkRGBA color;
  gdk_rgba_parse(&color, DV_COLORS[(v + NUM_COLORS) % NUM_COLORS]);
  *r = color.red;
  *g = color.green;
  *b = color.blue;
  *a = color.alpha;
  /*  *r = 0.44;
  *g = 0.55;
  *b = 0.66;
  *a = 1.0;
  double *t;
  int i = 0;
  while (i <= v) {
    switch (i % 3) {
    case 0:      
      t = r; break;
    case 1:
      t = g; break;
    case 2:
      t = b; break;
    }
    *t += 0.37;
    if (*t > 1.0)
      *t -= 1.0;
    i++;
    }*/
}

void dv_lookup_color(dv_dag_node_t *node, double *r, double *g, double *b, double *a) {
  int v = 0;
  dv_check(S->nc < DV_NUM_COLOR_POOLS);
  switch (S->nc) {
  case 0:
    v = dv_get_color_pool_index(S->nc, 0, 0, 0, node->pi->info.worker);
    break;
  case 1:
    v = dv_get_color_pool_index(S->nc, 0, 0, 0, node->pi->info.cpu);
    break;
  case 2:
    v = dv_get_color_pool_index(S->nc, 0, 0, 0, node->pi->info.kind);
    break;
  case 3:
    v = dv_get_color_pool_index(S->nc, 0, 0, node->pi->info.start.pos.file_idx, node->pi->info.start.pos.line);
    break;
  case 4:
    v = dv_get_color_pool_index(S->nc, 0, 0, node->pi->info.end.pos.file_idx, node->pi->info.end.pos.line);
    break;
  case 5:
    v = dv_get_color_pool_index(S->nc, node->pi->info.start.pos.file_idx, node->pi->info.start.pos.line, node->pi->info.end.pos.file_idx, node->pi->info.end.pos.line);
    break;
  default:
    dv_check(0);
    break;
  }  
  dv_lookup_color_value(v, r, g, b, a);
}


double dv_get_alpha_fading_out() {
  double ratio = S->a->ratio;
  double ret;
  //ret = (1.0 - ratio) * 0.75;
  ret = 1.0 - ratio * ratio;
  return ret;
}

double dv_get_alpha_fading_in() {
  double ratio = S->a->ratio;
  double ret;
  //ret = ratio * 1.5;
  ret = ratio * ratio;
  return ret;
}

/*-----------------end of Common Drawing functions-----------*/


/*-----------------DAG BBox Drawing functions-----------*/

static void draw_dvdag_node_1(cairo_t *cr, dv_dag_node_t *node) {
  // Count node drawn
  S->nd++;
  // Node color
  double x = node->x;
  double y = node->y;
  double c[4];
  dv_lookup_color(node, c, c+1, c+2, c+3);
  // Alpha
  double alpha = 1.0;
  // Draw path
  cairo_save(cr);
  cairo_new_path(cr);
  double xx, yy, w, h;
  if (dv_is_union(node)) {

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
      xx = x - node->lw - margin;
      yy = y - margin;
      w = node->lw + node->rw + 2 * margin;
      h = node->dw + 2 * margin;
      
    } else {
      
      // Normal-sized box
      xx = x - node->lw;
      yy = y;
      w = node->lw + node->rw;
      h = node->dw;
      if (node->d > S->a->new_d) {
        // Fading out
        alpha = dv_get_alpha_fading_out();
      } else if (node->d > S->cur_d) {
        // Fading in
        alpha = dv_get_alpha_fading_in();
      }
      
    }

  } else {
    
    // Normal-sized box (terminal node)
    xx = x - node->lw;
    yy = y;
    w = node->lw + node->rw;
    h = node->dw;
    if (node->d > S->a->new_d) {
      // Fading out
      alpha = dv_get_alpha_fading_out();
    } else if (node->d > S->cur_d) {
      // Fading in
      alpha = dv_get_alpha_fading_in();
    }
    
  }
  // Draw path
  cairo_move_to(cr, xx, yy);
  cairo_line_to(cr, xx + w, yy);
  cairo_line_to(cr, xx + w, yy + h);
  cairo_line_to(cr, xx, yy + h);
  cairo_close_path(cr);

  // Draw node
  cairo_set_source_rgba(cr, c[0], c[1], c[2], c[3] * alpha);
  cairo_fill_preserve(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
  cairo_stroke(cr);
  cairo_restore(cr);
}

static void draw_dvdag_node_r(cairo_t *cr, dv_dag_node_t *node) {
  /* Draw node */
  dv_check(node);
  if (!dv_is_union(node)
      || dv_is_shrinked(node) || dv_is_shrinking(node)) {
    draw_dvdag_node_1(cr, node);
  }

  /* Calculate inward */
  int is_single_node = 1;
  switch (node->pi->info.kind) {
  case dr_dag_node_kind_wait_tasks:
  case dr_dag_node_kind_end_task:
  case dr_dag_node_kind_create_task:
    break;
  case dr_dag_node_kind_section:
  case dr_dag_node_kind_task:
    if (!dv_is_single(node)) {
      dv_check(node->head);
      is_single_node = 0;
    }
    break;
  default:
    dv_check(0);
    break;
  }
  if (!is_single_node) {
    // Recursive call
		if (!node->avoid_inward)
			draw_dvdag_node_r(cr, node->head);
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
    // Recursive call
    draw_dvdag_node_r(cr, u);
    break;
  case 2:
    u = (dv_dag_node_t *) node->links->top->item; // cont node
    v = (dv_dag_node_t *) node->links->top->next->item; // task node
    // Recursive call
    draw_dvdag_node_r(cr, u);
    draw_dvdag_node_r(cr, v);
    break;
  default:
    dv_check(0);
    break;
  }
}

static void draw_dvdag_edge_1(cairo_t *cr, dv_dag_node_t *u, dv_dag_node_t *v) {
  if (u->y > v->y)
    return;
  double alpha = 1.0;
  if (u->d > S->a->new_d && v->d > S->a->new_d)
    alpha = dv_get_alpha_fading_out();
  else if (u->d > S->cur_d
           && v->d > S->cur_d
           && u->d <= S->a->new_d
           && v->d <= S->a->new_d)
    alpha = dv_get_alpha_fading_in();
  cairo_save(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
  double x1, y1, x2, y2;
  x1 = u->x;
  y1 = u->y + u->dw;
  x2 = v->x;
  y2 = v->y;
  cairo_move_to(cr, x1, y1);
  cairo_line_to(cr, x1, y2);
  cairo_line_to(cr, x2, y2);
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
    dv_check(u);
  }
  return u;
}

static void draw_dvdag_edge_last_r(cairo_t *cr, dv_dag_node_t *u, dv_dag_node_t *v) {
	if (u->avoid_inward) {
		draw_dvdag_edge_1(cr, u, v);
		return;
	}
  dv_llist_iterate_init(u->tails);
  dv_dag_node_t *u_tail;
  while (u_tail = (dv_dag_node_t *) dv_llist_iterate_next(u->tails)) {
    if (dv_is_single(u_tail))
      draw_dvdag_edge_1(cr, u_tail, v);
    else
      draw_dvdag_edge_last_r(cr, u_tail, v);
  }  
}

static void draw_dvdag_edge_r(cairo_t *cr, dv_dag_node_t *u) {
  if (!u) return;
  // Call head
  if (!dv_is_single(u)) {
		if (!u->avoid_inward)
			draw_dvdag_edge_r(cr, u->head);
  }
  // Iterate links
  dv_dag_node_t * v;
  dv_llist_iterate_init(u->links);
  while (v = (dv_dag_node_t *) dv_llist_iterate_next(u->links)) {

    if (dv_is_single(u)) {
      
      if (dv_is_single(v))
        draw_dvdag_edge_1(cr, u, v);
      else
        draw_dvdag_edge_1(cr, u, dv_dag_node_get_first(v->head));
      
    } else {
    
      if (dv_is_single(v))
        draw_dvdag_edge_last_r(cr, u, v);
      else
        draw_dvdag_edge_last_r(cr, u, dv_dag_node_get_first(v->head));

    }
    draw_dvdag_edge_r(cr, v);
    
  }
}

static void dv_draw_bbox_infotag_1(cairo_t *cr, dv_dag_node_t *node) {
  double line_height = 12;
  double padding = 4;
  int n = 6; /* number of lines */
  double xx, yy;
  // Split process based on layout type
  if (S->lt == 0) {
    // grid-like layout
    xx = node->vl->c + DV_RADIUS + 2*padding;
    yy = node->c - DV_RADIUS - 2*padding - line_height*(n-1);
  } else if (S->lt == 1 || S->lt == 2) {
    // bbox/timeline layouts    
    xx = node->x + node->rw + 2 * padding;
    yy = node->y - 2 * padding - line_height * (n - 1);
  } else
    dv_check(0);

  // Cover rectangle
  double width = 450.0;
  double height = n * line_height + 2 * padding;
  dv_draw_rounded_rectangle(cr,
                         xx - padding,
                         yy - line_height - padding,
                         width,
                         height);

  // Lines
  cairo_set_source_rgb(cr, 1.0, 1.0, 0.1);
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);

  // Line 1
  /* TODO: adaptable string length */
  char *s = (char *) dv_malloc( DV_STRING_LENGTH * sizeof(char) );
  sprintf(s, "[%ld] %s d=%d f=%d%d%d%d",
          node - G->T,
          NODE_KIND_NAMES[node->pi->info.kind],
          node->d,
          dv_is_union(node),
          dv_is_shrinked(node),
          dv_is_expanding(node),
          dv_is_shrinking(node));
  cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
  yy += line_height;
    
  // Line 2
  sprintf(s, "%llu-%llu (%llu) est=%llu",
          node->pi->info.start.t,
          node->pi->info.end.t,
          node->pi->info.end.t - node->pi->info.start.t,
          node->pi->info.est);
  cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
  yy += line_height;

  // Line 3
  sprintf(s, "T=%llu/%llu,nodes=%ld/%ld/%ld,edges=%ld/%ld/%ld/%ld",
          node->pi->info.t_1, 
          node->pi->info.t_inf,
          node->pi->info.logical_node_counts[dr_dag_node_kind_create_task],
          node->pi->info.logical_node_counts[dr_dag_node_kind_wait_tasks],
          node->pi->info.logical_node_counts[dr_dag_node_kind_end_task],
          node->pi->info.logical_edge_counts[dr_dag_edge_kind_end],
          node->pi->info.logical_edge_counts[dr_dag_edge_kind_create],
          node->pi->info.logical_edge_counts[dr_dag_edge_kind_create_cont],
          node->pi->info.logical_edge_counts[dr_dag_edge_kind_wait_cont]);
  cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
  yy += line_height;

  // Line 4
  sprintf(s, "by worker %d on cpu %d",
          node->pi->info.worker, 
          node->pi->info.cpu);
  cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
  yy += line_height;

  // Line 5
  dv_free(s, DV_STRING_LENGTH * sizeof(char));
  const char *ss = P->S->C + P->S->I[node->pi->info.start.pos.file_idx];
  s = (char *) dv_malloc( strlen(ss) + 10 );
  sprintf(s, "%s:%ld",
          ss,
          node->pi->info.start.pos.line);
  cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
  yy += line_height;

  // Line 6
  dv_free(s, strlen(ss) + 10);
  ss = P->S->C + P->S->I[node->pi->info.end.pos.file_idx];
  s = (char *) dv_malloc( strlen(ss) + 10 );  
  sprintf(s, "%s:%ld",
          ss,
          node->pi->info.end.pos.line);
  cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
  yy += line_height;
  
  dv_free(s, strlen(ss) + 10);
}

void dv_draw_bbox_dvdag(cairo_t *cr, dv_dag_t *G) {
  cairo_set_line_width(cr, DV_NODE_LINE_WIDTH);
  int i;
  // Draw nodes
  S->nd = 0;
  draw_dvdag_node_r(cr, G->rt);
  // Draw edges
  draw_dvdag_edge_r(cr, G->rt);
}


/*-----------------end of DAG BBox drawing functions-----------*/


/*-----------------TIMELINE Drawing functions-----------*/

static void draw_timeline_dvdag_node_1(cairo_t *cr, dv_dag_node_t *node) {
  // Count node drawn
  S->nd++;
  // Node color
  double x = node->x;
  double y = node->y;
  double c[4];
  dv_lookup_color(node, c, c+1, c+2, c+3);
  // Alpha
  double alpha = 1.0;
  // Draw path
  cairo_save(cr);
  cairo_new_path(cr);
  double xx, yy, w, h;
  // Normal-sized box (terminal node)
  xx = x - node->lw;
  yy = y;
  w = node->lw + node->rw;
  h = node->dw;
  // Draw path
  cairo_move_to(cr, xx, yy);
  cairo_line_to(cr, xx + w, yy);
  cairo_line_to(cr, xx + w, yy + h);
  cairo_line_to(cr, xx, yy + h);
  cairo_close_path(cr);

  // Draw node
  cairo_set_source_rgba(cr, c[0], c[1], c[2], c[3] * alpha);
  cairo_fill_preserve(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
  cairo_stroke(cr);
  cairo_restore(cr);
}

static void draw_timeline_dvdag_node_r(cairo_t *cr, dv_dag_node_t *node) {
  dv_check(node);
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
  if (is_single_node) {
    draw_timeline_dvdag_node_1(cr, node);
  } else {
    // Recursive call
		if (!dv_is_shrinked(node))
			draw_timeline_dvdag_node_r(cr, node->head);
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
    // Recursive call
    draw_timeline_dvdag_node_r(cr, u);
    break;
  case 2:
    u = (dv_dag_node_t *) node->links->top->item; // cont node
    v = (dv_dag_node_t *) node->links->top->next->item; // task node
    // Recursive call
    draw_timeline_dvdag_node_r(cr, u);
    draw_timeline_dvdag_node_r(cr, v);
    break;
  default:
    dv_check(0);
    break;
  }
}

static void dv_draw_timeline_dvdag(cairo_t *cr, dv_dag_t *G) {
  cairo_set_line_width(cr, DV_NODE_LINE_WIDTH);
  int i;
  // Draw nodes
  S->nd = 0;
  draw_timeline_dvdag_node_r(cr, G->rt);
  // Draw worker numbers
  cairo_set_source_rgb(cr, 0.1, 0.1, 0.1);
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);
  char *s = (char *) dv_malloc( DV_STRING_LENGTH * sizeof(char) );
  double xx, yy;
  xx = DV_RADIUS;
  yy = -4;
  for (i=0; i<G->nw; i++) {
    sprintf(s, "Worker %d", i);            
    cairo_move_to(cr, xx - 30, yy);
    cairo_show_text(cr, s);
    xx += 2 * DV_RADIUS + DV_HDIS;
  }
}

/*-----------------end of TIMELINE drawing functions-----------*/


/*-----Main drawing functions-----*/

void dv_draw_status(cairo_t *cr) {
  cairo_save(cr);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 14);

  char *s[10];
  int n = 0;
  int length = 50;

  // Depth
  s[n] = (char *) dv_malloc( length * sizeof(char) );
  sprintf(s[n], "D=%d/%d", S->cur_d, G->dmax);
  n++;

  // Nodes drawn
  s[n] = (char *) dv_malloc( length * sizeof(char) );
  sprintf(s[n], "ND=%ld, ", S->nd);
  n++;
  
  // ratio
  if (S->a->on) {
    s[n] = (char *) dv_malloc( length * sizeof(char) );
    sprintf(s[n], "ratio=%0.2f, ", S->a->ratio);
    n++;
  }

  /*s[n] = (char *) dv_malloc( length * sizeof(char) );
  sprintf(s[n], ", ");
  n++;*/

  int slength = 0;
  int i;
  for (i=0; i<n; i++) {
    slength += strlen(s[i]);
  }
  
  const double char_width = 8;
  double x = S->vpw  - DV_STATUS_PADDING - slength * char_width;
  double y = S->vph - DV_STATUS_PADDING;
  cairo_new_path(cr);
  cairo_move_to(cr, x, y);
  for (i=n-1; i>=0; i--) {
    cairo_show_text(cr, s[i]);
  }
  cairo_restore(cr);

  for (i=0; i<n; i++)
    dv_free(s[i], length * sizeof(char));
}

void dv_draw_bbox_infotags(cairo_t *cr, dv_dag_t *G) {
  dv_llist_iterate_init(G->itl);
  dv_dag_node_t * u;
  while (u = (dv_dag_node_t *) dv_llist_iterate_next(G->itl)) {
    if (dv_is_visible(u) && !dv_is_expanding(u)
        && (!u->parent || !dv_is_shrinking(u->parent)))
      dv_draw_bbox_infotag_1(cr, u);
  }
}

void dv_draw_dvdag(cairo_t *cr, dv_dag_t *G) {

  // Draw DAG
	if (S->lt == 0)
		dv_draw_glike_dvdag(cr, G);
  else if (S->lt == 1)
		dv_draw_bbox_dvdag(cr, G);
	else if (S->lt == 2)
		dv_draw_timeline_dvdag(cr, G);
  else
    dv_check(0);
  // Draw infotags
  dv_draw_bbox_infotags(cr, G);
	
}

/*-----end of Main drawing functions-----*/
