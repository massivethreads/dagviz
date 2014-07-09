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
#include <sys/time.h>

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
  dv_llist_cell_t * top;
} dv_llist_t;


/*-----------------Constants-----------------*/

#define DV_ZOOM_INCREMENT 1.25
#define DV_HDIS 70
#define DV_VDIS 100
#define DV_RADIUS 20
#define NUM_COLORS 34

#define DV_NODE_FLAG_NONE      0
#define DV_NODE_FLAG_UNION     1
#define DV_NODE_FLAG_SHRINKED  (1 << 1)
#define DV_NODE_FLAG_EXPANDING (1 << 2)
#define DV_NODE_FLAG_SHRINKING (1 << 3)

#define DV_ZOOM_TO_FIT_MARGIN 20
#define DV_STRING_LENGTH 100
#define DV_STATUS_PADDING 7
#define DV_SAFE_CLICK_RANGE 1
#define DV_UNION_NODE_MARGIN 4
#define DV_NODE_LINE_WIDTH 1.0
#define DV_EDGE_LINE_WIDTH 1.0
#define DV_RADIX_LOG 1.8
#define DV_RADIX_POWER 0.42
#define DV_RADIX_LINEAR 100000

#define DV_ANIMATION_DURATION 400 // milliseconds
#define DV_ANIMATION_STEP 30 // milliseconds

#define DV_LAYOUT_TYPE_INIT 0
#define DV_NODE_COLOR_INIT 0
#define DV_SCALE_TYPE_INIT 1
#define DV_FROMBT_INIT 0
#define DV_EDGE_TYPE_INIT 3
#define DV_EDGE_AFFIX_LENGTH 10
#define DV_CLICK_MODE_INIT 1

#define DV_COLOR_POOL_SIZE 100
#define DV_NUM_COLOR_POOLS 6

#define DV_TIMELINE_NODE_WITH_BORDER 1
#define DV_ENTRY_RADIX_MAX_LENGTH 20

/*--Grid-like layout--*/

typedef struct dv_grid_line {
  struct dv_grid_line * l;  /* left next grid line */
  struct dv_grid_line * r;  /* right next grid line */
  dv_llist_t L[1]; /* list of assigned nodes */
  double lc;    /* left count */
  double rc;    /* right count */
  double c;  /* coordinate */
} dv_grid_line_t;

/*--end of Grid-like layout--*/

/*-----------------Data Structures-----------------*/

typedef struct dv_animation {
  int on; /* on/off */
  double duration; /* milliseconds */
  double step; /* milliseconds */
  dv_llist_t movings[1]; /* list of moving nodes */
} dv_animation_t;

typedef struct dv_status {
  // Drag animation
  char drag_on; /* currently dragged or not */
  double pressx, pressy; /* currently pressed position */
  double accdisx, accdisy; /* accumulated dragged distance */
  // Node color
  int nc; /* node color: 0->worker, 1->cpu, 2->kind, 3->last */
  // Window's size
  double vpw, vph;  /* viewport's size */
  // Shrink/Expand animation
  dv_animation_t a[1]; /* animation struct */
  long nd; /* number of nodes drawn */
  int lt; /* layout type */
  int sdt; /* scale down type */
  double log_radix;
  double power_radix;
  double linear_radix;
  int frombt;
  // Color pool
  int CP[DV_NUM_COLOR_POOLS][DV_COLOR_POOL_SIZE][4]; // worker, cpu, nodekind, cp1, cp2, cp3
  int CP_sizes[DV_NUM_COLOR_POOLS];
  int et; /* edge type */
  int edge_affix; /* edge affix length */
  int cm; /* click mode */
} dv_status_t;

typedef struct dv_dag_node {
  
  /* task-parallel data */
  //dr_pi_dag_node * pi;
  long pii;

  /* state data */  
  char f[1]; /* node flags, 0x0: single, 0x01: union/collapsed, 0x11: union/expanded */
  int d; /* depth */
  
  /* linking structure */
  struct dv_dag_node * parent;
  struct dv_dag_node * pre;
  dv_llist_t links[1]; /* linked nodes */
  struct dv_dag_node * head; /* inner head node */
  dv_llist_t tails[1]; /* list of inner tail nodes */

  /* bbox/timeline layouts */
  double x, y; /* coordinates */
  double xp, xpre; /* coordinates based on parent, pre */
  double lw, rw, dw; /* left/right/down widths */
  double link_lw, link_rw, link_dw;
  int avoid_inward;

  /* grid-like layout */
  dv_grid_line_t * vl;  /* vertical line of outer grid */
  dv_grid_line_t * vl_in;  /* vertical line of inner grid */
  double lc, rc, dc; /* left/right/down counts */
  double c; /* coordinate */

  /* animation */
  double started; /* started time of animation */
  
} dv_dag_node_t;

typedef struct dv_dag {
  /* general data */
  long n;   /* number of nodes */
  long nw;      /* number of workers */
  dr_pi_string_table * st;   /* string table */

  /* DAG data */
  dv_dag_node_t * T;  /* array of all nodes */
  dv_dag_node_t * rt;  /* root task */
  int dmax; /* depth max */
  double bt; /* begin time */
  double et; /* end time */

  /* drawing parameters */
  char init;     /* to recognize initial drawing */
  double zoom_ratio;  /* zoom ratio of the graph to draw */
  double x, y;        /* current coordinates of the central point */
  double basex, basey;
  dv_llist_t itl[1]; /* list of nodes that have info tag */

  /* grid-like layout */
  dv_grid_line_t rvl[1]; /* root vl */

  /* layout status */
  int cur_d; /* current depth */
  int cur_d_ex; /* current depth of extensible union nodes */

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

/* read.c */
dr_pi_dag_node * dv_pidag_get_node(dv_dag_node_t *);
void dv_read_dag_file_to_pidag(char *, dr_pi_dag *);
void dv_convert_pidag_to_dvdag(dr_pi_dag *, dv_dag_t *);

/* layout.c */
double dv_layout_calculate_gap(dv_dag_node_t *);
double dv_layout_calculate_reverse_ratio(dv_dag_node_t *);
void dv_layout_dvdag(dv_dag_t *);
double dv_get_time();
void dv_animation_init(dv_animation_t *);
void dv_animation_start(dv_animation_t *);
void dv_animation_stop(dv_animation_t *);

/* draw.c */
void dv_draw_text(cairo_t *);
void dv_draw_rounded_rectangle(cairo_t *, double, double, double, double);
void dv_lookup_color(dv_dag_node_t *, double *, double *, double *, double *);
double dv_get_alpha_fading_out(dv_dag_node_t *);
double dv_get_alpha_fading_in(dv_dag_node_t *);
void draw_edge_1(cairo_t *, dv_dag_node_t *, dv_dag_node_t *);
void dv_draw_status(cairo_t *);
void dv_draw_infotags(cairo_t *, dv_dag_t *);
void dv_draw_dvdag(cairo_t *, dv_dag_t *);

/* view_glike.c */
void dv_layout_glike_dvdag(dv_dag_t *);
void dv_draw_glike_dvdag(cairo_t *, dv_dag_t *);

/* view_bbox.c */
double dv_layout_calculate_hsize(dv_dag_node_t *);
double dv_layout_calculate_vresize(double);
double dv_layout_calculate_vsize(dv_dag_node_t *);
void dv_layout_bbox_dvdag(dv_dag_t *);
void dv_draw_bbox_dvdag(cairo_t *, dv_dag_t *);

/* view_timeline.c */
void dv_layout_timeline_dvdag(dv_dag_t *);
void dv_draw_timeline_dvdag(cairo_t *, dv_dag_t *);

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
int dv_llist_is_empty(dv_llist_t *);
dv_llist_cell_t * dv_llist_ensure_freelist();
void dv_llist_add(dv_llist_t *, void *);
void * dv_llist_get(dv_llist_t *);
void * dv_llist_remove(dv_llist_t *, void *);
void dv_llist_iterate_init(dv_llist_t *);
void * dv_llist_iterate_next(dv_llist_t *);
int dv_llist_size(dv_llist_t *);

const char * dv_convert_char_to_binary(int );
double dv_max(double, double);


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

static void dv_node_flag_init(char *f) {
  *f = DV_NODE_FLAG_NONE;
}

static int dv_node_flag_check(char *f, char t) {
  int ret = ((*f & t) == t);
  return ret;
}

static void dv_node_flag_set(char *f, char t) {
  if (!dv_node_flag_check(f,t))
    *f += t;
}

static int dv_node_flag_remove(char *f, char t) {
  if (dv_node_flag_check(f,t))
    *f -= t;
}

static int dv_is_union(dv_dag_node_t *node) {
  if (!node) return 0;
  return dv_node_flag_check(node->f, DV_NODE_FLAG_UNION);
}

static int dv_is_shrinked(dv_dag_node_t *node) {
  if (!node) return 0;
  return dv_node_flag_check(node->f, DV_NODE_FLAG_SHRINKED);
}

static int dv_is_expanded(dv_dag_node_t *node) {
  if (!node) return 0;
  return !dv_is_shrinked(node);
}

static int dv_is_expanding(dv_dag_node_t *node) {
  if (!node) return 0;
  return dv_node_flag_check(node->f, DV_NODE_FLAG_EXPANDING);
}

static int dv_is_shrinking(dv_dag_node_t *node) {
  if (!node) return 0;
  return dv_node_flag_check(node->f, DV_NODE_FLAG_SHRINKING);
}

static int dv_is_single(dv_dag_node_t *node) {
  if (!node) return 0;
  return (!dv_is_union(node) || (dv_is_shrinked(node) && !dv_is_expanding(node)));
}

static int dv_is_visible(dv_dag_node_t *node) {
  if (!node) return 0;
  if (!node->parent || dv_is_expanded(node->parent)) {
    if (!dv_is_union(node) || dv_is_shrinked(node))
      return 1;
  }
  return 0;
}

#endif /* DAGVIZ_HEADER_ */
