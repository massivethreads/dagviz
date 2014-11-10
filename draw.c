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
  cairo_save(cr);
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
  cairo_set_line_width(cr, 0.5);
  cairo_stroke(cr);
  cairo_restore(cr);
}

static int dv_get_color_pool_index(int t, int v0, int v1, int v2, int v3) {
  int n = CS->CP_sizes[t];
  int i;
  for (i=0; i<n; i++) {
    if (CS->CP[t][i][0] == v0 && CS->CP[t][i][1] == v1
        && CS->CP[t][i][2] == v2 && CS->CP[t][i][3] == v3)
      return i;
  }
  dv_check(n < DV_COLOR_POOL_SIZE);
  CS->CP[t][n][0] = v0;
  CS->CP[t][n][1] = v1;
  CS->CP[t][n][2] = v2;
  CS->CP[t][n][3] = v3;
  CS->CP_sizes[t]++;
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

void dv_lookup_color(dr_pi_dag_node *pi, int nc, double *r, double *g, double *b, double *a) {
  int v = 0;
  dv_check(nc < DV_NUM_COLOR_POOLS);
  switch (nc) {
  case 0:
    v = dv_get_color_pool_index(nc, 0, 0, 0, pi->info.worker);
    break;
  case 1:
    v = dv_get_color_pool_index(nc, 0, 0, 0, pi->info.cpu);
    break;
  case 2:
    v = dv_get_color_pool_index(nc, 0, 0, 0, pi->info.kind);
    break;
  case 3:
    v = dv_get_color_pool_index(nc, 0, 0, pi->info.start.pos.file_idx, pi->info.start.pos.line);
    break;
  case 4:
    v = dv_get_color_pool_index(nc, 0, 0, pi->info.end.pos.file_idx, pi->info.end.pos.line);
    break;
  case 5:
    v = dv_get_color_pool_index(nc, pi->info.start.pos.file_idx, pi->info.start.pos.line, pi->info.end.pos.file_idx, pi->info.end.pos.line);
    break;
  default:
    dv_check(0);
    break;
  }  
  dv_lookup_color_value(v, r, g, b, a);
}

double dv_view_get_alpha_fading_out(dv_view_t *V, dv_dag_node_t *node) {
  double ratio = (dv_get_time() - node->started) / V->S->a->duration;
  double ret;
  //ret = (1.0 - ratio) * 0.75;
  ret = 1.0 - ratio * ratio;
  return ret;
}

double dv_view_get_alpha_fading_in(dv_view_t *V, dv_dag_node_t *node) {
  double ratio = (dv_get_time() - node->started) / V->S->a->duration;
  double ret;
  //ret = ratio * 1.5;
  ret = ratio * ratio;
  return ret;
}

void dv_view_draw_edge_1(dv_view_t *V, cairo_t *cr, dv_dag_node_t *u, dv_dag_node_t *v) {
  dv_view_status_t *S = V->S;
  if (S->et == 0)
    return;
  
  /* Get coordinates */
  double x1, y1, x2, y2;
  dv_node_coordinate_t *uc = &u->c[S->lt];
  dv_node_coordinate_t *vc = &v->c[S->lt];
  x1 = uc->x;
  y1 = uc->y + uc->dw;
  x2 = vc->x;
  y2 = vc->y;
  if (y1 > y2)
    return;
  
  /* Get alpha */
  double alpha = 1.0;
  if ((!u->parent || dv_is_shrinking(u->parent))
      //&& (!v->parent || dv_is_shrinking(v->parent))
      && (u->parent == v->parent))
    alpha = dv_view_get_alpha_fading_out(V, u->parent);
  else if ((!u->parent || dv_is_expanding(u->parent))
           //&& (!v->parent || dv_is_expanding(v->parent))
           && (u->parent == v->parent))
    alpha = dv_view_get_alpha_fading_in(V, u->parent);

  /* Draw edge */
  dr_pi_dag_node *pi;
  cairo_save(cr);
  cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
  cairo_move_to(cr, x1, y1);
  // edge affix
  double a = S->edge_affix;
  if (a > (y2 - y1) / 2)
    a = (y2 - y1) / 2;
  if (a != 0) {
    cairo_line_to(cr, x1, y1 + a);
    y1 += a;
    y2 -= a;
  }
  /* edge type */
  switch (S->et) {
  case 0:
    // no edge
    break;
  case 1:
    // straight
    cairo_line_to(cr, x2, y2);
    break;
  case 2:
    // down
    cairo_line_to(cr, x1, y2);
    cairo_line_to(cr, x2, y2);
    break;
  case 3:
    // winding
    pi = dv_pidag_get_node(V->D->P, u);
    if (pi->info.kind == dr_dag_node_kind_create_task)
      cairo_line_to(cr, x2, y1);
    else
      cairo_line_to(cr, x1, y2);
    cairo_line_to(cr, x2, y2);
    break;
  default:
    dv_check(0);
  }    
  // edge affix
  if (a != 0) {
    cairo_line_to(cr, x2, y2 + a);
  }    
  cairo_stroke(cr);
  cairo_restore(cr);
}

/*-----------------end of Common Drawing functions-----------*/


/*-----Main drawing functions-----*/

void
dv_view_draw_status(dv_view_t * V, cairo_t * cr, int count) {
  dv_dag_t * D = V->D;
  dv_view_status_t * S = V->S;
  cairo_save(cr);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);

  char s[50];
  const double char_width = 7;
  const double line_height = 15;
  double x = S->vpw  - DV_STATUS_PADDING;
  double y = S->vph - DV_STATUS_PADDING - count * line_height;
  cairo_new_path(cr);

  // Identifier
  sprintf(s, "P%ld-D%ld-V%ld", D->P - CS->P, D - CS->D, V - CS->V);
  x -= strlen(s) * char_width;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);
  
  // Depth
  sprintf(s, "d=%d-%d/%d, ", D->cur_d_ex, D->cur_d, D->dmax);
  x -= strlen(s) * char_width;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);

  // Node pool
  sprintf(s, "np=%d/%d, ", D->Tn, D->Tsz);
  x -= strlen(s) * char_width;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);
  
  // Nodes drawn
  sprintf(s, "n=%ld/%ld, ", S->nd, S->ndh);
  x -= strlen(s) * char_width;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);
  
  // Zoom ratios
  sprintf(s, "zr=(%lf,%lf), ", S->zoom_ratio_x, S->zoom_ratio_y);
  x -= strlen(s) * char_width;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);
  
  // Radix, radius
  double radix = dv_dag_get_radix(D);
  sprintf(s, "r=(%lf,%lf), ", radix, D->radius);
  x -= strlen(s) * char_width;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);
  
  // Nodes animating
  if (S->a->on) {
    sprintf(s, "na=%d, ", dv_llist_size(S->a->movings));
    x -= strlen(s) * char_width;
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, s);
  }
  
  // ratio
  /*
  if (S->a->on) {
    s[n] = (char *) dv_malloc( length * sizeof(char) );
    sprintf(s[n], "ratio=%0.2f, ", S->a->ratio);
    n++;
  }
  */

  /*s[n] = (char *) dv_malloc( length * sizeof(char) );
  sprintf(s[n], ", ");
  n++;*/

  cairo_restore(cr);
}

void
dv_view_draw_infotag_1(dv_view_t * V, cairo_t * cr, cairo_matrix_t * mt, dv_dag_node_t * node) {
  cairo_save(cr);
  dv_dag_t * D = V->D;
  dv_view_status_t * S = V->S;
  double line_height = 12;
  double padding = 4;
  int n = 6; /* number of lines */
  
  // Get coordinates
  double xx, yy;
  dv_node_coordinate_t * c = &node->c[S->lt];
  xx = c->x + c->rw + 2 * padding;
  yy = c->y - 2 * padding;
  //cairo_matrix_transform_point(mt, &xx, &yy);
  yy -= line_height * (n - 1);

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
  dr_pi_dag_node * pi = dv_pidag_get_node(D->P, node);
  char * s = (char *) dv_malloc( DV_STRING_LENGTH * sizeof(char) );
  sprintf(s, "[%ld][%ld] %s d=%d f=%d%d%d%d%d%d n=%ld/%ld/%ld",
          pi - D->P->T,
          node - D->T,
          dv_get_node_kind_name(pi->info.kind),
          node->d,
          dv_is_set(node),
          dv_is_union(node),
          dv_is_inner_loaded(node),
          dv_is_shrinked(node),
          dv_is_expanding(node),
          dv_is_shrinking(node),
          pi->info.cur_node_count,
          pi->info.min_node_count,
          pi->info.n_child_create_tasks);
  cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
  yy += line_height;
    
  // Line 2
  sprintf(s, "%llu-%llu (%llu) est=%llu",
          pi->info.start.t,
          pi->info.end.t,
          pi->info.end.t - pi->info.start.t,
          pi->info.est);
  cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
  yy += line_height;

  // Line 3
  sprintf(s, "T=%llu/%llu,nodes=%ld/%ld/%ld/%ld,edges=%ld/%ld/%ld/%ld/%ld",
          pi->info.t_1, 
          pi->info.t_inf,
          pi->info.logical_node_counts[dr_dag_node_kind_create_task],
          pi->info.logical_node_counts[dr_dag_node_kind_wait_tasks],
          pi->info.logical_node_counts[dr_dag_node_kind_other],
          pi->info.logical_node_counts[dr_dag_node_kind_end_task],
          pi->info.logical_edge_counts[dr_dag_edge_kind_end],
          pi->info.logical_edge_counts[dr_dag_edge_kind_create],
          pi->info.logical_edge_counts[dr_dag_edge_kind_create_cont],
          pi->info.logical_edge_counts[dr_dag_edge_kind_wait_cont],
          pi->info.logical_edge_counts[dr_dag_edge_kind_other_cont]);
  cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
  yy += line_height;

  // Line 4
  sprintf(s, "by worker %d on cpu %d",
          pi->info.worker, 
          pi->info.cpu);
  cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
  yy += line_height;

  // Line 5
  dv_free(s, DV_STRING_LENGTH * sizeof(char));
  const char * ss = D->P->S->C + D->P->S->I[pi->info.start.pos.file_idx];
  s = (char *) dv_malloc( strlen(ss) + 10 );
  sprintf(s, "%s:%ld",
          ss,
          pi->info.start.pos.line);
  cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
  yy += line_height;

  // Line 6
  dv_free(s, strlen(ss) + 10);
  ss = D->P->S->C + D->P->S->I[pi->info.end.pos.file_idx];
  s = (char *) dv_malloc( strlen(ss) + 10 );  
  sprintf(s, "%s:%ld",
          ss,
          pi->info.end.pos.line);
  cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
  yy += line_height;
  
  dv_free(s, strlen(ss) + 10);
  cairo_restore(cr);
}

void
dv_view_draw_infotags(dv_view_t * V, cairo_t * cr, cairo_matrix_t * mt) {
  dv_dag_node_t * node = NULL;
  while (node = (dv_dag_node_t *) dv_llist_pop(V->D->itl)) {
    dv_view_draw_infotag_1(V, cr, mt, node);
  }  
}

void
dv_view_draw(dv_view_t * V, cairo_t * cr) {
  dv_dag_t * D = V->D;
  dv_view_status_t * S = V->S;
  
  // Draw DAG
  S->nd = 0;
  S->ndh = 0;
  D->cur_d = 0;
  D->cur_d_ex = D->dmax;
  switch (S->lt) {
  case 0:
    dv_view_draw_glike(V, cr);
    break;
  case 1:
    dv_view_draw_bbox(V, cr);
    break;
  case 2:
    dv_view_draw_timeline(V, cr);
    break;
  case 3:
    dv_view_draw_timeline2(V, cr);
    break;
  default:
    dv_check(0);
  }
  // Draw infotags
  //dv_view_draw_infotags(V, cr);  
}

void
dv_viewport_draw_label(dv_viewport_t * VP, cairo_t * cr) {
  cairo_save(cr);
  cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);

  char s[20];
  //const double char_width = 8;
  //const double line_height = 18;
  double x = DV_STATUS_PADDING;
  double y = VP->vph - DV_STATUS_PADDING;
  cairo_new_path(cr);

  // Identifier
  sprintf(s, "VIEWPORT %ld", VP - CS->VP);
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);  

  cairo_restore(cr);
}

/*-----end of Main drawing functions-----*/
