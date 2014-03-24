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

void print_dvgraph_to_stdout(dv_graph_t *G) {
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

void print_layout_to_stdout(dv_graph_t *G) {
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

