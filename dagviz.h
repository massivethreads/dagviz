#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <cairo.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>


/*-----------------DR DAG structures-----------------*/

typedef unsigned long long dr_clock_t;
typedef struct dr_dag_node dr_dag_node;

#define NO_CHUNK 1

typedef enum {
	dr_dag_node_kind_create_task,
	dr_dag_node_kind_wait_tasks,
	dr_dag_node_kind_end_task,
	dr_dag_node_kind_section,
	dr_dag_node_kind_task,
} dr_dag_node_kind_t;

const char * const NODE_KIND_NAMES[] = {
	"create",
	"wait",
	"end",
	"section",
	"task"
};

typedef enum {
	dr_dag_edge_kind_end,    /* end -> parent */
	dr_dag_edge_kind_create, /* create -> child */
	dr_dag_edge_kind_create_cont, /* create -> next */
	dr_dag_edge_kind_wait_cont,	/* wait -> next */
	dr_dag_edge_kind_max,
} dr_dag_edge_kind_t;

const char * const EDGE_KIND_NAMES[] = {
	"end",
	"create",
	"create_cont",
	"wait_cont",
	"max",
};

typedef struct dr_dag_node_list dr_dag_node_list;

#if NO_CHUNK
#else
typedef struct dr_dag_node_chunk dr_dag_node_chunk;
#endif

  /* list of dag nodes */
struct dr_dag_node_list {
#if NO_CHUNK
	dr_dag_node * head;
	dr_dag_node * tail;
#else
	dr_dag_node_chunk * head;
	dr_dag_node_chunk * tail;
#endif
};

typedef struct {
	/* pointer to filename. valid in dr_dag_node */
	const char * file;
	/* index in dr_flat_string_table. valid in dr_pi_dag_node */    
	long file_idx;
	/* line number */
	long line;
} code_pos;

typedef struct {
	dr_clock_t t;		/* clock */
	code_pos pos;		/* code position */
} dr_clock_pos;

typedef struct dr_dag_node_info {
	dr_clock_pos start; /* start clock,position */
	dr_clock_pos end;	 /* end clock,position */
	dr_clock_t est;	 /* earliest start time */
	dr_clock_t t_1;	 /* work */
	dr_clock_t t_inf;	 /* critical path */
	/* number of nodes in this subgraph */
	long node_counts[dr_dag_node_kind_section];
	/* number of edges connecting nodes in this subgraph */
	long edge_counts[dr_dag_edge_kind_max];
	/* direct children of create_task type */
	long n_child_create_tasks;
	int worker;
	int cpu;
	dr_dag_node_kind_t kind;
	dr_dag_node_kind_t last_node_kind;
} dr_dag_node_info;

typedef struct dr_pi_dag_node dr_pi_dag_node;

struct dr_dag_node {
	dr_dag_node_info info;
#if NO_CHUNK
	dr_dag_node * next;
#endif
	dr_pi_dag_node * forward;
	union {
		dr_dag_node * child;		/* kind == create_task */
		struct {				/* kind == section/task */
			dr_dag_node_list subgraphs[1];
			union {
				dr_dag_node * parent_section; /* kind == section */
				dr_dag_node * active_section; /* kind == task */
			};
		};
	};
};

#if NO_CHUNK
typedef struct dr_dag_node_page {
	struct dr_dag_node_page * next;
	long sz;
	dr_dag_node nodes[2];	/* this is the minimum size */
} dr_dag_node_page;

typedef struct {
	dr_dag_node * head;
	dr_dag_node * tail;
	dr_dag_node_page * pages;
} dr_dag_node_freelist;
#endif

#if NO_CHUNK
#else
/* list of dr_dag_node, used to dynamically
	 grow the subgraphs of section/task */
enum { dr_dag_node_chunk_sz = 3 };
/* 16 + 128 * 7 */
typedef struct dr_dag_node_chunk {
	struct dr_dag_node_chunk * next;
	int n;
	dr_dag_node a[dr_dag_node_chunk_sz];
} dr_dag_node_chunk;
#endif



/*-----------------DR PI DAG structures-----------------*/

/* a node of the dag, position independent */
struct dr_pi_dag_node {
  /* misc. information about this node */
  dr_dag_node_info info;
  /* two indexes in the edges array, pointing to 
     the begining and the end of edges from this node */
  long edges_begin;	 
  long edges_end;
  union {
    /* valid when this node is a create node.
       index of its child */
    long child_offset;
    /* valid when this node is a section or task node
       begin/end indexes of its subgraphs */
    struct {
      long subgraphs_begin_offset;
      long subgraphs_end_offset;
    };
  };
};

typedef struct dr_pi_dag_edge {
  dr_dag_edge_kind_t kind;
  long u;
  long v;
} dr_pi_dag_edge;

typedef struct dr_string_table_cell {
  struct dr_string_table_cell * next;
  const char * s;
} dr_string_table_cell;

typedef struct {
  dr_string_table_cell * head;
  dr_string_table_cell * tail;
  long n;
} dr_string_table;

typedef struct {
  long n;			/* number of strings */
  long sz;			/* total bytes including headers */
  long * I;			/* index I[0] .. I[n-1] */
  const char * C;		/* char array */
} dr_pi_string_table;

typedef struct dr_pi_dag {
  long n;			/* length of T */
  long m;			/* length of E */
  long num_workers;		/* number of workers */
  dr_pi_dag_node * T;		/* all nodes in a contiguous array */
  dr_pi_dag_edge * E;		/* all edges in a contiguous array */
  dr_pi_string_table * S;
} dr_pi_dag;


/*-----------------DV DAG Visualizer Structures-----------------*/

typedef struct dv_grid_line {
	double c;  /* coordinate */
	int lv;
	struct dv_grid_line * l;  /* left next grid line */
	struct dv_grid_line * r;  /* right next grid line */
} dv_grid_line_t;

typedef struct dv_graph_node {
	long idx;
	dr_dag_node_info * info;
	int ek; /* edge kind */
	union {
		/* create */
		struct {
			struct dv_graph_node * el;  /* edge left */
			struct dv_graph_node * er;  /* edge right */
		};
		/* wait_cont, end_task */
		struct dv_graph_node * ej;  /* edge join */
	};
	
	dv_grid_line_t * vl;  /* vertical line */
	dv_grid_line_t * hl;  /* horizontal line */
} dv_graph_node_t;

typedef struct dv_linked_list {
	void * item;
	struct dv_linked_list * next;
} dv_linked_list_t;

typedef struct dv_graph {
	long n;               /* num of nodes on the visual graph */
  long num_workers;		  /* num of workers */
	dv_graph_node_t * T;  /* all nodes in a contiguous array */
	dv_graph_node_t * root_node;  /* root node of the graph */
  dr_pi_string_table * S;	 /* string table */
	
	dv_grid_line_t * root_vl;  /* vertical line */
	dv_grid_line_t * root_hl;  /* horizontal line */
	double zoom_ratio;  /* zoom ratio of the graph to draw */
	double width, height;  /* viewport's size */
	double x, y;        /* current coordinates of the central point */
	dv_linked_list_t itl; /* list of nodes that have info tag */
} dv_graph_t;

typedef struct dv_status {
	char drag_on;
	double pressx, pressy;
	int nc; /* node color: 0->worker, 1->cpu, 2->kind, 3->last */
} dv_status_t;

/*-----------------DV DAG Visualizer Inlines-----------------*/

#define DV_ZOOM_INCREMENT 1.25
#define DV_HDIS 70
#define DV_VDIS 100
#define DV_RADIUS 30

static int dv_check_(int condition, const char * condition_s, 
										 const char * __file__, int __line__, 
										 const char * func) {
	if (!condition) {
		fprintf(stderr, "%s:%d:%s: check failed : %s\n", 
						__file__, __line__, func, condition_s);
		exit(1);
	}
	return 1;
}

#define dv_check(x) (dv_check_(((x)?1:0), #x, __FILE__, __LINE__, __func__))


/*-----------------DV DAG Visualizer Headers-----------------*/

#define NUM_COLORS 34
const char * COLORS[NUM_COLORS] =
	{"orange", "gold", "cyan", "azure", "green",
	 "magenta", "brown1", "burlywood1", "peachpuff", "aquamarine",
	 "chartreuse", "skyblue", "burlywood", "cadetblue", "chocolate",
	 "coral", "cornflowerblue", "cornsilk4", "darkolivegreen1", "darkorange1",
	 "khaki3", "lavenderblush2", "lemonchiffon1", "lightblue1", "lightcyan",
	 "lightgoldenrod", "lightgoldenrodyellow", "lightpink2", "lightsalmon2", "lightskyblue1",
	 "lightsteelblue3", "lightyellow3", "maroon1", "yellowgreen"};
