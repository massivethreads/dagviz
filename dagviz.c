#include "dagviz.h"

static dv_graph_t G[1];
static GtkWidget *window;
static GtkWidget *darea;

/*-----------------Read .dag format-----------------*/

static void * read_pi_dag_node(void * dp, dr_pi_dag_node * des) {
	dr_pi_dag_node * np = (dr_pi_dag_node *) dp;
	*des = *np;
	return ++np;
}

static void * read_pi_dag_edge(void * dp, dr_pi_dag_edge * des) {
	dr_pi_dag_edge * ep = (dr_pi_dag_edge *) dp;
	*des = *ep;
	return ++ep;
}

static void * read_pi_string_table(void * dp, dr_pi_string_table * des) {
	dr_pi_string_table * stp = (dr_pi_string_table *) dp;
	*des = *stp;
	des->I = (long *) ++stp;
	des->C = (const char *) (des->I + des->n);
	return dp + des->sz;
}

static void print_pi_dag_edge(dr_pi_dag_edge * de, int i) {
	printf(
				 "DAG edge %d:\n"
				 "  kind: %d (%s)\n"
				 "  u: %ld\n"
				 "  v: %ld\n",
				 i,
				 de->kind,
				 EDGE_KIND_NAMES[de->kind],
				 de->u,
				 de->v
				 );
}

static void print_pi_string_table(dr_pi_string_table * stp, int i) {
	printf(
				 "String table %d:\n"
				 "  n: %ld\n"
				 "  sz: %ld\n",
				 i,
				 stp->n,
				 stp->sz
				 );
	printf("  I:");
	int j;
	for (j=0; j<stp->n; j++) {
		printf(" %ld", stp->I[j]);
	}
	printf("\n");
	printf("  C:");
	for (j=0; j<stp->n; j++) {
		printf(" \"%s\"\n    ", stp->C + stp->I[j]);
	}
	printf("\n");
}

static void print_pi_dag_node(dr_pi_dag_node * dn, int i) {
	printf(
		"DAG node %d:\n"
		"  info.start: %llu, %p, %ld, %ld\n"
		"  info.end: %llu, %p, %ld, %ld\n"
		//"  info.est: %llu\n"
		"  info.kind: %d (%s)\n"
		"  info.last_node_kind: %d (%s)\n"
		"  info.node_counts: %ld/%ld/%ld\n"
		"  info.edge_counts: %ld/%ld/%ld/%ld\n"
		"  info.n_child_create_tasks: %ld\n"
		"  info.worker/cpu: %d, %d\n"
		"  edges_begin/end: %ld, %ld\n",
		i,
		(unsigned long long) dn->info.start.t,
		dn->info.start.pos.file,
		dn->info.start.pos.file_idx,
		dn->info.start.pos.line,
		(unsigned long long) dn->info.end.t,
		dn->info.end.pos.file,
		dn->info.end.pos.file_idx,
		dn->info.end.pos.line,
		//(unsigned long long) dn->info.est,
		dn->info.kind,
		NODE_KIND_NAMES[dn->info.kind],
		dn->info.last_node_kind,
		NODE_KIND_NAMES[dn->info.last_node_kind],
		dn->info.node_counts[0],
		dn->info.node_counts[1],
		dn->info.node_counts[2],
		dn->info.edge_counts[0],
		dn->info.edge_counts[1],
		dn->info.edge_counts[2],
		dn->info.edge_counts[3],
		dn->info.n_child_create_tasks,
		dn->info.worker,
		dn->info.cpu,
		dn->edges_begin,
		dn->edges_end
	);
	if (dn->info.kind == dr_dag_node_kind_create_task) {
		printf("  child_offset: %ld\n",
					 dn->child_offset
					 );
	} else if (dn->info.kind == dr_dag_node_kind_section
						 || dn->info.kind == dr_dag_node_kind_task) {
		printf("  subgraphs_begin_offset: %ld\n"
					 "  subgraphs_end_offset: %ld\n",
					 dn->subgraphs_begin_offset,
					 dn->subgraphs_end_offset);
	}
}

static void read_dag_file_to_stdout(char * filename) {
	int err;
	int fd;
	struct stat statbuf;
	void *dp, *dp_head;
	
	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open: %d\n", errno);
		exit(1);
	}
	err = fstat(fd, &statbuf);
	if (err < 0) {
		fprintf(stderr, "fstat: %d\n", errno);
		exit(1);
	}
	printf("File state:\n"
				 "  st_size = %d\n",
				 (int) statbuf.st_size);
	
	dp = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (!dp) {
		fprintf(stderr, "mmap: error\n");
		exit(1);
	}

	dp_head = dp;
	long n, m, nw;
	long * ldp = (long *) dp;
	n = *ldp;
	ldp++;
	m = *ldp;
	ldp++;
	nw = *ldp;
	ldp++;
	printf("n = %ld\nm = %ld\nnw = %ld\n", n, m, nw);

	dp = ldp;
	dr_pi_dag_node dn;
	int i;
	for (i=0; i<n; i++) {
		dp = read_pi_dag_node(dp, &dn);
		print_pi_dag_node(&dn, i);
	}
	dr_pi_dag_edge de;
	for (i=0; i<m; i++) {
		dp = read_pi_dag_edge(dp, &de);
		print_pi_dag_edge(&de, i);
	}
	dr_pi_string_table S;
	dp = read_pi_string_table(dp, &S);
	print_pi_string_table(&S, 0);
	printf("Have read %ld bytes\n", dp - dp_head);
	
	close(fd);
}


/*-----------------DV Graph functions-----------*/

static void read_dag_file_to_pidag(char * filename, dr_pi_dag * P) {
	int err;
	int fd;
	struct stat statbuf;
	void *dp;
	
	fd = open(filename, O_RDONLY);
	if (fd < 0) {
		fprintf(stderr, "open: %d\n", errno);
		exit(1);
	}
	err = fstat(fd, &statbuf);
	if (err < 0) {
		fprintf(stderr, "fstat: %d\n", errno);
		exit(1);
	}
	printf("File state:\n"
				 "  st_size = %d\n",
				 (int) statbuf.st_size);
	
	dp = mmap(0, statbuf.st_size, PROT_READ, MAP_SHARED, fd, 0);
	if (!dp) {
		fprintf(stderr, "mmap: error\n");
		exit(1);
	}

	long n, m, nw;
	long * ldp = (long *) dp;
	n = *ldp;
	ldp++;
	m = *ldp;
	ldp++;
	nw = *ldp;
	ldp++;
	printf("n = %ld\nm = %ld\nnw = %ld\n", n, m, nw);
	P->n = n;
	P->m = m;
	P->num_workers = nw;

	P->T = (dr_pi_dag_node *) ldp;
	P->E = (dr_pi_dag_edge *) (P->T + n);
	dr_pi_string_table * stp = (dr_pi_string_table *) (P->E + m);
	dr_pi_string_table * S = (dr_pi_string_table *) malloc(sizeof(dr_pi_string_table));
	*S = *stp;
	S->I = (long *) (stp + 1);
	S->C = (const char *) (S->I + S->n);
	P->S = S;
	print_pi_string_table(S, 0);
	
	close(fd);
}

static long dv_count_nodes(dr_pi_dag *P) {
	long a[P->n];
	long n = 0;
	long i, j, k;
	for (i=0; i<P->m; i++)
		for (j=0; j<2; j++) {
			long u;
			if (!j)
				u = P->E[i].u;
			else
				u = P->E[i].v;
			for (k=0; k<n; k++) {
				if (u == a[k])
					break;
			}
			if (k == n) {
				a[n++] = u;
			}
		}
	return n;
}

static void convert_pidag_to_dvgraph(dr_pi_dag *P, dv_graph_t *G) {
	G->num_workers = P->num_workers;
	G->S = P->S;
	// Initialize G->T
	G->n = dv_count_nodes(P);
	G->T = (dv_graph_node_t *) malloc( G->n*sizeof(dv_graph_node_t) );
	// Fill G->T's idx, info and ej
	long n = 0;
	long i, j, k, u, v;
	int existing;
	for (i=0; i<P->m; i++) {
		for (j=0; j<2; j++) {
			if (!j)
				u = P->E[i].u;
			else
				u = P->E[i].v;
			existing = 0;
			for (k=0; k<n; k++)
				if (u == G->T[k].idx) {
					existing = 1;
					break;
				}
			if (!existing) {
				G->T[n].idx = u;
				G->T[n].info = &P->T[u].info;
				G->T[n].ej = NULL;
				dv_check(n < G->n);
				n++;		
			}
		}
	}
	dv_check(n == G->n);
	// Fill G->T's edge links
	dv_graph_node_t *nu, *nv;
	for (i=0; i<P->m; i++) {
		u = P->E[i].u;
		v = P->E[i].v;
		// Get nu
		for (j=0; j<n; j++)
			if (G->T[j].idx == u)
				break;
		dv_check(j < n);
		nu = &G->T[j];
		// Get dv
		for (j=0; j<n; j++)
			if (G->T[j].idx == v)
				break;
		dv_check(j < n);
		nv = &G->T[j];
		// Set edge link
		switch (P->E[i].kind) {
		case dr_dag_edge_kind_end:
			nu->ej = nv;
			dv_check(nu->info->kind == dr_dag_node_kind_end_task);
			break;
		case dr_dag_edge_kind_create:
			nu->el = nv;
			dv_check(nu->info->kind == dr_dag_node_kind_create_task);
			break;
		case dr_dag_edge_kind_create_cont:
			nu->er = nv;
			dv_check(nu->info->kind == dr_dag_node_kind_create_task);
			break;
		case dr_dag_edge_kind_wait_cont:
			nu->ej = nv;
			dv_check(nu->info->kind == dr_dag_node_kind_wait_tasks);
			break;
		default:
			dv_check(0);
		}
	}
	G->root_node = G->T;
	G->zoom_ratio = 1.0;
}

static void print_dvgraph_node(dv_graph_node_t *node, int i) {
	int kind = node->info->kind;
	printf(
				 "DV graph node %d:\n"
				 "  address: %p\n"
				 "  idx: %ld\n"
				 "  info.kind: %d (%s)\n",
				 i,
				 node,
				 node->idx,
				 kind,
				 NODE_KIND_NAMES[kind]);
	if (kind == dr_dag_node_kind_create_task) {
		printf("  el: %p\n"
					 "  er: %p\n",
					 node->el,
					 node->er
					 );
	} else if (kind == dr_dag_node_kind_wait_tasks
						 || kind == dr_dag_node_kind_end_task) {
		printf("  ej: %p\n",
					 node->ej
					 );
	} else {
		dv_check(0);
	}	
}

static void print_dvgraph_node_layout(dv_graph_node_t *node, int i) {
	int kind = node->info->kind;
	printf(
				 "DV graph node %d:\n"
				 "  address: %p\n"
				 "  info.kind: %d (%s)\n"
				 "  verline: %0.1f\n"
				 "  horline: %0.1f\n",
				 i,
				 node,
				 kind,
				 NODE_KIND_NAMES[kind],
				 node->vl->c,
				 node->hl->c);
	if (kind == dr_dag_node_kind_create_task) {
		printf("  el: %p\n"
					 "  er: %p\n",
					 node->el,
					 node->er
					 );
	} else if (kind == dr_dag_node_kind_wait_tasks
						 || kind == dr_dag_node_kind_end_task) {
		printf("  ej: %p\n",
					 node->ej
					 );
	} else {
		dv_check(0);
	}	
}

static void print_dvgraph_to_stdout(dv_graph_t *G) {
	printf(
				 "DV Graph: \n"
				 "  n: %ld\n"
				 "  num_workers: %ld\n",
				 G->n,
				 G->num_workers);
	int i;
	for (i=0; i<G->n; i++)
		print_dvgraph_node(&G->T[i], i);
}

static void print_layout_to_stdout(dv_graph_t *G) {
	printf(
				 "Layout of DV Graph: \n"
				 "  n: %ld\n"
				 "  num_workers: %ld\n",
				 G->n,
				 G->num_workers);
	int i;
	for (i=0; i<G->n; i++)
		print_dvgraph_node_layout(&G->T[i], i);
}


/*-----------------DAG layout functions-----------*/

static dv_grid_line_t * create_grid_line(int lv) {
	dv_grid_line_t * l = (dv_grid_line_t *) malloc(sizeof(dv_grid_line_t));
	l->c = 0;
	l->lv = lv;
	l->l = NULL;
	l->r = NULL;
	return l;
}

static dv_grid_line_t * grid_find_left_next_level(dv_grid_line_t *l) {
	int lv = l->lv;
	dv_grid_line_t * ll = l;
	while (ll && ll->lv > lv + 1)
		ll = ll->l;
	if (ll && ll->lv == lv + 1)
		return ll;
	return NULL;
}
static dv_grid_line_t * grid_find_right_next_level(dv_grid_line_t *l) {
	int lv = l->lv;
	dv_grid_line_t * ll = l;
	while (ll && ll->lv > lv + 1)
		ll = ll->r;
	if (ll && ll->lv == lv + 1)
		return ll;
	return NULL;
}
static dv_grid_line_t * grid_insert_left_next_level(dv_grid_line_t *l) {
	int lv = l->lv;
	dv_grid_line_t *newl = create_grid_line(lv+1);
	newl->l = l->l;
	newl->r = l;
	if (l->l)
		l->l->r = newl;
	l->l = newl;
	return newl;
}
static dv_grid_line_t * grid_insert_right_next_level(dv_grid_line_t *l) {
	int lv = l->lv;
	dv_grid_line_t *newl = create_grid_line(lv+1);
	newl->l = l;
	newl->r = l->r;
	if (l->r)
		l->r->l = newl;
	l->r = newl;
	return newl;
}
static dv_grid_line_t * grid_get_right_next_level(dv_grid_line_t *l) {
	dv_grid_line_t *ll = l->r;
	if (!ll) {
		printf("insert hline\n");
		ll = grid_insert_right_next_level(l);
	}
	return ll;
}
static dv_grid_line_t * grid_find_left_pre_level(dv_grid_line_t *l) {
	int lv = l->lv;
	dv_grid_line_t * ll = l;
	while (ll && ll->lv >= lv )
		ll = ll->l;
	if (ll && ll->lv == lv - 1)
		return ll;
	return NULL;
}
static dv_grid_line_t * grid_find_right_pre_level(dv_grid_line_t *l) {
	int lv = l->lv;
	dv_grid_line_t * ll = l;
	while (ll && ll->lv >= lv )
		ll = ll->r;
	if (ll && ll->lv == lv - 1)
		return ll;
	return NULL;
}
	
static void layout_dvgraph_set_node(dv_graph_node_t * node) {
	int kind = node->info->kind;
	dv_check(kind < dr_dag_node_kind_section);
	
	if (kind == dr_dag_node_kind_create_task) {
		
		dv_grid_line_t * vl_l = grid_find_left_next_level(node->vl);
		dv_grid_line_t * vl_r = grid_find_right_next_level(node->vl);
		dv_grid_line_t * hl_r = grid_get_right_next_level(node->hl);
		if (vl_l == NULL && vl_r == NULL) {
			vl_l = grid_insert_left_next_level(node->vl);
			vl_r = grid_insert_right_next_level(node->vl);
		} else if (vl_l == NULL || vl_r == NULL) {
			dv_check(0);
		}
		node->el->vl = vl_l;
		node->el->hl = hl_r;
		node->er->vl = vl_r;
		node->er->hl = hl_r;
												 
	} else if (kind == dr_dag_node_kind_end_task) {

		if (node->ej) {
			dv_grid_line_t * vl = grid_find_right_pre_level(node->vl);
			dv_check(vl);
			dv_grid_line_t * hl = grid_get_right_next_level(node->hl);
			node->ej->vl = vl;
			if (!node->ej->hl || node->ej->hl->c < hl->c)
				node->ej->hl = hl;
		}
		
	} else if (kind == dr_dag_node_kind_wait_tasks) {

		dv_grid_line_t * vl = grid_find_left_pre_level(node->vl);
		dv_check(vl);
		dv_grid_line_t * hl = grid_get_right_next_level(node->hl);
		node->ej->vl = vl;
		if (!node->ej->hl || node->ej->hl->c < hl->c)
			node->ej->hl = hl;
		
	}
		
	// Recursive calls
	switch (kind) {
	case dr_dag_node_kind_create_task:
		layout_dvgraph_set_node(node->el);
		layout_dvgraph_set_node(node->er);
		break;
	case dr_dag_node_kind_wait_tasks:
		layout_dvgraph_set_node(node->ej);
		break;
	case dr_dag_node_kind_end_task:
		if (node->ej)
			layout_dvgraph_set_node(node->ej);
		break;
	default:
		dv_check(0);
	}
}

static void layout_dvgraph(dv_graph_t *G) {
	// Set nodes to grid lines
	G->root_vl = create_grid_line(0);
	G->root_hl = create_grid_line(0);
	G->root_node->vl = G->root_vl;
	G->root_node->hl = G->root_hl;
	layout_dvgraph_set_node(G->root_node);
	
	// Set real coordinates
	int count = 0;
	dv_grid_line_t * l = G->root_vl;
	while (l->l) {
		l = l->l;
		count++;
	}
	G->root_vl->c = DV_PADDING + count * DV_HDIS;
	G->root_hl->c = DV_PADDING;
	l = G->root_vl;
	while (l->l) {
		l = l->l;
		l->c = l->r->c - DV_HDIS;
	}
	l = G->root_vl;
	while (l->r) {
		l = l->r;
		l->c = l->l->c + DV_HDIS;
	}
	l = G->root_hl;
	while (l->r) {
		l = l->r;
		l->c = l->l->c + DV_VDIS;
	}
}

static void calculate_graph_size(dv_graph_t *G) {
	
}
/*-----------------end of DAG layout functions-----------*/


/*-----------------DAG drawing functions-----------*/

static void draw_dvgraph_node(cairo_t *cr, dv_graph_node_t *node) {
	double x = node->vl->c;
	double y = node->hl->c;
	cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
	cairo_arc(cr, x, y, DV_RADIUS, 0.0, 2*M_PI);
	cairo_fill_preserve(cr);
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_stroke(cr);
}

static void draw_dvgraph_edge(cairo_t *cr, dv_graph_node_t *u, dv_graph_node_t *v) {
	cairo_set_source_rgb(cr, 0.0, 0.0, 0.0);
	cairo_move_to(cr, u->vl->c, u->hl->c + DV_RADIUS);
	cairo_line_to(cr, v->vl->c, v->hl->c - DV_RADIUS);
	cairo_stroke(cr);
}

static void draw_dvgraph(cairo_t *cr, dv_graph_t *G) {
	cairo_set_line_width(cr, 2.0);
	int i;
	for (i=0; i<G->n; i++) {
		draw_dvgraph_node(cr, &G->T[i]);
	}
	for (i=0; i<G->n; i++) {
		dv_graph_node_t *node = G->T + i;
		int kind = node->info->kind;
		if (kind == dr_dag_node_kind_create_task) {
			draw_dvgraph_edge(cr, node, node->el);
			draw_dvgraph_edge(cr, node, node->er);
		} else {
			if (node->ej)
				draw_dvgraph_edge(cr, node, node->ej);
		}
	}
}

/*-----------------end of DAG drawing functions-----------*/


/*-----------------Miscellaneous drawing functions------------*/
static void draw_hello(cairo_t *cr)
{
  cairo_select_font_face(cr, "serif", CAIRO_FONT_SLANT_NORMAL, CAIRO_FONT_WEIGHT_BOLD);
  cairo_set_font_size(cr, 32.0);
  cairo_set_source_rgb(cr, 0.0, 0.0, 1.0);
  cairo_move_to(cr, 10.0, 50.0);
  cairo_show_text(cr, "Hello, world");
}

static void draw_rounded_rectangle(cairo_t *cr)
{
  double x = 25.6;
  double y = 25.6;
  double width = 204.8;
  double height = 204.8;
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

  cairo_set_source_rgb(cr, 0.5, 0.5, 1);
  cairo_fill_preserve(cr);
  cairo_set_source_rgba(cr, 0.5, 0, 0, 0.5);
  cairo_set_line_width(cr, 10.0);
  cairo_stroke(cr);
}
/*-----------------end of Miscellaneous drawing functions-----*/



/*-----------------DV Visualizer GUI-------------------*/
static void do_drawing(cairo_t *cr)
{
	cairo_scale(cr, G->zoom_ratio, G->zoom_ratio);
	draw_dvgraph(cr, G);
  //draw_hello(cr);
  //draw_rounded_rectangle(cr);
}

static void do_zooming(double zoom_ratio)
{
	G->zoom_ratio = zoom_ratio;
	gtk_widget_queue_draw(darea);
}

static gboolean on_draw_event(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
  do_drawing(cr);
  return FALSE;
}

static gboolean on_btn_zoomin_clicked(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	do_zooming(G->zoom_ratio * DV_ZOOM_INCREMENT);
	return TRUE;
}

static gboolean on_btn_zoomout_clicked(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	do_zooming(G->zoom_ratio / DV_ZOOM_INCREMENT);
	return TRUE;
}

static gboolean on_mouse_press(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	return TRUE;
}

static gboolean on_mouse_release(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	return TRUE;
}

static gboolean on_mouse_move(GtkWidget *widget, cairo_t *cr, gpointer user_data)
{
	return TRUE;
}

static int open_gui(int argc, char *argv[])
{
  gtk_init(&argc, &argv);

	// Main window
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), 600, 600);
  gtk_window_set_title(GTK_WINDOW(window), "DAG Visualizer");
  g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
	GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
	gtk_container_add(GTK_CONTAINER(window), vbox);	

	// Toolbar
	GtkWidget *toolbar = gtk_toolbar_new();
	GtkToolItem *btn_zoomin = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_IN);
	GtkToolItem *btn_zoomout = gtk_tool_button_new_from_stock(GTK_STOCK_ZOOM_OUT);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoomin, -1);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_zoomout, -1);
	g_signal_connect(G_OBJECT(btn_zoomin), "clicked", G_CALLBACK(on_btn_zoomin_clicked), NULL);
	g_signal_connect(G_OBJECT(btn_zoomout), "clicked", G_CALLBACK(on_btn_zoomout_clicked), NULL);
	gtk_box_pack_start(GTK_BOX(vbox), toolbar, FALSE, FALSE, 0);
	
	// Drawing Area
  darea = gtk_drawing_area_new();
  g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), NULL);
	g_signal_connect(G_OBJECT(darea), "button-press-event", G_CALLBACK(on_mouse_press), NULL);
	g_signal_connect(G_OBJECT(darea), "button-release-event", G_CALLBACK(on_mouse_release), NULL);
	g_signal_connect(G_OBJECT(darea), "motion-notify-event", G_CALLBACK(on_mouse_move), NULL);
	gtk_box_pack_start(GTK_BOX(vbox), darea, TRUE, TRUE, 0);

	// Run main loop
  gtk_widget_show_all(window);
  gtk_main();

  return 1;
}
/*-----------------end of DV Visualizer GUI-------------------*/



/*-----------------Main begins-----------------*/

static void check_layout(dv_graph_t *G) {
	int i;
	for (i=0; i<G->n; i++) {
		dv_graph_node_t *node = G->T + i;
		int kind = node->info->kind;
		if (kind == dr_dag_node_kind_create_task) {
			printf(
						 "%ld(%0.0f,%0.0f) -> "
						 "%ld(%0.0f,%0.0f), "
						 "%ld(%0.0f,%0.0f)\n",
						 node->idx, node->vl->c, node->hl->c,
						 node->el->idx, node->el->vl->c, node->el->hl->c,
						 node->er->idx, node->er->vl->c, node->er->hl->c);
		} else if (kind == dr_dag_node_kind_wait_tasks
							 || kind == dr_dag_node_kind_end_task) {
			printf(
						 "%ld(%0.0f,%0.0f) -> ",
						 node->idx, node->vl->c, node->hl->c);
			if (node->ej)
				printf(
							 "%ld(%0.0f,%0.0f)\n",
							 node->ej->idx, node->ej->vl->c, node->ej->hl->c);
			else
				printf("null\n");
		}
	}
	dv_grid_line_t *l;
	l = G->root_vl;
	while (l) {
		printf("(%d,%0.0f) ", l->lv, l->c);
		l = l->l;
	}
	printf("\n");
	l = G->root_vl;
	while (l) {
		printf("(%d,%0.0f) ", l->lv, l->c);
		l = l->r;
	}
	printf("\n");
	l = G->root_hl;
	while (l) {
		printf("(%d,%0.0f) ", l->lv, l->c);
		l = l->l;
	}	
}

int main(int argc, char *argv[])
{
	if (argc > 1) {
		dr_pi_dag P[1];
		read_dag_file_to_pidag(argv[1], P);
		convert_pidag_to_dvgraph(P, G);
		print_dvgraph_to_stdout(G);
		layout_dvgraph(G);
		print_layout_to_stdout(G);
		check_layout(G);
		calculate_graph_size(G);
	}
	//return 1;
	/*if (argc > 1)
		read_dag_file_to_stdout(argv[1]);
	return 1;
	*/
	return open_gui(argc, argv);
}

/*-----------------Main ends-------------------*/
