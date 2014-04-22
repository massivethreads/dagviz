#include "dagviz.h"


/*-----------------Drawing functions-----------*/

static void draw_text(cairo_t *cr) {
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

static void draw_rounded_rectangle(cairo_t *cr, double x, double y, double width, double height)
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

static void lookup_color(int v, double *r, double *g, double *b, double *a) {
	GdkRGBA color;
	gdk_rgba_parse(&color, DV_COLORS[(v + NUM_COLORS) % NUM_COLORS]);
	*r = color.red;
	*g = color.green;
	*b = color.blue;
	*a = color.alpha;
	/*	*r = 0.44;
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

static double get_alpha_fading_out() {
	double ratio = S->a->ratio;
	double ret;
	//ret = (1.0 - ratio) * 0.75;
	ret = 1.0 - ratio * ratio;
	return ret;
}

static double get_alpha_fading_in() {
	double ratio = S->a->ratio;
	double ret;
	//ret = ratio * 1.5;
	ret = ratio * ratio;
	return ret;
}

static void draw_dvdag_node_1(cairo_t *cr, dv_dag_node_t *node) {
	// Node color
	double x = node->vl->c;
	double y = node->c;
	double c[4];
	int v = 0;
	switch (S->nc) {
	case 0:
		v = node->pi->info.worker; break;
	case 1:
		v = node->pi->info.cpu; break;
	case 2:
		v = node->pi->info.kind; break;
	default:
		v = node->pi->info.worker;
	}
	lookup_color(v, c, c+1, c+2, c+3);
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
				alpha = get_alpha_fading_out();
				margin = get_alpha_fading_in();
			} else {
				// Fading in
				alpha = get_alpha_fading_in();
				margin = get_alpha_fading_out();
			}
			// Large-sized box
			margin *= DV_UNION_NODE_MARGIN;
			xx = x - node->lc * DV_HDIS - DV_RADIUS - margin;
			yy = y - DV_RADIUS - margin;
			dv_dag_node_t * hd = (dv_dag_node_t *) node->heads->top->item;
			h = hd->dc * DV_VDIS + 2 * (DV_RADIUS + margin);
			w = (node->lc + node->rc) * DV_HDIS + 2 * (DV_RADIUS + margin);
			
		} else {
			
			// Normal-sized box
			xx = x - DV_RADIUS;
			yy = y - DV_RADIUS;
			w = 2 * DV_RADIUS;
			h = 2 * DV_RADIUS;
			if (node->lv > S->a->new_sel) {
				// Fading out
				alpha = get_alpha_fading_out();
			} else if (node->lv > S->sel) {
				// Fading in
				alpha = get_alpha_fading_in();
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
		if (node->lv > S->a->new_sel) {
			// Fading out
			alpha = get_alpha_fading_out();
		} else if (node->lv > S->sel) {
			// Fading in
			alpha = get_alpha_fading_in();
		}
		
	}
	
	// Draw node
	cairo_set_source_rgba(cr, c[0], c[1], c[2], c[3] * alpha);
	cairo_fill_preserve(cr);
	cairo_set_source_rgba(cr, 0.0, 0.0, 0.0, alpha);
	cairo_stroke(cr);
	cairo_restore(cr);
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
		dv_llist_iterate_init(u->heads);
		while (v = (dv_dag_node_t *) dv_llist_iterate_next(u->heads)) {
			draw_dvdag_node_r(cr, v);
		}
	}
}

static void draw_dvdag_edge_1(cairo_t *cr, dv_dag_node_t *u, dv_dag_node_t *v) {
	if (u->c + DV_RADIUS > v->c - DV_RADIUS)
		return;
	double alpha = 1.0;
	if (u->lv > S->a->new_sel && v->lv > S->a->new_sel)
		alpha = get_alpha_fading_out();
	else if (u->lv > S->sel && v->lv > S->sel
					 && u->lv <= S->a->new_sel
					 && v->lv <= S->a->new_sel)
		alpha = get_alpha_fading_in();
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
		dv_check(u->heads->top);
		u = (dv_dag_node_t *) u->heads->top->item;
		dv_check(u);
	}
	return u;
}

static dv_dag_node_t * dv_dag_node_get_last(dv_dag_node_t *u) {
	dv_check(u);
	while (dv_node_flag_check(u->f, DV_NODE_FLAG_UNION)
				 && (!dv_node_flag_check(u->f, DV_NODE_FLAG_SHRINKED)
						 || dv_node_flag_check(u->f, DV_NODE_FLAG_EXPANDING))) {
		dv_check(u->heads->top);
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
				|| (dv_is_shrinked(u)	&& !dv_is_expanding(u))) {
			
			if (!dv_is_union(v)
					|| (dv_is_shrinked(v)	&& !dv_is_expanding(v))) {
				
				draw_dvdag_edge_1(cr, u, v);
				
			} else {
				
				dv_llist_iterate_init(v->heads);
				while (v_head = (dv_dag_node_t *) dv_llist_iterate_next(v->heads)) {
					dv_dag_node_t * v_first = dv_dag_node_get_first(v_head);
					draw_dvdag_edge_1(cr, u, v_first);
				}
				
			}
			
		} else {
			
			dv_llist_iterate_init(u->tails);
			while (u_tail = (dv_dag_node_t *) dv_llist_iterate_next(u->tails)) {
				dv_dag_node_t * u_last = dv_dag_node_get_last(u_tail);
				
				if (!dv_is_union(v)
						|| (dv_is_shrinked(v) && !dv_is_expanding(v))) {
					draw_dvdag_edge_1(cr, u_last, v);
				} else {
					
					dv_llist_iterate_init(v->heads);
					while (v_head = (dv_dag_node_t *) dv_llist_iterate_next(v->heads)) {
						dv_dag_node_t * v_first = dv_dag_node_get_first(v_head);
						draw_dvdag_edge_1(cr, u_last, v_first);
					}
					
				}
				
			}
			
		}
		draw_dvdag_edge_r(cr, v);
		
	}

	// Call head
	if (dv_is_union(u)
			&& ( !dv_is_shrinked(u) || dv_is_expanding(u) )) {
		dv_llist_iterate_init(u->heads);
		while (v = (dv_dag_node_t *) dv_llist_iterate_next(u->heads))
			draw_dvdag_edge_r(cr, v);
	}
	
}

static void draw_dvdag_infotag(cairo_t *cr, dv_dag_node_t *node) {
	double line_height = 12;
	double padding = 4;
	int n = 6; /* number of lines */
	double xx = node->vl->c + DV_RADIUS + padding;
	double yy = node->c - DV_RADIUS - 2*padding - line_height * (n - 1);

	// Cover rectangle
	double width = 450.0;
	double height = n * line_height + 3*padding;
	draw_rounded_rectangle(cr,
												 node->vl->c + DV_RADIUS,
												 node->c - DV_RADIUS - height,
												 width,
												 height);

	// Lines
	cairo_set_source_rgb(cr, 1.0, 1.0, 0.1);
	cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);

	// Line 1
	/* TODO: adaptable string length */
	char *s = (char *) dv_malloc( DV_STRING_LENGTH * sizeof(char) );
	sprintf(s, "[%ld] %s lv=%d f=%d%d%d%d dc=%0.1lf c=%0.1lf",
					node - G->T,
					NODE_KIND_NAMES[node->pi->info.kind],
					node->lv,
					dv_is_union(node),
					dv_is_shrinked(node),
					dv_is_expanding(node),
					dv_is_shrinking(node),
					node->dc,
					node->c);
	cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
	yy += line_height;
	  
	// Line 2
	sprintf(s, "%llu-%llu (%llu) est=%llu",
					node->pi->info.start.t,
					node->pi->info.end.t,
					node->pi->info.start.t - node->pi->info.end.t,
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

void dv_draw_dvdag(cairo_t *cr, dv_dag_t *G) {
	cairo_set_line_width(cr, 2.0);
	int i;
	// Draw nodes
	draw_dvdag_node_r(cr, G->rt);
	// Draw edges
	draw_dvdag_edge_r(cr, G->rt);
	// Draw info tags
	dv_llist_iterate_init(G->itl);
	dv_dag_node_t * u;
	while (u = (dv_dag_node_t *) dv_llist_iterate_next(G->itl)) {
		draw_dvdag_infotag(cr, u);
	}
}

void dv_draw_status(cairo_t *cr) {
	cairo_save(cr);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 14);

	char *s[10];
	int n = 0;
	int length = 50;
	
	s[n] = (char *) dv_malloc( length * sizeof(char) );
	sprintf(s[n], "SEL=%d", S->sel);
	n++;

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
	double x = S->vpw	- DV_STATUS_PADDING - slength * char_width;
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

/*-----------------end of DAG drawing functions-----------*/

