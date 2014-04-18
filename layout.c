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

static void dv_dag_node_init(dv_dag_node_t *u, dv_dag_node_t *p, dr_pi_dag_node *pi) {
	u->pi = pi;
	dv_node_flag_init(u->f);
	dv_llist_init(u->links);
	dv_llist_init(u->heads);
	dv_llist_init(u->tails);
	u->vl = 0;
	u->hl = 0;
	dv_grid_init(u->grid, u);
	u->dc = 0L;
	u->lc = 0L;
	u->rc = 0L;
	u->c = 0L;
	u->parent = p;
	u->lv = (p)?(p->lv + 1):0;
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

			// Set u.f
			dv_node_flag_set(u->f, DV_NODE_FLAG_UNION);
			// Allocate memory for all child nodes
			u_a = p;
			u_b = p + (pi_b - pi_a);
			dv_check(u_b <= plim);
			p = u_b;
			// Set ux.pi
			pi_x = pi_a;
			for (u_x = u_a; u_x < u_b; u_x++) {
				dv_dag_node_init(u_x, u, pi_x);
				pi_x++;
				if (u_x->lv > G->lvmax)
					G->lvmax = u_x->lv;
			}
			// Push child nodes to stack
			for (u_x = u_b-1; u_x >= u_a; u_x--) {
				dv_stack_push(s, (void *) u_x);
			}
			// Set u.heads, u.tails
			dv_llist_add(u->heads, (void *) u_a);
			dv_llist_add(u->tails, (void *) (u_b - 1));
			// Set ux.links, u.tails
			for (u_x = u_a; u_x < u_b - 1; u_x++) {
				// x -> x+1
				dv_llist_add(u_x->links, (void *) (u_x + 1));
				if (u_x->pi->info.kind == dr_dag_node_kind_create_task) {
					pi_t = u_x->pi + u_x->pi->child_offset;
					dv_check(p < plim);
					u_t = p++;
					dv_dag_node_init(u_t, u, pi_t);
					// Push u_t to stack
					dv_stack_push(s, (void *) u_t);
					// c -> T
					dv_llist_add(u_x->links, (void *) u_t);
					// --> u.tails
					dv_llist_add(u->tails, (void *) u_t);
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
	dv_dag_node_init(G->rt, 0, P->T);
	G->lvmax = 0;
	// Traverse pidag's nodes
	dv_stack_t s[1];
	dv_stack_init(s);
	dv_stack_push(s, (void *) G->rt);
	while (s->top) {
		dv_dag_node_t * x = (dv_dag_node_t *) dv_stack_pop(s);
		p = dv_traverse_node(x->pi, x, p, plim, s, G);
	}
	// Drawing parameters
	G->init = 1;
	G->zoom_ratio = 1.0;
	G->x = 0.0;
	G->y = 0.0;
	G->width = G->height = 0.0;
	G->basex = G->basey = 0.0;
	dv_llist_init(G->itl);
}

/*-----------------end of Read .dag format-----------*/


/*-----------------Grid-related functions-----------*/

void dv_grid_line_init(dv_grid_line_t * l) {
	l->l = 0;
	l->r = 0;
	dv_llist_init(l->L);
	l->c = 0;
}

dv_grid_line_t * dv_grid_line_create() {
	dv_grid_line_t * l = (dv_grid_line_t *) dv_malloc( sizeof(dv_grid_line_t) );
	dv_grid_line_init(l);
	return l;
}

void dv_grid_init(dv_grid_t * grid, dv_dag_node_t *owner) {
	dv_grid_line_init(grid->vl);
	dv_grid_line_init(grid->hl);
	grid->owner = owner;
}

dv_grid_t * dv_grid_create(dv_dag_node_t *owner) {
	dv_grid_t * g = (dv_grid_t *) dv_malloc( sizeof(dv_grid_t) );
	dv_grid_init(g, owner);
	return g;
}

/*-----------------end of Grid-related functions-----------*/


/*-----------------Layout functions-----------*/

static void bind_node_1(dv_dag_node_t *u, dv_grid_line_t *l1, dv_grid_line_t *l2) {
	u->vl = l1;
	u->hl = l2;
	dv_llist_add(l1->L, u);
	dv_llist_add(l2->L, u);
}

static void dv_layout_bind_node(dv_dag_node_t * u, dv_grid_line_t * l1, dv_grid_line_t * l2) {
	// Bind itself
	bind_node_1(u, l1, l2);
	// Bind child nodes
	switch (u->pi->info.kind) {
	case dr_dag_node_kind_wait_tasks:
	case dr_dag_node_kind_end_task:
	case dr_dag_node_kind_create_task:
		break;
	case dr_dag_node_kind_section:
	case dr_dag_node_kind_task:
		if (dv_is_union(u)) {
			
			dv_check(u->heads->top);
			dv_dag_node_t * hd = (dv_dag_node_t *) u->heads->top->item;
			dv_layout_bind_node(hd, u->grid->vl, u->grid->hl);
			
		}
		break;
	default:
		dv_check(0);
		break;
	}

	// Call following links
	int count = 0;
	dv_llist_cell_t * c = u->links->top;
	while (c) {
		count++;
		c = c->next;
	}
	dv_dag_node_t * v;
	dv_dag_node_t * vv;
	switch (count) {
	case 0:
		break;
	case 1:
		v = (dv_dag_node_t *) u->links->top->item;
		if (!l2->r)
			l2->r = dv_grid_line_create();
		dv_layout_bind_node(v, l1, l2->r);
		break;
	case 2:
		v = (dv_dag_node_t *) u->links->top->item;
		vv = (dv_dag_node_t *) u->links->top->next->item;
		if (!l1->r)
			l1->r = dv_grid_line_create();
		if (!l1->l)
			l1->l = dv_grid_line_create();
		if (!l2->r)
			l2->r = dv_grid_line_create();
		dv_layout_bind_node(v, l1->r, l2->r);
		dv_layout_bind_node(vv, l1->l, l2->r);
		break;
	default:
		dv_check(0);
		break;
	}

}

static double dv_layout_calculate_gap(dv_dag_node_t *node) {
	double gap = 1.0;
	if (!node)
		return gap;
	if (dv_is_shrinking(node)) {
		gap = 1.0 - S->a->ratio * 1.0;
	} else if (dv_is_expanding(node)) {
		gap = S->a->ratio * 1.0;		
	}
	return gap;
}

static double dv_layout_count_line_left(dv_grid_line_t *);

static double dv_layout_count_line_right(dv_grid_line_t *l) {
	dv_llist_iterate_init(l->L);
	dv_dag_node_t * node;
  double max = 0.0;
	while ( node = (dv_dag_node_t *) dv_llist_iterate_next(l->L) ) {
		if (dv_is_union(node)
				&& ( !dv_is_shrinked(node) || dv_is_expanding(node) )) {
			dv_grid_line_t * ll = node->grid->vl;
			double num = 0.0;
			double gap = dv_layout_calculate_gap(node);
			while (ll->r) {
				ll->rc = dv_layout_count_line_right(ll);
				ll->r->lc = dv_layout_count_line_left(ll->r);
				num += ll->rc + gap + ll->r->lc;
				ll = ll->r;
			}
			ll->rc = dv_layout_count_line_right(ll);
			num += ll->rc;
			node->rc = num;
			if (num > max) {
				max = num;
			}
		}
	}
	return max;
}

static double dv_layout_count_line_left(dv_grid_line_t *l) {
	dv_llist_iterate_init(l->L);
	dv_dag_node_t * node;
  double max = 0.0;
	while ( node = (dv_dag_node_t *) dv_llist_iterate_next(l->L) ) {
		if (dv_is_union(node)
				&& ( !dv_is_shrinked(node) || dv_is_expanding(node) )) {
			dv_grid_line_t * ll = node->grid->vl;
			double num = 0.0;
			double gap = dv_layout_calculate_gap(node);
			while (ll->l) {
				ll->lc = dv_layout_count_line_left(ll);
				ll->l->rc = dv_layout_count_line_right(ll->l);
				num += ll->lc + gap + ll->l->rc;
				ll = ll->l;
			}
			ll->lc = dv_layout_count_line_left(ll);
			num += ll->lc;
			node->lc = num;
			if (num > max)
				max = num;
		}
	}
	return max;
}

static double dv_layout_count_line_down(dv_dag_node_t *node) {
	double c = 0.0;
	switch (node->pi->info.kind) {
	case dr_dag_node_kind_wait_tasks:
	case dr_dag_node_kind_end_task:
	case dr_dag_node_kind_create_task:
		break;
	case dr_dag_node_kind_section:
	case dr_dag_node_kind_task:
		if (dv_is_union(node)
				&& ( !dv_is_shrinked(node) || dv_is_expanding(node) )) {

			// Call recursive head
			dv_check(node->heads->top);
			dv_dag_node_t * hd = (dv_dag_node_t *) node->heads->top->item;
			hd->dc = dv_layout_count_line_down(hd);
			c = hd->dc;
			
		}
		break;
	default:
		dv_check(0);
		break;
	}

	// Calculate gap
	double gap = 0.0;
	if (node->links->top)
		gap = dv_layout_calculate_gap(node->parent);

	// Call following links
	dv_llist_iterate_init(node->links);
	dv_dag_node_t * n;
	double max = 0L;
	double num;
	while (n = (dv_dag_node_t *) dv_llist_iterate_next(node->links)) {
		n->dc = dv_layout_count_line_down(n);
		if (n->dc > max)
			max = n->dc;
	}
	
	return c + gap + max;
}

static void dv_layout_align_line_rightleft(dv_grid_t *grid) {
	dv_grid_line_t * l = grid->vl;
	double gap = dv_layout_calculate_gap(grid->owner);
	dv_grid_line_t * ll;
	dv_grid_line_t * lll;
	// Set c right
	ll = l;
	while (ll->r) {
		lll = ll->r;
		lll->c = ll->c + gap * DV_HDIS + (ll->rc + lll->lc) * DV_HDIS;
		ll = lll;
	}
	// Set c left
	ll = l;
	while (ll->l) {
		lll = ll->l;
		lll->c = ll->c - gap * DV_HDIS - (ll->lc + lll->rc) * DV_HDIS;
		ll = lll;
	}
	// Call recursive right
	ll = l;
	while (ll) {
		dv_llist_iterate_init(ll->L);
		dv_dag_node_t * n;
		while (n = (dv_dag_node_t *) dv_llist_iterate_next(ll->L)) {
			n->grid->vl->c = ll->c;
			dv_layout_align_line_rightleft(n->grid);
		}
		ll = ll->r;
	}
	// Call recursive left
	ll = l->l;
	while (ll) {
		dv_llist_iterate_init(ll->L);
		dv_dag_node_t * n;
		while (n = (dv_dag_node_t *) dv_llist_iterate_next(ll->L)) {
			n->grid->vl->c = ll->c;
			dv_layout_align_line_rightleft(n->grid);
		}
		ll = ll->l;
	}
}

static void dv_layout_align_line_down(dv_dag_node_t *node) {
	dv_check(node);
	dv_dag_node_t * n;
	double max = 0.0;
	dv_llist_iterate_init(node->links);
	while (n = (dv_dag_node_t *) dv_llist_iterate_next(node->links)) {
		if (n->dc > max)
			max = n->dc;
	}
	dv_llist_iterate_init(node->links);
	while (n = (dv_dag_node_t *) dv_llist_iterate_next(node->links)) {
		n->c = node->c + (node->dc - max) * DV_VDIS;
		dv_layout_align_line_down(n);
	}
	if (node->heads->top) {
		n = (dv_dag_node_t *) node->heads->top->item;
		n->c = node->c;
		dv_layout_align_line_down(n);
	}
}

void dv_layout_dvdag(dv_dag_t *G) {
	printf("dv_layout_dvdag() begins ..\n");
	
	// Bind nodes to lines
	dv_grid_init(G->grid, 0);
	dv_layout_bind_node(G->rt, G->grid->vl, G->grid->hl);
	
	// Count lines
	G->grid->vl->rc = dv_layout_count_line_right(G->grid->vl);
	G->grid->vl->lc = dv_layout_count_line_left(G->grid->vl);
	G->rt->dc = dv_layout_count_line_down(G->rt);

	// Align lines
	G->grid->vl->c = 0.0;
	G->grid->hl->c = 0.0;
	dv_layout_align_line_rightleft(G->grid);
	G->rt->c = 0.0;
	dv_layout_align_line_down(G->rt);

}

void dv_relayout_dvdag(dv_dag_t *G) {
	// Count lines
	G->grid->vl->rc = dv_layout_count_line_right(G->grid->vl);
	G->grid->vl->lc = dv_layout_count_line_left(G->grid->vl);
	G->rt->dc = dv_layout_count_line_down(G->rt);

	// Align lines
	G->grid->vl->c = 0.0;
	G->grid->hl->c = 0.0;
	dv_layout_align_line_rightleft(G->grid);
	G->rt->c = 0.0;
	dv_layout_align_line_down(G->rt);
}

/*-----------------end of Layout functions-----------*/


/*-----------Animation functions----------------------*/

static void dv_animation_set_moving_nodes(dv_dag_t *G, dv_animation_t *a) {
	dv_dag_node_t *node;
	int i;
	for (i=0; i<G->n; i++) {
		node = G->T + i;
		if (dv_is_union(node)) {
			
			if (node->lv >= a->new_sel && !dv_is_shrinked(node)) {
				// Need to be shrinked
				dv_node_flag_set(node->f, DV_NODE_FLAG_SHRINKING);
			} else if (node->lv < a->new_sel && dv_is_shrinked(node)) {
				// Need to be expanded
				dv_node_flag_set(node->f, DV_NODE_FLAG_EXPANDING);
			}
			
		}
	}
}

double dv_get_time()
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	return tv.tv_sec * 1.0E3 + ((double)tv.tv_usec) / 1.0E3;
}

void dv_animation_init(dv_animation_t *a) {
	a->on = 0;
	a->duration = 1000; // milliseconds
	a->step = 30; // milliseconds
	a->started = 0.0;
	a->new_sel = 0;
	a->ratio = 0.0;
}

static gboolean dv_animation_tick(gpointer data) {
	dv_animation_t *a = (dv_animation_t *) data;
	dv_check(a->on);
	a->ratio = (dv_get_time() - a->started) / a->duration;
	dv_check(a->ratio >= 0);
	if (a->ratio >= 1.0) {
		dv_animation_stop(a);
		gtk_widget_queue_draw(darea);
		return 0;
	}
	dv_relayout_dvdag(G);
	gtk_widget_queue_draw(darea);
	return 1;
}

void dv_animation_start(dv_animation_t *a) {
	dv_animation_set_moving_nodes(G, a);
	a->on = 1;
	a->ratio = 0.0;
	a->started = dv_get_time();
	g_timeout_add(a->step, dv_animation_tick, a);
}

void dv_animation_stop(dv_animation_t *a) {
	a->on = 0;
	S->sel = a->new_sel;
	
	dv_dag_node_t *node;
	int i;
	for (i=0; i<G->n; i++) {
		node = G->T + i;
		if (dv_node_flag_check(node->f, DV_NODE_FLAG_SHRINKING)) {
			dv_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKING);
			dv_node_flag_set(node->f, DV_NODE_FLAG_SHRINKED);
		} else if (dv_node_flag_check(node->f, DV_NODE_FLAG_EXPANDING)) {
			dv_node_flag_remove(node->f, DV_NODE_FLAG_EXPANDING);
			dv_node_flag_remove(node->f, DV_NODE_FLAG_SHRINKED);
		}
	}
	dv_relayout_dvdag(G);
	gtk_widget_queue_draw(darea);
}

/*-----------end of Animation functions----------------------*/
