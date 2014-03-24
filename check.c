#include "dagviz.h"

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


void read_dag_file_to_stdout(char * filename) {
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

void check_layout(dv_graph_t *G) {
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

