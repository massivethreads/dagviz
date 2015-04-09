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
  cairo_stroke(cr);
  cairo_restore(cr);
}

void
dv_draw_path_isosceles_triangle(cairo_t * cr, double x, double y, double w, double h) {
  cairo_save(cr);
  cairo_move_to(cr, x + w / 2.0, y);
  cairo_rel_line_to(cr, - w / 2.0, h);
  cairo_rel_line_to(cr, w, 0.0);
  cairo_close_path(cr);
  cairo_restore(cr);
}

void
dv_draw_path_isosceles_triangle_upside_down(cairo_t * cr, double x, double y, double w, double h) {
  cairo_save(cr);
  cairo_move_to(cr, x, y);
  cairo_rel_line_to(cr, w / 2.0, h);
  cairo_rel_line_to(cr, w / 2.0, - h);
  cairo_close_path(cr);
  cairo_restore(cr);
}

void
dv_draw_path_rectangle(cairo_t * cr, double x, double y, double w, double h) {
  cairo_save(cr);
  cairo_move_to(cr, x, y);
  cairo_rel_line_to(cr, 0.0, h);
  cairo_rel_line_to(cr, w, 0.0);
  cairo_rel_line_to(cr, 0.0, - h);
  cairo_close_path(cr);
  cairo_restore(cr);  
}

void
dv_draw_path_rounded_rectangle(cairo_t * cr, double x, double y, double w, double h) {
  cairo_save(cr);

  double r = h / 8.0;     /* corner radius */
  double d = M_PI / 180.0; /* degrees */
  
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + r    , y + r    , r, -180 * d, -90 * d);
  cairo_arc(cr, x + w - r, y + r    , r,  -90 * d,   0 * d);
  cairo_arc(cr, x + w - r, y + h - r, r,    0 * d,  90 * d);
  cairo_arc(cr, x + r    , y + h - r, r,   90 * d, 180 * d);
  cairo_close_path(cr);
  
  cairo_restore(cr);  
}

void
dv_draw_path_circle(cairo_t * cr, double x, double y, double w) {
  cairo_save(cr);

  double r = w / 2.0;
  cairo_new_sub_path(cr);
  cairo_arc(cr, x + r, y + r, r, 0.0, 2 * M_PI);
  cairo_close_path(cr);
    
  cairo_restore(cr);
}

void
dv_lookup_color_value(int v, double * r, double * g, double * b, double * a) {
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

void
dv_lookup_color(dr_pi_dag_node * pi, int nc, double * r, double * g, double * b, double * a) {
  int v = dv_pidag_node_lookup_value(pi, nc);
  dv_lookup_color_value(v, r, g, b, a);
}

cairo_pattern_t *
dv_create_color_linear_pattern(int * stops, int n, double w, double alpha) {
  if (n == 0 || n == 1)
    return NULL;
  cairo_pattern_t * pat = cairo_pattern_create_linear(0.0, 0.0, w, 0.0);
  double step = 1.0 / (n - 1);
  double r, g, b, a;
  int i;
  for (i = 0; i < n; i++) {
    dv_lookup_color_value(stops[i], &r, &g, &b, &a);
    a *= alpha;
    cairo_pattern_add_color_stop_rgba(pat, i * step, r, g, b, a);
  }
  return pat;
}

cairo_pattern_t *
dv_get_color_linear_pattern(dv_view_t * V, double w, double alpha, long long remark) {
  cairo_pattern_t * pat = cairo_pattern_create_linear(0.0, 0.0, w, 0.0);

  int a[DV_MAX_NUM_REMARKS];
  int n = 0;
  long v = 0;
  int i;
  for (i = 0; i < V->D->nr; i++) {
    if (dv_get_bit(remark, i)) {
      v = V->D->ar[i];
      a[n++] = v;
    }
  }

  double step;
  GdkRGBA color;
  if (n > 0) {
    /* Remark colors */
    step = 1.0 / (n - 1 + 1);
    for (i=0; i<n; i++) {
      v = a[i];
      dv_lookup_color_value(v, &color.red, &color.green, &color.blue, &color.alpha);
      cairo_pattern_add_color_stop_rgba(pat, i * step, color.red, color.green, color.blue, color.alpha);
    }
    gdk_rgba_parse(&color, "white");
    cairo_pattern_add_color_stop_rgba(pat, n * step, color.red, color.green, color.blue, color.alpha);
  } else {
    /* Mixed */
    char ** stops = (char **) DV_LINEAR_PATTERN_STOPS;
    n = DV_LINEAR_PATTERN_STOPS_NUM;
    step = 1.0 / (n - 1);
    for (i = 0; i < n; i++) {
      gdk_rgba_parse(&color, stops[i]);
      cairo_pattern_add_color_stop_rgba(pat, i * step, color.red, color.green, color.blue, color.alpha * alpha);
    }
  }
  
  return pat;
}

cairo_pattern_t *
dv_get_color_radial_pattern(double radius, double alpha) {
  char ** stops = (char **) DV_RADIAL_PATTERN_STOPS;
  int n = DV_RADIAL_PATTERN_STOPS_NUM;
  cairo_pattern_t * pat = cairo_pattern_create_radial(0.0, 0.0, 0.0, 0.0, 0.0, radius);
  double step = 1.0 / (n - 1);
  double r, g, b, a;
  int i;
  for (i = 0; i < n; i++) {
    GdkRGBA color;
    gdk_rgba_parse(&color, stops[i]);
    cairo_pattern_add_color_stop_rgba(pat, i * step, color.red, color.green, color.blue, color.alpha * alpha);
  }
  return pat;
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

void
dv_view_draw_edge_1(dv_view_t * V, cairo_t * cr, dv_dag_node_t * u, dv_dag_node_t * v) {
  dv_view_status_t * S = V->S;
  if (S->et == 0)
    return;
  
  /* Get coordinates */
  double x1, y1, x2, y2;
  dv_node_coordinate_t * uc = &u->c[S->lt];
  dv_node_coordinate_t * vc = &v->c[S->lt];
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
  dr_pi_dag_node * pi;
  //cairo_save(cr);
  //cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
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
    pi = dv_pidag_get_node_by_dag_node(V->D->P, u);
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
  //cairo_stroke(cr);
  //cairo_restore(cr);
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
  /*
  sprintf(s, "P%ld-D%ld-V%ld", D->P - CS->P, D - CS->D, V - CS->V);
  x -= strlen(s) * char_width;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);
  */
  
  // Depth
  //sprintf(s, "d=%d/%d/%d, ", D->cur_d_ex, D->cur_d, D->dmax);
  sprintf(s, "depth:%d/%d", D->cur_d, D->dmax);
  x -= strlen(s) * char_width;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);

  // Node pool
  sprintf(s, "pool:%ld/%ld(%ldMB), ", CS->pool->n, CS->pool->N, CS->pool->sz / (1 << 20));
  x -= strlen(s) * char_width;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);
  
  // Nodes drawn
  sprintf(s, "nodes:%ld/%ld/%ld, ", S->nd, S->ndh, D->P->n);
  x -= strlen(s) * char_width;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);
  
  // Zoom ratios
  /*
  sprintf(s, "zr=(%lf,%lf), ", S->zoom_ratio_x, S->zoom_ratio_y);
  x -= strlen(s) * char_width;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);
  */
  
  // Radix, radius
  /*
  double radix = dv_dag_get_radix(D);
  sprintf(s, "r=(%lf,%lf), ", radix, D->radius);
  x -= strlen(s) * char_width;
  cairo_move_to(cr, x, y);
  cairo_show_text(cr, s);
  */
  
  // Nodes animating
  /*
  if (S->a->on) {
    sprintf(s, "na=%d, ", dv_llist_size(S->a->movings));
    x -= strlen(s) * char_width;
    cairo_move_to(cr, x, y);
    cairo_show_text(cr, s);
  }
  */
  
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
  double line_height = 13;
  double padding = 3;
  double upper_padding = padding;
  double lower_padding = 2.5 * padding;
  int n = 6; /* number of lines */
  
  // Get coordinates
  double xx, yy;
  dv_node_coordinate_t * c = &node->c[S->lt];
  xx = c->x + c->rw + 2 * padding;
  yy = c->y - 2 * padding;
  //cairo_matrix_transform_point(mt, &xx, &yy);
  yy -= line_height * (n - 1);
  double width = 450.0;
  double height = n * line_height + upper_padding + lower_padding;
  
  double cairo_bound_left = dv_view_cairo_coordinate_bound_left(V);
  double cairo_bound_right = dv_view_cairo_coordinate_bound_right(V);
  double cairo_bound_up = dv_view_cairo_coordinate_bound_up(V);
  double cairo_bound_down = dv_view_cairo_coordinate_bound_down(V);
  /*
  if (xx - padding < cairo_bound_left || xx - padding + width >= cairo_bound_right
      || yy -line_height - padding < cairo_bound_up || yy -line_height - padding + height >= cairo_bound_down) {
    cairo_restore(cr);
    return;
  }
  */
  /* TODO: adhoc limit numbers of 100 & 200 */
  if (V->S->zoom_ratio_x > 100 || V->S->zoom_ratio_y > 200) {
    cairo_restore(cr);
    return;
  }

  // Cover rectangle
  dv_draw_rounded_rectangle(cr,
                            xx - padding,
                            yy - line_height - upper_padding,
                            width,
                            height);

  // Lines
  cairo_set_source_rgb(cr, 1.0, 1.0, 0.1);
  cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 13);

  // Line 1
  /* TODO: adaptable string length */
  dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, node);
  char * s = (char *) dv_malloc( DV_STRING_LENGTH * sizeof(char) );
  sprintf(s, "[%ld] %s d=%d f=%d%d%d%d%d%d n=%ld/%ld/%ld",
          pi - D->P->T,
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
  sprintf(s, "%llu-%llu (%llu) ready=%llu est=%llu",
          pi->info.start.t,
          pi->info.end.t,
          pi->info.end.t - pi->info.start.t,
	  pi->info.first_ready_t,
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
  sprintf(s, "by worker %d on cpu %d (r=%lld",
          pi->info.worker, 
          pi->info.cpu,
          node->r);
  cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
  char s2[V->D->nr * 2 + 1];
  char s3[5];
  s2[0] = 0;
  long long r = node->r;
  long v;
  int i = 0;
  while (i < V->D->nr) {
    if (dv_get_bit(r, i)) {
      v = V->D->ar[i];
      if (strlen(s2) == 0)
        sprintf(s3, " {%ld", v);
      else
        sprintf(s3, ",%ld", v);
      strcat(s2, s3);
    }
    i++;
  }
  if (strlen(s2) > 0)
    strcat(s2, "}");
  strcat(s2, ")");
  cairo_show_text(cr, s2);
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
    dv_view_draw_dag(V, cr);
    break;
  case 1:
    dv_view_draw_dagbox(V, cr);
    break;
  case 2:
    dv_view_draw_timeline(V, cr);
    break;
  case 3:
    dv_view_draw_timeline2(V, cr);
    break;
  case 4: {
    double start = dv_get_time();
    dv_view_draw_paraprof(V, cr);
    double end = dv_get_time();
    //fprintf(stderr, "draw time: %lf\n", end - start);
    break;
  }
  default:
    dv_check(0);
  }
  // Draw infotags
  dv_view_draw_infotags(V, cr, NULL);
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

void
dv_view_draw_legend(dv_view_t * V, cairo_t * cr) {
  dv_dag_t * D = V->D;
  dv_view_status_t * S = V->S;
  
  switch (S->lt) {
  case 0:
    dv_view_draw_legend_dag(V, cr);
    break;
  case 1:
    //dv_view_draw_legend_dagbox(V, cr);
    break;
  case 2:
    //dv_view_draw_legend_timeline(V, cr);
    break;
  case 3:
    //dv_view_draw_legend_timeline2(V, cr);
    break;
  case 4: {
    //dv_view_draw_legend_paraprof(V, cr);
    break;
  }
  default:
    dv_check(0);
  }
}

/*-----end of Main drawing functions-----*/
