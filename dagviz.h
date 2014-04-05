#ifndef DAGVIZ_HEADER_
#define DAGVIZ_HEADER_

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

#include <dag_recorder_impl.h>


/*-----Utilities-----*/
typedef struct dv_linked_list {
	void * item;
	struct dv_linked_list * next;
} dv_linked_list_t;

typedef struct dv_stack_cell {
	void * item;
	struct dv_stack_cell * next;
} dv_stack_cell_t;

typedef struct dv_stack {
	dv_stack_cell_t * freelist;
	dv_stack_cell_t * top;
} dv_stack_t;

typedef struct dv_llist_cell {
	void * item;
	struct dv_llist_cell * next;
} dv_llist_cell_t;

typedef struct dv_llist {
	int i;
	dv_llist_cell_t * head;
} dv_llist_t;


/*-----------------Constants-----------------*/

#define DV_ZOOM_INCREMENT 1.25
#define DV_HDIS 70
#define DV_VDIS 100
#define DV_RADIUS 30
#define NUM_COLORS 34

#define DV_NODE_STATE_NONE     0
#define DV_NODE_STATE_UNION    1
#define DV_NODE_STATE_EXPANDED (1 << 1)
#define DV_NODE_STATE_MOVING   (1 << 2)

#define DV_ZOOM_TO_FIT_MARGIN 20


/*-----------------Data Structures-----------------*/

typedef struct dv_status {
	char drag_on; /* currently dragged or not */
	double pressx, pressy; /* currently pressed position */
	int nc; /* node color: 0->worker, 1->cpu, 2->kind, 3->last */
} dv_status_t;

typedef struct dv_grid_line {
	struct dv_grid_line * l;  /* left next grid line */
	struct dv_grid_line * r;  /* right next grid line */
	dv_llist_t L[1]; /* list of assigned nodes */
	double lc;    /* left count */
	double rc;    /* right count */
	double c;  /* coordinate */
} dv_grid_line_t;

typedef struct dv_grid {
	dv_grid_line_t vl[1];
	dv_grid_line_t hl[1];
} dv_grid_t;

typedef struct dv_dag_node {
	
	/* data */
	dr_pi_dag_node * pi;
	/* node status */
	char s; /* 0x0: single, 0x01: union/collapsed, 0x11: union/expanded  */

	/* outward topology */
	dv_llist_t links[1]; /* linked nodes */
	dv_grid_line_t * vl;  /* vertical line of outer grid */
	dv_grid_line_t * hl;  /* horizontal line of outer grid */

	/* inward topology */
	dv_grid_t grid[1]; /* inner grid */
	dv_llist_t heads[1]; /* list of inner head nodes */
	dv_llist_t tails[1]; /* list of inner tail nodes */

	double dc;
	double c;
	
} dv_dag_node_t;


typedef struct dv_dag {
	/* data */
	long n;   /* number of nodes */
  long nw;		  /* number of workers */
  dr_pi_string_table * st;	 /* string table */

	/* topology */
	dv_dag_node_t * T;  /* array of all nodes */
	dv_dag_node_t * rt;  /* root task */
	dv_grid_t grid[1];  /* root grid */

	/* drawing parameters */
	char init;     /* to recognize initial drawing */
	double zoom_ratio;  /* zoom ratio of the graph to draw */
	double x, y;        /* current coordinates of the central point */
	double width, height;  /* viewport's size */
	double basex, basey;
	dv_llist_t itl[1]; /* list of nodes that have info tag */
} dv_dag_t;


/*------Global variables-----*/

extern const char * const NODE_KIND_NAMES[];
extern const char * const EDGE_KIND_NAMES[];
extern const char * const DV_COLORS[];

extern dv_status_t S[];
extern dr_pi_dag P[];
extern dv_dag_t G[];
extern GtkWidget *window;
extern GtkWidget *darea;
extern dv_llist_cell_t *FL;

/*-----------------Headers-----------------*/

/* print.c */
void print_pi_dag_node(dr_pi_dag_node *, int);
void print_pi_dag_edge(dr_pi_dag_edge *, int);
void print_pi_string_table(dr_pi_string_table *, int);
void print_dvdag(dv_dag_t *);
void print_layout(dv_dag_t *);
void print_dag_file(char *);
void check_layout(dv_dag_t *);

/* layout.c */
void dv_grid_line_init(dv_grid_line_t *);
dv_grid_line_t * dv_grid_line_create();
void dv_grid_init(dv_grid_t *);
dv_grid_t * dv_grid_create();
void dv_read_dag_file_to_pidag(char *, dr_pi_dag *);
void dv_convert_pidag_to_dvdag(dr_pi_dag *, dv_dag_t *);
void dv_layout_dvdag(dv_dag_t *);

/* draw.c */
void draw_dvdag(cairo_t *, dv_dag_t *);

/* utils.c */
void dv_stack_init(dv_stack_t *);
void dv_stack_fini(dv_stack_t *);
void dv_stack_push(dv_stack_t *, void *);
void * dv_stack_pop(dv_stack_t *);

dv_linked_list_t * dv_linked_list_create();
void dv_linked_list_destroy(dv_linked_list_t *);
void dv_linked_list_init(dv_linked_list_t *);
void * dv_linked_list_remove(dv_linked_list_t *, void *);
void dv_linked_list_add(dv_linked_list_t *, void *);
	
void dv_llist_init(dv_llist_t *);
void dv_llist_fini(dv_llist_t *);
dv_llist_t * dv_llist_create();
void dv_llist_destroy(dv_llist_t *);
dv_llist_cell_t * dv_llist_ensure_freelist();
void dv_llist_add(dv_llist_t *, void *);
void * dv_llist_get(dv_llist_t *);
void * dv_llist_remove(dv_llist_t *, void *);
void dv_llist_iterate_init(dv_llist_t *);
void * dv_llist_iterate_next(dv_llist_t *);


/*-----------------Inlines-----------------*/

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

static void * dv_malloc(size_t sz) {
	void * a = malloc(sz);
	dv_check(a);
	return a;
}

static void dv_free(void * a, size_t sz) {
	if (a) {
		free(a);
	} else {
		dv_check(sz == 0);
	}
}

static int dv_node_state_check(char s, char t) {
	int ret = ((s & t) == t);
	//printf("node_state_check: %d, %d = %d\n", s, t, ret);
	return ret;
}


#endif /* DAGVIZ_HEADER_ */
