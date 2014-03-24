#include "dagviz.h"

dv_graph_t G[1];
dv_status_t S[1];

/*-----------------Read .dag format-----------*/

void read_dag_file_to_pidag(char * filename, dr_pi_dag * P) {
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

static long pidag_count_nodes(dr_pi_dag *P) {
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

void convert_pidag_to_dvgraph(dr_pi_dag *P, dv_graph_t *G) {
	G->num_workers = P->num_workers;
	G->S = P->S;
	// Initialize G->T
	G->n = pidag_count_nodes(P);
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

/*-----------------end of Read .dag format-----------*/


/*-----------------Layout functions-----------*/

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

void layout_dvgraph(dv_graph_t *G) {
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

/*-----------------end of Layout functions-----------*/

