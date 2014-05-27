#include "dagviz.h"

static dv_graph_t G[1];
static GtkWidget *window;
static GtkWidget *darea;
static dv_status_t S[1];

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
		"  info.logical_node_counts: %ld/%ld/%ld\n"
		"  info.logical_edge_counts: %ld/%ld/%ld/%ld\n"
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
		dn->info.logical_node_counts[0],
		dn->info.logical_node_counts[1],
		dn->info.logical_node_counts[2],
		dn->info.logical_edge_counts[0],
		dn->info.logical_edge_counts[1],
		dn->info.logical_edge_counts[2],
		dn->info.logical_edge_counts[3],
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
	if (G->n == 0)
		G->n = 1;
	G->T = (dv_graph_node_t *) malloc( G->n*sizeof(dv_graph_node_t) );
	// Fill G->T's idx, info and ej (NULL), ek (-1)
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
				G->T[n].ek = -1;
				G->T[n].ej = NULL;
				dv_check(n < G->n);
				n++;		
			}
		}
	}
	dv_check(G->n == 1 || n == G->n);
	// Fill G->T's edge links
	dv_graph_node_t *nu, *nv;
	printf("num edges m = %ld\n", P->m);
	if (n == 0) {
		for (i=0; i<P->n; i++)
			if (P->T[i].info.kind == dr_dag_node_kind_end_task) {
				G->T[n].idx = i;
				G->T[n].info = &P->T[i].info;
				G->T[n].ek = -1;
				G->T[n].ej = NULL;
				n++;
				break;
			}				
	}
	for (i=0; i<P->m; i++) {
		u = P->E[i].u;
		v = P->E[i].v;
		// Get nu
		for (j=0; j<n; j++)
			if (G->T[j].idx == u)
				break;
		dv_check(j < n);
		nu = &G->T[j];
		// Get nv
		for (j=0; j<n; j++)
			if (G->T[j].idx == v)
				break;
		dv_check(j < n);
		nv = &G->T[j];
		// Set edge link
		nu->ek = P->E[i].kind;
		switch (P->E[i].kind) {
		case dr_dag_edge_kind_end:
			nu->ej = nv;
			//dv_check(nu->info->kind == dr_dag_node_kind_end_task);
			break;
		case dr_dag_edge_kind_create:
			nu->el = nv;
			//dv_check(nu->info->kind == dr_dag_node_kind_create_task);
			break;
		case dr_dag_edge_kind_create_cont:
			nu->er = nv;
			//dv_check(nu->info->kind == dr_dag_node_kind_create_task);
			break;
		case dr_dag_edge_kind_wait_cont:
			nu->ej = nv;
			//dv_check(nu->info->kind == dr_dag_node_kind_wait_tasks);
			break;
		default:
			printf("P->E[%ld].kind = %d\n", i, P->E[i].kind);
			dv_check(0);
		}
	}
	G->root_node = G->T;
	G->zoom_ratio = 1.0;
	G->itl.item = NULL;
	G->itl.next = NULL;
}

static void print_dvgraph_node(dv_graph_node_t *node, int i) {
	int kind = node->info->kind;
	printf(
				 "DV graph node %d:\n"
				 "  address: %p\n"
				 "  idx: %ld\n"
				 "  info.kind: %d (%s)\n"
				 "  ek: %s\n",
				 i,
				 node,
				 node->idx,
				 kind,
				 NODE_KIND_NAMES[kind],
				 EDGE_KIND_NAMES[node->ek]);
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
						 || kind == dr_dag_node_kind_end_task
						 || kind == dr_dag_node_kind_section
						 || kind == dr_dag_node_kind_task) {
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
		if (G->T[i].ek == -1)
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
		ll = grid_insert_right_next_level(l);
	}
	return ll;
}
static dv_grid_line_t * grid_find_left_pre_level(dv_grid_line_t *l) {
	int lv = l->lv;
	dv_grid_line_t * ll = l;
	while (ll && ll->lv >= lv ) {
		ll = ll->l;
	}
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
	int ek = node->ek;
	
	if (ek == dr_dag_edge_kind_create
			|| ek == dr_dag_edge_kind_create_cont) {
		
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
		layout_dvgraph_set_node(node->el);
		layout_dvgraph_set_node(node->er);
												 
	} else if (ek == dr_dag_edge_kind_end) {

		if (node->ej) {
			dv_grid_line_t * vl = grid_find_right_pre_level(node->vl);
			dv_check(vl);
			dv_grid_line_t * hl = grid_get_right_next_level(node->hl);
			if (node->ej->vl == NULL || node->ej->hl == NULL) {
				dv_check(node->ej->vl == NULL && node->ej->hl == NULL);
				node->ej->vl = vl;
				node->ej->hl = hl;
				layout_dvgraph_set_node(node->ej);
			} else {
				dv_check(node->ej->vl != NULL && node->ej->hl != NULL);
				if (node->ej->hl->lv < hl->lv) {
					node->ej->hl = hl;
					layout_dvgraph_set_node(node->ej);
				} else if (node->ej->hl->lv > hl->lv && node->ej->vl != vl) {
					node->ej->vl = vl;
					layout_dvgraph_set_node(node->ej);
				}
			}
		}
		
	} else if (ek == dr_dag_edge_kind_wait_cont) {

		if (node->info->kind == dr_dag_node_kind_section) {
			
			node->ej->vl = node->vl;
			node->ej->hl = grid_get_right_next_level(node->hl);
			layout_dvgraph_set_node(node->ej);		

		} else {

			dv_grid_line_t * vl = grid_find_left_pre_level(node->vl);
			if (!vl)
				vl = node->vl;
			dv_check(vl);
			dv_grid_line_t * hl = grid_get_right_next_level(node->hl);
			if (node->ej->vl == NULL || node->ej->hl == NULL) {
				dv_check(node->ej->vl == NULL && node->ej->hl == NULL);
				node->ej->vl = vl;
				node->ej->hl = hl;
				layout_dvgraph_set_node(node->ej);
			} else {
				dv_check(node->ej->vl != NULL && node->ej->hl != NULL);
				if (node->ej->hl->lv < hl->lv) {
					node->ej->hl = hl;
					layout_dvgraph_set_node(node->ej);
				} else if (node->ej->hl->lv > hl->lv && node->ej->vl != vl) {
					node->ej->vl = vl;
					layout_dvgraph_set_node(node->ej);
				}
			}

		}
		
	} else {
		/* TODO: should not get here so many times */
		printf("This node %ld (%s) has ek = %d\n", node->idx, NODE_KIND_NAMES[node->info->kind], ek);
	}

}

static void layout_dvgraph(dv_graph_t *G) {
	printf("layout_dvgraph()...\n");
	// Set nodes to grid lines
	G->root_vl = create_grid_line(0);
	G->root_hl = create_grid_line(0);
	G->root_node->vl = G->root_vl;
	G->root_node->hl = G->root_hl;
	layout_dvgraph_set_node(G->root_node);
	
	// Set real coordinates for grid lines
	printf("  set grid lines...\n");
	dv_grid_line_t * l;
	l = G->root_vl;
	l->c = 0.0;
	while (l->l) {
		l->l->c = l->c - DV_HDIS;
		l = l->l;
	}
	l = G->root_vl;
	while (l->r) {
		l->r->c = l->c + DV_HDIS;
		l = l->r;
	}
	int count = 0;
	l = G->root_hl;
	while (l->r) {
		count++;
		l = l->r;
	}
	l = G->root_hl;
	l->c = - DV_VDIS * count / 2;
	while (l->r) {
		l->r->c = l->c + DV_VDIS;
		l = l->r;
	}

	// Set central point's coordinates
	G->x = 0.0;
	G->y = 0.0;	
}

/*-----------------end of DAG layout functions-----------*/



/*-----------------DAG drawing functions-----------*/

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
	gdk_rgba_parse(&color, COLORS[v % NUM_COLORS]);
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

static void draw_dvgraph_node(cairo_t *cr, dv_graph_node_t *node) {
	double x = node->vl->c;
	double y = node->hl->c;
	double c[4];
	int v = 0;
	switch (S->nc) {
	case 0:
		v = node->info->worker; break;
	case 1:
		v = node->info->cpu; break;
	case 2:
		v = node->info->kind; break;
	default:
		v = node->info->worker;
	}
	lookup_color(v, c, c+1, c+2, c+3);
	cairo_set_source_rgba(cr, c[0], c[1], c[2], c[3]);
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

static void draw_dvgraph_infotag(cairo_t *cr, dv_graph_node_t *node) {
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
}

static void draw_dvgraph(cairo_t *cr, dv_graph_t *G) {
	cairo_set_line_width(cr, 2.0);
	int i;
	// Draw nodes
	for (i=0; i<G->n; i++) {
		draw_dvgraph_node(cr, &G->T[i]);
	}
	// Draw edges
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
	// Draw info tags
	dv_linked_list_t *l = &G->itl;
	while (l) {
		if (l->item) {
			draw_dvgraph_infotag(cr, (dv_graph_node_t *) l->item);
		}
		l = l->next;
	}
}

/*-----------------end of DAG drawing functions-----------*/


/*-----------------DV Visualizer GUI-------------------*/
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

static void do_drawing(cairo_t *cr)
{
	cairo_translate(cr, 0.5*G->width + G->x, 0.5*G->height + G->y);
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
	G->width = gtk_widget_get_allocated_width(widget);
	G->height = gtk_widget_get_allocated_height(widget);
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

static gboolean on_scroll_event(GtkWidget *widget, GdkEventScroll *event, gpointer user_data) {
	if (event->direction == GDK_SCROLL_UP) {
		do_zooming(G->zoom_ratio * DV_ZOOM_INCREMENT);
	} else if (event->direction == GDK_SCROLL_DOWN) {
		do_zooming(G->zoom_ratio / DV_ZOOM_INCREMENT);
	}
	return TRUE;
}

static dv_graph_node_t *get_clicked_node(double x, double y) {
	dv_graph_node_t *ret = NULL;
	dv_grid_line_t *vl, *hl;
	int i;
	for (i=0; i<G->n; i++) {
		vl = G->T[i].vl;
		hl = G->T[i].hl;
		if (vl->c - DV_RADIUS < x && x < vl->c + DV_RADIUS
				&& hl->c - DV_RADIUS < y && y < hl->c + DV_RADIUS) {
			ret = &G->T[i];
			break;
		}
	}
	return ret;
}

static void *linked_list_remove(dv_linked_list_t *list, void *item) {
	void * ret = NULL;
	dv_linked_list_t *l = list;
	dv_linked_list_t *pre = NULL;
	while (l) {
		if (l->item == item) {
			break;
		}
		pre = l;
		l = l->next;
	}
	if (l && l->item == item) {
		ret = l->item;
		if (pre) {
			pre->next = l->next;
			free(l);
		} else {
			l->item = NULL;
		}		
	}
	return ret;
}

static void linked_list_add(dv_linked_list_t *list, void *item) {
	if (list->item == NULL) {
		list->item = item;
	} else {
		dv_linked_list_t *newl = (dv_linked_list_t *) malloc(sizeof(dv_linked_list_t));
		newl->item = item;
		newl->next = NULL;
		dv_linked_list_t *l = list;
		while (l->next)
			l = l->next;
		l->next = newl;
	}
}

static gboolean on_button_event(GtkWidget *widget, GdkEventButton *event, gpointer user_data)
{
	if (event->type == GDK_BUTTON_PRESS) {
		// Drag
		S->drag_on = 1;
		S->pressx = event->x;
		S->pressy = event->y;
	}	else if (event->type == GDK_BUTTON_RELEASE) {
		// Drag
		S->drag_on = 0;
		S->pressx = 0;
		S->pressy = 0;
	} else if (event->type == GDK_2BUTTON_PRESS) {
		// Info tag
		double ox = (event->x - 0.5*G->width - G->x) / G->zoom_ratio;
		double oy = (event->y - 0.5*G->height - G->y) / G->zoom_ratio;
		dv_graph_node_t *node_pressed = get_clicked_node(ox, oy);
		if (node_pressed) {
			if (!linked_list_remove(&G->itl, node_pressed)) {
				linked_list_add(&G->itl, node_pressed);
			}
			gtk_widget_queue_draw(darea);
		}
	}
	return TRUE;
}

static gboolean on_motion_event(GtkWidget *widget, GdkEventMotion *event, gpointer user_data)
{
	if (S->drag_on) {
		// Drag
		double deltax = event->x - S->pressx;
		double deltay = event->y - S->pressy;
		G->x += deltax;
		G->y += deltay;
		S->pressx = event->x;
		S->pressy = event->y;
		gtk_widget_queue_draw(darea);
	}
	return TRUE;
}

static gboolean on_combobox_changed(GtkComboBox *widget, gpointer user_data) {
	S->nc = gtk_combo_box_get_active(widget);
	gtk_widget_queue_draw(darea);
	return TRUE;
}

static int open_gui(int argc, char *argv[])
{
  gtk_init(&argc, &argv);

	// Main window
  window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  //gtk_window_set_position(GTK_WINDOW(window), GTK_WIN_POS_CENTER);
  gtk_window_set_default_size(GTK_WINDOW(window), 1000, 700);
	//gtk_window_fullscreen(GTK_WINDOW(window));
	gtk_window_maximize(GTK_WINDOW(window));
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

	// Combo box
	GtkToolItem *btn_combo = gtk_tool_item_new();
	GtkWidget *combobox = gtk_combo_box_text_new();
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "worker", "Worker");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "cpu", "CPU");
	gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(combobox), "kind", "Node kind");
	gtk_combo_box_set_active(GTK_COMBO_BOX(combobox), 0);
	g_signal_connect(G_OBJECT(combobox), "changed", G_CALLBACK(on_combobox_changed), NULL);
	gtk_container_add(GTK_CONTAINER(btn_combo), combobox);
	gtk_toolbar_insert(GTK_TOOLBAR(toolbar), btn_combo, -1);
	
	// Drawing Area
  darea = gtk_drawing_area_new();
  g_signal_connect(G_OBJECT(darea), "draw", G_CALLBACK(on_draw_event), NULL);
	gtk_widget_add_events(GTK_WIDGET(darea), GDK_SCROLL_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK);
	g_signal_connect(G_OBJECT(darea), "scroll-event", G_CALLBACK(on_scroll_event), NULL);
	g_signal_connect(G_OBJECT(darea), "button-press-event", G_CALLBACK(on_button_event), NULL);
	g_signal_connect(G_OBJECT(darea), "button-release-event", G_CALLBACK(on_button_event), NULL);
	g_signal_connect(G_OBJECT(darea), "motion-notify-event", G_CALLBACK(on_motion_event), NULL);
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
}

int main(int argc, char *argv[])
{
	if (argc > 1) {
		dr_pi_dag P[1];
		read_dag_file_to_pidag(argv[1], P);
		convert_pidag_to_dvgraph(P, G);
		//print_dvgraph_to_stdout(G);
		layout_dvgraph(G);
		printf("finished layout.\n");
		//print_layout_to_stdout(G);
		//check_layout(G);
		S->drag_on = 0;
		S->pressx = 0.0;
		S->pressy = 0.0;
	}
	//return 1;
	/*if (argc > 1)
		read_dag_file_to_stdout(argv[1]);
	return 1;
	*/
	return open_gui(argc, argv);
}

/*-----------------Main ends-------------------*/
