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

static void draw_dvdag_node(cairo_t *cr, dv_dag_node_t *node) {
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
	cairo_set_source_rgba(cr, c[0], c[1], c[2], c[3]);
	cairo_arc(cr, x, y, DV_RADIUS, 0.0, 2*M_PI);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_stroke(cr);
}

static void draw_dvdag_edge(cairo_t *cr, dv_dag_node_t *u, dv_dag_node_t *v) {
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_move_to(cr, u->vl->c, u->hl->c + DV_RADIUS);
	cairo_line_to(cr, v->vl->c, v->hl->c - DV_RADIUS);
	cairo_stroke(cr);
}

static void draw_dvdag_infotag(cairo_t *cr, dv_dag_node_t *node) {
#if 0	
	double line_height = 12;
	double padding = 4;
	int n = 6; /* number of lines */
	double xx = node->vl->c + DV_RADIUS + padding;
	double yy = node->hl->c - DV_RADIUS - 2*padding - line_height * (n - 1);

	// Cover rectangle
	double width = 450.0;
	double height = n * line_height + 3*padding;
	draw_rounded_rectangle(cr,
												 node->vl->c + DV_RADIUS,
												 node->hl->c - DV_RADIUS - height,
												 width,
												 height);

	// Lines
	cairo_set_source_rgb(cr, 1.0, 1.0, 0.1);
	cairo_select_font_face(cr, "Courier", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 12);

	// Line 1
	/* TODO: adaptable string length */
	char *s = (char *) malloc( 100 * sizeof(char) );
	sprintf(s, "[%ld] %s",
					node->idx,
					NODE_KIND_NAMES[node->info->kind]);
	cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
	yy += line_height;
	  
	// Line 2
	sprintf(s, "%llu-%llu (%llu) est=%llu",
					node->info->start.t,
					node->info->end.t,
					node->info->start.t - node->info->end.t,
					node->info->est);
	cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
	yy += line_height;

	// Line 3
	sprintf(s, "T=%llu/%llu,nodes=%ld/%ld/%ld,edges=%ld/%ld/%ld/%ld",
					node->info->t_1, 
					node->info->t_inf,
					node->info->logical_node_counts[dr_dag_node_kind_create_task],
					node->info->logical_node_counts[dr_dag_node_kind_wait_tasks],
					node->info->logical_node_counts[dr_dag_node_kind_end_task],
					node->info->logical_edge_counts[dr_dag_edge_kind_end],
					node->info->logical_edge_counts[dr_dag_edge_kind_create],
					node->info->logical_edge_counts[dr_dag_edge_kind_create_cont],
					node->info->logical_edge_counts[dr_dag_edge_kind_wait_cont]);
	cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
	yy += line_height;

	// Line 4
	sprintf(s, "by worker %d on cpu %d",
					node->info->worker, 
					node->info->cpu);
	cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
	yy += line_height;

	// Line 5
	free(s);
	const char *ss = G->S->C + G->S->I[node->info->start.pos.file_idx];
	s = (char *) malloc( strlen(ss) + 10 );
	sprintf(s, "%s:%ld",
					ss,
					node->info->start.pos.line);
	cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
	yy += line_height;

	// Line 6
	free(s);
	ss = G->S->C + G->S->I[node->info->end.pos.file_idx];
	s = (char *) malloc( strlen(ss) + 10 );	
	sprintf(s, "%s:%ld",
					ss,
					node->info->end.pos.line);
	cairo_move_to(cr, xx, yy);
  cairo_show_text(cr, s);
	yy += line_height;
	
	free(s);
#endif
}

void draw_dvdag(cairo_t *cr, dv_dag_t *G) {
	cairo_set_line_width(cr, 2.0);
	int i;
	// Draw nodes
	for (i=0; i<G->n; i++) {
		draw_dvdag_node(cr, &G->T[i]);
	}
#if 0
	// Draw edges
	for (i=0; i<G->n; i++) {
		dv_dag_node_t *node = G->T + i;
		int kind = node->info->kind;
		if (kind == dr_dag_node_kind_create_task) {
			draw_dvdag_edge(cr, node, node->el);
			draw_dvdag_edge(cr, node, node->er);
		} else {
			if (node->ej)
				draw_dvdag_edge(cr, node, node->ej);
		}
	}
	// Draw info tags
	dv_llist_cell_t *l = G->itl->head;
	while (l) {
		if (l->item) {
			draw_dvdag_infotag(cr, (dv_dag_node_t *) l->item);
		}
		l = l->next;
	}
#endif	
}

/*-----------------end of DAG drawing functions-----------*/

