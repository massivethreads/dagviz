#include "dagviz.h"

const char * const DV_COLORS[] =
	{"orange", "gold", "cyan", "azure", "green",
	 "magenta", "brown1", "burlywood1", "peachpuff", "aquamarine",
	 "chartreuse", "skyblue", "burlywood", "cadetblue", "chocolate",
	 "coral", "cornflowerblue", "cornsilk4", "darkolivegreen1", "darkorange1",
	 "khaki3", "lavenderblush2", "lemonchiffon1", "lightblue1", "lightcyan",
	 "lightgoldenrod", "lightgoldenrodyellow", "lightpink2", "lightsalmon2", "lightskyblue1",
	 "lightsteelblue3", "lightyellow3", "maroon1", "yellowgreen"};

const char * const NODE_KIND_NAMES[] = {
	"create",
	"wait",
	"end",
	"section",
	"task"
};

const char * const EDGE_KIND_NAMES[] = {
	"end",
	"create",
	"create_cont",
	"wait_cont",
	"max",
};

void print_pi_dag_node(dr_pi_dag_node * dn, int i) {
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

void print_pi_dag_edge(dr_pi_dag_edge * de, int i) {
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

void print_pi_string_table(dr_pi_string_table * stp, int i) {
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


static void print_dvdag_node(dv_dag_node_t *node, int i) {
	int kind = node->pi->info.kind;
	printf("Node %d (%p): %d(%s)\n",				 
				 i,
				 node,
				 kind,
				 NODE_KIND_NAMES[kind]);
}

void print_dvdag(dv_dag_t *G) {
	printf(
				 "DV DAG: \n"
				 "  n: %ld\n"
				 "  nw: %ld\n",
				 G->n,
				 G->nw);
	int i;
	for (i=0; i<G->n; i++) {
			print_dvdag_node(G->T + i, i);
	}
}

static void print_layout_node(dv_dag_node_t *node, int i) {
	int kind = node->pi->info.kind;
	printf(
				 "  Node %d:\n"
				 "    info.kind: (%s)\n"
				 "    (vl->c,c): (%0.1f,%0.1f)\n"
				 "    grid->vl->(lc,rc): (%0.1f,%0.1f)\n"
				 "    dc: %0.1f\n",
				 i,
				 NODE_KIND_NAMES[kind],
				 node->vl->c, node->c,
				 node->grid->vl->lc, node->grid->vl->rc,
				 node->dc);
}

void print_layout(dv_dag_t *G) {
	printf(
				 "Layout of DV DAG: (n=%ld)\n",
				 G->n);
	int i;
	for (i=0; i<G->n; i++)
		print_layout_node(G->T + i, i);
}


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


void print_dag_file(char * filename) {
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

void check_layout(dv_dag_t *G) {
	/*int i;
	for (i=0; i<G->n; i++) {
		dv_dag_node_t *node = G->T + i;
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
		}*/
}

