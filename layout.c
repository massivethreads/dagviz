#include "dagviz.h"

dv_status_t S[1];
dr_pi_dag P[1];
dv_dag_t G[1];

/*-----------------Read .dag format-----------*/

void dv_read_dag_file_to_pidag(char * filename, dr_pi_dag * P) {
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
	printf("File status:\n"
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
	printf("PI DAG:\n"
				 "  n = %ld\n"
				 "  m = %ld\n"
				 "  nw = %ld\n", n, m, nw);
	P->n = n;
	P->m = m;
	P->num_workers = nw;

	P->T = (dr_pi_dag_node *) ldp;
	P->E = (dr_pi_dag_edge *) (P->T + n);
	dr_pi_string_table * stp = (dr_pi_string_table *) (P->E + m);
	dr_pi_string_table * S = (dr_pi_string_table *) dv_malloc( sizeof(dr_pi_string_table) );
	*S = *stp;
	S->I = (long *) (stp + 1);
	S->C = (const char *) (S->I + S->n);
	P->S = S;
	
	close(fd);
}

static void dv_dag_node_init(dv_dag_node_t *u) {
	u->pi = 0;
	u->s = 0b000;
	dv_linked_list_init(u->links);
	dv_linked_list_init(u->heads);
	dv_linked_list_init(u->tails);
	u->vl = 0;
	u->hl = 0;
	//u->grid
}

static dv_dag_node_t * dv_traverse_node(dr_pi_dag_node *pi, dv_dag_node_t *u, dv_dag_node_t *p, dv_dag_node_t *plim, dv_stack_t *s, dv_dag_t *G) {
	dr_pi_dag_node * pi_a;
	dr_pi_dag_node * pi_b;
	dr_pi_dag_node * pi_x;
	dr_pi_dag_node * pi_t;
	dv_dag_node_t * u_a;
	dv_dag_node_t * u_b;
	dv_dag_node_t * u_x;
	dv_dag_node_t * u_t;
	switch (pi->info.kind) {
	case dr_dag_node_kind_wait_tasks:
	case dr_dag_node_kind_end_task:
	case dr_dag_node_kind_create_task:
		// Do nothing
		break;
	case dr_dag_node_kind_section:
	case dr_dag_node_kind_task:
		pi_a = pi + pi->subgraphs_begin_offset;
		pi_b = pi + pi->subgraphs_end_offset;
		if (pi_a < pi_b) {

			// Set u.s
			u->s = 0b001;
			// Allocate memory for all child nodes
			u_a = p;
			u_b = p + (pi_b - pi_a);
			dv_check(u_b <= plim);
			p = u_b;
			// Set ux.pi
			pi_x = pi_a;
			for (u_x = u_a; u_x < u_b; u_x++) {
				dv_dag_node_init(u_x);
				u_x->pi = pi_x++;
			}
			// Push child nodes to stack
			for (u_x = u_b-1; u_x >= u_a; u_x--) {
				dv_stack_push(s, (void *) u_x);
			}
			// Set u.heads, u.tails
			dv_linked_list_add(u->heads, (void *) u_a);
			dv_linked_list_add(u->tails, (void *) (u_b - 1));
			// Set ux.links, u.tails
			for (u_x = u_a; u_x < u_b - 1; u_x++) {
				// x -> x+1
				dv_linked_list_add(u_x->links, (void *) (u_x + 1));
				if (u_x->pi->info.kind == dr_dag_node_kind_create_task) {
					pi_t = u_x->pi + u_x->pi->child_offset;
					dv_check(p < plim);
					u_t = p++;
					dv_dag_node_init(u_t);
					u_t->pi = pi_t;
					// Push u_t to stack
					dv_stack_push(s, (void *) u_t);
					// c -> T
					dv_linked_list_add(u_x->links, (void *) u_t);
					// --> u.tails
					dv_linked_list_add(u->tails, (void *) u_t);
				}
			}

		} else {

			// Do nothing
			
		}
		break;
	default:
		dv_check(0);
		break;
	}

	return p;
}

void dv_convert_pidag_to_dvdag(dr_pi_dag *P, dv_dag_t *G) {
	G->n = P->n;
	G->nw = P->num_workers;
	G->st = P->S;
	// Initialize rt, T
	G->T = (dv_dag_node_t *) dv_malloc( sizeof(dv_dag_node_t) * G->n );
	dv_dag_node_t * p = G->T;
	dv_dag_node_t * plim = G->T + G->n;
	dv_check(p < plim);
	G->rt = p++;
	dv_dag_node_init(G->rt);
	G->rt->pi = P->T;
	// Traverse pidag's nodes
	dv_stack_t s[1];
	dv_stack_init(s);
	dv_stack_push(s, (void *) G->rt);
	while (s->top) {
		dv_dag_node_t * x = (dv_dag_node_t *) dv_stack_pop(s);
		p = dv_traverse_node(x->pi, x, p, plim, s, G);
	}
	// Drawing parameters
	G->zoom_ratio = 1.0;
	G->itl.item = NULL;
	G->itl.next = NULL;
}

/*-----------------end of Read .dag format-----------*/


/*-----------------Layout functions-----------*/

static void dv_grid_line_init(dv_grid_line_t * l) {
	l->c = 0;
	l->l = 0;
	l->r = 0;
}

static dv_grid_line_t * dv_grid_line_create() {
	dv_grid_line_t * l = (dv_grid_line_t *) dv_malloc( sizeof(dv_grid_line_t) );
	dv_grid_line_init(l);
	return l;
}

static void dv_grid_init(dv_grid_t * grid) {
	dv_grid_line_init(grid->vl);
	dv_grid_line_init(grid->hl);
}

static dv_grid_t * dv_grid_create() {
	dv_grid_t * g = (dv_grid_t *) dv_malloc( sizeof(dv_grid_t) );
	dv_grid_init(g);
	return g;
}

static void dv_layout_set_node(dv_dag_node_t * node) {
	
}

void dv_layout_dvdag(dv_dag_t *G) {
	printf("dv_layout_dvdag()...\n");
	// Set nodes to grid lines
	dv_grid_init(G->grid);
	G->rt->vl = G->grid->vl;
	G->rt->hl = G->grid->hl;
	dv_layout_set_node(G->rt);
	
	// Set real coordinates for grid lines
	printf("  set grid lines...\n");

	// Set central point's coordinates
	G->x = 0.0;
	G->y = 0.0;
}

/*-----------------end of Layout functions-----------*/

