#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <errno.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h> /* for isdigit() */

#define DAG_RECORDER 2
#include <dag_recorder_impl.h>

#define GtkWidget void

#define _unused_ __attribute__((unused))
#define _static_unused_ static __attribute__((unused))

#define DV_MAX_DAG_FILE 1000
#define DV_MAX_DAG 1000
#define DV_MAX_VIEW 1000

#define DV_NUM_LAYOUT_TYPES 6
#define DV_LAYOUT_TYPE_DAG 0
#define DV_LAYOUT_TYPE_DAG_BOX 1
#define DV_LAYOUT_TYPE_TIMELINE 3
#define DV_LAYOUT_TYPE_TIMELINE_VER 2
#define DV_LAYOUT_TYPE_PARAPROF 4
#define DV_LAYOUT_TYPE_CRITICAL_PATH 5
#define DV_LAYOUT_TYPE_INIT 0 // not paraprof coz need to check H of D

#define DV_NUM_CRITICAL_PATHS 3
#define DV_CRITICAL_PATH_0 0 /* most work */
#define DV_CRITICAL_PATH_1 1 /* last finished tails */
#define DV_CRITICAL_PATH_2 2 /* most problematic delay */
#define DV_CRITICAL_PATH_0_COLOR "red"
#define DV_CRITICAL_PATH_1_COLOR "green"
#define DV_CRITICAL_PATH_2_COLOR "blue"

#define DV_MAX_NUM_REMARKS 32

#define DV_DEFAULT_PAGE_SIZE 1 << 20 // 1 MB
#define DV_DEFAULT_PAGE_MIN_NODES 2
#define DV_DEFAULT_PAGE_MIN_ENTRIES 2

#define DV_NODE_FLAG_NONE         0        /* none */
#define DV_NODE_FLAG_SET          1        /* none - set */
#define DV_NODE_FLAG_UNION        (1 << 1) /* single - union */
#define DV_NODE_FLAG_INNER_LOADED (1 << 2) /* no inner - inner loaded */
#define DV_NODE_FLAG_SHRINKED     (1 << 3) /* expanded - shrinked */
#define DV_NODE_FLAG_EXPANDING    (1 << 4) /* expanding */
#define DV_NODE_FLAG_SHRINKING    (1 << 5) /* shrinking */

#define DV_OK 0
#define DV_ERROR_OONP 1 /* out of node pool */

#define DV_STACK_CELL_SZ 1
#define DV_LLIST_CELL_SZ 1

#define DV_NODE_FLAG_CRITICAL_PATH_0 (1 << 6) /* node is on critical path of work */
#define DV_NODE_FLAG_CRITICAL_PATH_1 (1 << 7) /* node is on critical path of work & delay */
#define DV_NODE_FLAG_CRITICAL_PATH_2 (1 << 8) /* node is on critical path of weighted work & delay */

#define DV_NODE_COLOR_INIT 0 // 0:worker, 1:cpu, 2:kind, 3:code start, 4:code end, 5: code segment
#define DV_SCALE_TYPE_INIT 0 // 0:log, 1:power, 2:linear
#define DV_RADIX_LOG 1.8
#define DV_RADIX_POWER 0.42
#define DV_RADIX_LINEAR 100000 //100000

#define DV_FROMBT_INIT 0
#define DV_COLOR_POOL_SIZE 100
#define DV_NUM_COLOR_POOLS 6

#define DV_PARAPROF_MIN_ENTRY_INTERVAL 2000


#if 0
#define NUM_COLORS 34

#define DV_STRING_LENGTH 1000
#define DV_STATUS_PADDING 7
#define DV_SAFE_CLICK_RANGE 1

#define DV_ANIMATION_DURATION 400 // milliseconds
#define DV_ANIMATION_STEP 80 // milliseconds

#define DV_EDGE_TYPE_INIT 3
#define DV_EDGE_AFFIX_LENGTH 0//10
#define DV_CLICK_MODE_INIT 2 // 0:none, 1:info, 2:expand/collapse
#define DV_HOVER_MODE_INIT 6 // 0:none, 1:info, 2:expand, 3:collapse, 4:expand/collapse, 5:remark similar nodes, 6:highlight
#define DV_SHOW_LEGEND_INIT 0
#define DV_SHOW_STATUS_INIT 0
#define DV_REMAIN_INNER_INIT 1
#define DV_COLOR_REMARKED_ONLY 1

#define DV_TIMELINE_NODE_WITH_BORDER 0
#define DV_ENTRY_RADIX_MAX_LENGTH 20

#define DV_DAG_NODE_POOL_SIZE 50000

#define DV_MAX_VIEWPORT 100

#define backtrace_sample_sz 15

#define DV_HISTOGRAM_PIECE_POOL_SIZE 100000
#define DV_HISTOGRAM_ENTRY_POOL_SIZE 40000

#define DV_HISTOGRAM_DIVIDE_TO_PIECES 0

#define DV_CAIRO_BOUND_MIN -8e6 //-(1<<23)
#define DV_CAIRO_BOUND_MAX 8e6 //(1<<23)

#define DV_MAX_DISTRIBUTION 10

#define DV_STAT_DISTRIBUTION_OUTPUT_DEFAULT_NAME "00dv_stat_distribution.gpl"
#define DV_STAT_BREAKDOWN_OUTPUT_DEFAULT_NAME "00dv_stat_breakdown.gpl"
#define DV_STAT_BREAKDOWN_OUTPUT_DEFAULT_NAME_2 "00dv_stat_critical_path_breakdown.gpl"

#define DV_VIEW_AUTO_ZOOMFIT_INIT 4 /* 0:none, 1:hor, 2:ver, 3:based, 4:full */
#define DV_VIEW_ADJUST_AUTO_ZOOMFIT_INIT 1

#define _unused_ __attribute__((unused))
#define _static_unused_ static __attribute__((unused))

#define DV_VERBOSE_LEVEL_DEFAULT 0

#define DV_DAG_NODE_HIDDEN_ABOVE 0b0001
#define DV_DAG_NODE_HIDDEN_RIGHT 0b0010
#define DV_DAG_NODE_HIDDEN_BELOW 0b0100
#define DV_DAG_NODE_HIDDEN_LEFT  0b1000

#endif


/* Stack & linked list */

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
  int sz;
  dv_llist_cell_t * top;
} dv_llist_t;


/* DAG structures */

typedef struct dv_pidag {
  char * fn; /* original dag file name string */
  char * filename; /* dag file name */
  char * short_filename; /* short dag file name excluding dir and extension */
  long n;			/* length of T */
  long m;			/* length of E */
  unsigned long long start_clock;             /* absolute clock time of start */
  long num_workers;		/* number of workers */
  dr_pi_dag_node * T;		/* all nodes in a contiguous array */
  dr_pi_dag_edge * E;		/* all edges in a contiguous array */
  dr_pi_string_table * S;
  //struct stat stat[1]; /* file stat structure */
  dv_llist_t itl[1]; /* list of pii's of nodes that have info tag */
  dr_pi_dag * G;
  long sz;

  /* DAG management window */
  char * name;
  GtkWidget * mini_frame;
} dv_pidag_t;

typedef struct dv_node_coordinate {
  double x, y; /* coordinates */
  double xp, xpre; /* coordinates based on parent, pre */
  double lw, rw, dw; /* left/right/down widths */
  double link_lw, link_rw, link_dw;
} dv_node_coordinate_t;

typedef struct dv_critical_path_stat {
  double work;
  double delay;
  double sched_delay;
  double sched_delays[dr_dag_edge_kind_max];
  double sched_delay_nowork; /* total no-work during scheduler delays */
  double sched_delay_delay; /* total delay during scheduler delays */
} dv_critical_path_stat_t;

typedef struct dv_dag_node {
  /* task-parallel data */
  long pii;

  /* state data */  
  int f[1]; /* node flags, 0x0: single, 0x01: union/collapsed, 0x11: union/expanded */
  int d; /* depth */
  
  /* linking structure */
  struct dv_dag_node * parent;
  struct dv_dag_node * pre;
  struct dv_dag_node * next; /* is used for linking in pool before node is really initialized */
  struct dv_dag_node * spawn;
  struct dv_dag_node * head; /* inner head node */
  struct dv_dag_node * tail; /* inner tail node */

  /* layout */
  dv_node_coordinate_t c[DV_NUM_LAYOUT_TYPES]; /* 0:dag, 1:dagbox, 2:timeline_ver, 3:timeline, 4:paraprof */

  /* animation */
  double started; /* started time of animation */

  long long r; /* remarks */
  long long link_r; /* summed remarks with linked paths */

  char highlight; /* to highlight the node when drawing */

  /* statistics of inner subgraphs */
  dv_critical_path_stat_t cpss[DV_NUM_CRITICAL_PATHS];
} dv_dag_node_t;

typedef struct dv_histogram dv_histogram_t;

typedef struct dv_dag {
  char * name;
  char * name_on_graph;
  /* PIDAG */
  dv_pidag_t * P;

  /* DAG's skeleton */
  //dv_dag_node_t * T;  /* array of all nodes */
  //char * To;
  //long Tsz;
  //long Tn;

  /* DAG */
  dv_dag_node_t * rt;  /* root task */
  int dmax; /* depth max */
  double bt; /* begin time */
  double et; /* end time */
  long n;

  /* expansion state */
  int cur_d; /* current depth */
  int cur_d_ex; /* current depth of extensible union nodes */
  int collapsing_d; /* current depth excluding children of collapsing nodes */

  /* layout parameters */
  int sdt; /* scale down type: 0 (log), 1 (power), 2 (linear) */
  double log_radix;
  double power_radix;
  double linear_radix;
  int frombt;
  double radius;
  int mV[DV_MAX_VIEW]; /* mark Vs that are bound to this D */

  /* other */
  dv_llist_t itl[1]; /* list of nodes that have info tag */
  dv_histogram_t * H; /* structure for the paraprof view (5th) */
  int nviews[DV_NUM_LAYOUT_TYPES]; /* num of views referencing each layout type */

  int ar[DV_MAX_NUM_REMARKS];
  int nr;

  /* DAG management window */
  GtkWidget * mini_frame;
  GtkWidget * views_box;
  GtkWidget * status_label;

  int draw_with_current_time;
  double current_time;
  double time_step;
  int show_critical_paths[DV_NUM_CRITICAL_PATHS];
  int critical_paths_computed;
} dv_dag_t;


/* Histogram structures */

typedef enum {
  dv_histogram_layer_running,
  dv_histogram_layer_ready_end,
  dv_histogram_layer_ready_create,
  dv_histogram_layer_ready_create_cont,
  dv_histogram_layer_ready_wait_cont,
  dv_histogram_layer_ready_other_cont,
  dv_histogram_layer_max,
} dv_histogram_layer_t;

typedef struct dv_histogram_entry {
  double t;
  struct dv_histogram_entry * next;
  double h[dv_histogram_layer_max];
  double value_1;
  double cumul_value_1;
  double value_2;
  double cumul_value_2;
  double value_3;
  double cumul_value_3;
} dv_histogram_entry_t;

typedef struct dv_histogram {
  dv_histogram_entry_t * head_e;
  dv_histogram_entry_t * tail_e;
  long n_e;
  dv_dag_t * D;
  double work, delay, nowork;
  double min_entry_interval;
  double unit_thick;
  dv_histogram_entry_t * tallest_e;
  double max_h;
} dv_histogram_t;


/* Memory pool structures */

typedef struct dv_histogram_entry_page {
  struct dv_histogram_entry_page * next;
  long sz;
  dv_histogram_entry_t entries[DV_DEFAULT_PAGE_MIN_ENTRIES];
} dv_histogram_entry_page_t;

typedef struct dv_histogram_entry_pool {
  dv_histogram_entry_t * head;
  dv_histogram_entry_t * tail;
  dv_histogram_entry_page_t * pages;
  long sz; /* size in bytes */
  long N;  /* total number of entries */
  long n;  /* number of free entries */
} dv_histogram_entry_pool_t;


typedef struct dv_dag_node_page {
  struct dv_dag_node_page * next;
  long sz;
  dv_dag_node_t nodes[DV_DEFAULT_PAGE_MIN_NODES];
} dv_dag_node_page_t;

typedef struct dv_dag_node_pool {
  dv_dag_node_t * head;
  dv_dag_node_t * tail;
  dv_dag_node_page_t * pages;
  long sz; /* size in bytes */
  long N;  /* total number of nodes */
  long n;  /* number of free nodes */
} dv_dag_node_pool_t;


/* Global state structure */
typedef long long dv_clock_t;

typedef struct dv_stat_breakdown_graph {
  int checked_D[DV_MAX_DAG]; /* show or not show */
  char * fn;
  dv_clock_t work[DV_MAX_DAG];
  dv_clock_t delay[DV_MAX_DAG];
  dv_clock_t nowork[DV_MAX_DAG];
  char * fn_2;
  int checked_cp[DV_NUM_CRITICAL_PATHS];
} dv_stat_breakdown_graph_t;

/* runtime-adjustable options */
typedef struct dv_options {
  double zoom_step_ratio;
  double scale_step_ratio;
  
  double hnd; /* horizontal node distance */
  double vnd; /* vertical node distance */
  double radius; /* node radius */
  double vp_mark_radius; /* radius of viewport's mark */
  double nlw; /* node line width */
  double nlw_collective_node_factor; /* node line width's multiplying factor for collective nodes */

  double ruler_width;
  double zoom_to_fit_margin;
  double zoom_to_fit_margin_bottom;
  double paraprof_zoom_to_fit_margin;
  double paraprof_zoom_to_fit_margin_bottom;
  double union_node_puffing_margin;
  double clipping_frame_margin;
} dv_options_t;

/* default values for runtime-adjustable options */
_static_unused_ dv_options_t dv_options_default_values = {
  1.06, /* zoom_step_ratio */
  1.04, /* scale_step_ratio */

  70.0, /* hnd: horizontal node distance */
  70.0, /* vnd: vertical node distance */
  20.0, /* radius */
  5.0,  /* vp_mark_radius */
  1.5,  /* nlw: node line width */
  2.0,  /* nlw_collective_node_factor */
  
  15.0, /* ruler_width */
  30.0, /* zoom_to_fit_margin */
  15.0, /* zoom_to_fit_margin_bottom */
  30.0, /* paraprof_zoom_to_fit_margin */
  30.0, /* paraprof_zoom_to_fit_margin_bottom */
  6.0,  /* union_node_puffing_margin */
  0.0,  /* clipping_frame_margin */
};

typedef struct dv_global_state {
  /* DAG */
  dv_pidag_t P[DV_MAX_DAG_FILE];
  dv_dag_t D[DV_MAX_DAG];
  int nP;
  int nD;
  dv_llist_cell_t * FL;
  int err;
  
  /* Color pools */
  int CP[DV_NUM_COLOR_POOLS][DV_COLOR_POOL_SIZE][4]; // worker, cpu, nodekind, cp1, cp2, cp3
  int CP_sizes[DV_NUM_COLOR_POOLS];
  
  /* Memory Pools */
  dv_dag_node_pool_t pool[1];
  dv_histogram_entry_pool_t epool[1];

  /* Statistics */
  dv_stat_breakdown_graph_t SBG[1];

  int verbose_level;

  int oncp_flags[DV_NUM_CRITICAL_PATHS];
  char * cp_colors[DV_NUM_CRITICAL_PATHS];

  dv_options_t opts;
} dv_global_state_t;

extern dv_global_state_t CS[]; /* global common state */


/* PIDAG */
dv_pidag_t *     dv_pidag_read_new_file(char *);
dr_pi_dag_node * dv_pidag_get_node_by_id(dv_pidag_t *, long);
dr_pi_dag_node * dv_pidag_get_node_by_dag_node(dv_pidag_t *, dv_dag_node_t *);

/* DAG */
void dv_dag_node_init(dv_dag_node_t *, dv_dag_node_t *, long);
int dv_dag_node_set(dv_dag_t *, dv_dag_node_t *);
int dv_dag_build_node_inner(dv_dag_t *, dv_dag_node_t *);
int dv_dag_collapse_node_inner(dv_dag_t *, dv_dag_node_t *);
void dv_dag_clear_shrinked_nodes(dv_dag_t *);
void dv_dag_init(dv_dag_t *, dv_pidag_t *);
dv_dag_t * dv_dag_create_new_with_pidag(dv_pidag_t *);
double dv_dag_get_radix(dv_dag_t *);
void dv_dag_set_radix(dv_dag_t *, double);
int dv_pidag_node_lookup_value(dr_pi_dag_node *, int);
int dv_dag_node_lookup_value(dv_dag_t *, dv_dag_node_t *, int);

dv_dag_node_t * dv_dag_node_traverse_children(dv_dag_node_t *, dv_dag_node_t *);
dv_dag_node_t * dv_dag_node_traverse_children_inorder(dv_dag_node_t *, dv_dag_node_t *);
dv_dag_node_t * dv_dag_node_traverse_tails(dv_dag_node_t *, dv_dag_node_t *);
dv_dag_node_t * dv_dag_node_traverse_nexts(dv_dag_node_t *, dv_dag_node_t *);

int dv_dag_node_count_nexts(dv_dag_node_t *);
dv_dag_node_t * dv_dag_node_get_next(dv_dag_node_t *);
dv_dag_node_t * dv_dag_node_get_single_head(dv_dag_node_t *);
dv_dag_node_t * dv_dag_node_get_single_last(dv_dag_node_t *);

/* Memory pools */
void dv_dag_node_pool_init(dv_dag_node_pool_t *);
dv_dag_node_t * dv_dag_node_pool_pop(dv_dag_node_pool_t *);
void dv_dag_node_pool_push(dv_dag_node_pool_t *, dv_dag_node_t *);
dv_dag_node_t * dv_dag_node_pool_pop_contiguous(dv_dag_node_pool_t *, long);

void dv_histogram_entry_pool_init(dv_histogram_entry_pool_t *);
dv_histogram_entry_t * dv_histogram_entry_pool_pop(dv_histogram_entry_pool_t *);
void dv_histogram_entry_pool_push(dv_histogram_entry_pool_t *, dv_histogram_entry_t *);

/* Basic data structures */
void dv_stack_init(dv_stack_t *);
void dv_stack_fini(dv_stack_t *);
void dv_stack_push(dv_stack_t *, void *);
void * dv_stack_pop(dv_stack_t *);

void dv_llist_init(dv_llist_t *);
void dv_llist_fini(dv_llist_t *);
dv_llist_t * dv_llist_create();
void dv_llist_destroy(dv_llist_t *);
int dv_llist_is_empty(dv_llist_t *);
void dv_llist_add(dv_llist_t *, void *);
void * dv_llist_pop(dv_llist_t *);
void * dv_llist_get(dv_llist_t *, int);
void * dv_llist_get_top(dv_llist_t *);
void * dv_llist_remove(dv_llist_t *, void *);
int dv_llist_has(dv_llist_t *, void *);
void * dv_llist_iterate_next(dv_llist_t *, void *);
int dv_llist_size(dv_llist_t *);

/* Histogram */
void dv_histogram_init(dv_histogram_t *);
double dv_histogram_get_max_height(dv_histogram_t *);
dv_histogram_entry_t * dv_histogram_insert_entry(dv_histogram_t *, double, dv_histogram_entry_t *);
void dv_histogram_add_node(dv_histogram_t *, dv_dag_node_t *, dv_histogram_entry_t **);
void dv_histogram_remove_node(dv_histogram_t *, dv_dag_node_t *, dv_histogram_entry_t **);
void dv_histogram_clean(dv_histogram_t *);
void dv_histogram_fini(dv_histogram_t *);
void dv_histogram_reset(dv_histogram_t *);
void dv_histogram_build_all(dv_histogram_t *);
void dv_histogram_compute_significant_intervals(dv_histogram_t *);

/* Compute */
char * dv_filename_get_short_name(char *);
void dv_global_state_init(dv_global_state_t *);
void dv_dag_build_inner_all(dv_dag_t *);
void dv_dag_compute_critical_paths(dv_dag_t *);

int dm_get_dag_id(char *);
dv_dag_t * dm_compute_dag_file(char *);




/***** Utilities *****/

_static_unused_ int
dv_check_(int condition, const char * condition_s, 
                     const char * __file__, int __line__, 
                     const char * func) {
  if (!condition) {
    fprintf(stderr, "%s:%d:%s: check failed : %s\n", 
            __file__, __line__, func, condition_s);
    //dv_get_callpath_by_backtrace();
    //dv_get_callpath_by_libunwind();
    exit(1);
  }
  return 1;
}

#define dv_check(x) (dv_check_(((x)?1:0), #x, __FILE__, __LINE__, __func__))

_static_unused_ void *
dv_malloc(size_t sz) {
  void * a = malloc(sz);
  dv_check(a);
  return a;
}

_static_unused_ void
dv_free(void * a, size_t sz) {
  if (a) {
    free(a);
  } else {
    dv_check(sz == 0);
  }
}

_static_unused_ double
dv_max(double d1, double d2) {
  if (d1 > d2)
    return d1;
  else
    return d2;
}

_static_unused_ double
dv_min(double d1, double d2) {
  if (d1 < d2)
    return d1;
  else
    return d2;
}

_static_unused_ const char *
dv_convert_char_to_binary(int x) {
  static char b[9];
  b[0] = '\0';
  int z;
  for (z=1; z<=1<<3; z<<=1)
    strcat(b, ((x & z) == z) ? "1" : "0");
  return b;
}

/* Node flag */

_static_unused_ void
dv_node_flag_init(int * f) {
  *f = DV_NODE_FLAG_NONE;
}

_static_unused_ int
dv_node_flag_check(int * f, int t) {
  int ret = ((*f & t) == t);
  return ret;
}

_static_unused_ void
dv_node_flag_set(int * f, int t) {
  if (!dv_node_flag_check(f,t))
    *f += t;
}

_static_unused_ void
dv_node_flag_remove(int * f, int t) {
  if (dv_node_flag_check(f,t))
    *f -= t;
}

_static_unused_ int
dv_is_set(dv_dag_node_t * node) {
  if (!node) return 0;
  return dv_node_flag_check(node->f, DV_NODE_FLAG_SET);
}

_static_unused_ int
dv_is_union(dv_dag_node_t * node) {
  if (!node) return 0;
  return dv_node_flag_check(node->f, DV_NODE_FLAG_UNION);
}

_static_unused_ int
dv_is_inner_loaded(dv_dag_node_t * node) {
  if (!node) return 0;
  return dv_node_flag_check(node->f, DV_NODE_FLAG_INNER_LOADED);
}

_static_unused_ int
dv_is_shrinked(dv_dag_node_t * node) {
  if (!node) return 0;
  return dv_node_flag_check(node->f, DV_NODE_FLAG_SHRINKED);
}

_static_unused_ int
dv_is_expanded(dv_dag_node_t * node) {
  if (!node) return 0;
  return !dv_is_shrinked(node);
}

_static_unused_ int
dv_is_expanding(dv_dag_node_t * node) {
  if (!node) return 0;
  return dv_node_flag_check(node->f, DV_NODE_FLAG_EXPANDING);
}

_static_unused_ int
dv_is_shrinking(dv_dag_node_t * node) {
  if (!node) return 0;
  return dv_node_flag_check(node->f, DV_NODE_FLAG_SHRINKING);
}

_static_unused_ int
dv_is_single(dv_dag_node_t * node) {
  if (!node) return 0;
  if (!dv_is_set(node)
      || !dv_is_union(node) || !dv_is_inner_loaded(node)
      || (dv_is_shrinked(node) && !dv_is_expanding(node)))
    return 1;
  
  return 0;
}

_static_unused_ int
dv_is_visible(dv_dag_node_t * node) {
  if (!node) return 0;
  dv_check(dv_is_set(node));
  if (!node->parent || dv_is_expanded(node->parent)) {
    if (!dv_is_union(node) || !dv_is_inner_loaded(node) || dv_is_shrinked(node))
      return 1;
  }
  return 0;
}

_static_unused_ int
dv_is_inward_callable(dv_dag_node_t * node) {
  if (!node) return 0;
  dv_check(dv_is_set(node));
  if (dv_is_union(node) && dv_is_inner_loaded(node)
      && ( dv_is_expanded(node) || dv_is_expanding(node) ))
    return 1;
  return 0;
}

_static_unused_ int
dv_log_set_error(int err) {
  CS->err = err;
  fprintf(stderr, "CS->err = %d\n", err);
  return err;
}

_static_unused_ int
dv_get_color_pool_index(int t, int v0, int v1, int v2, int v3) {
  int n = CS->CP_sizes[t];
  int i;
  for (i=0; i<n; i++) {
    if (CS->CP[t][i][0] == v0 && CS->CP[t][i][1] == v1
        && CS->CP[t][i][2] == v2 && CS->CP[t][i][3] == v3)
      return i;
  }
  dv_check(n < DV_COLOR_POOL_SIZE);
  CS->CP[t][n][0] = v0;
  CS->CP[t][n][1] = v1;
  CS->CP[t][n][2] = v2;
  CS->CP[t][n][3] = v3;
  CS->CP_sizes[t]++;
  return n;
}

/* Environment variables */

_static_unused_ int
dv_get_env_int(char * s, int * t) {
  char * v = getenv(s);
  if (v) {
    *t = atoi(v);
    return 1;
  }
  return 0;
}

_static_unused_ int
dv_get_env_long(char * s, long * t) {
  char * v = getenv(s);
  if (v) {
    *t = atol(v);
    return 1;
  }
  return 0;
}

_static_unused_ int
dv_get_env_string(char * s, char ** t) {
  char * v = getenv(s);
  if (v) {
    *t = strdup(v);
    return 1;
  }
  return 0;
}

_static_unused_ double
dv_get_time() {
  /* millisecond */
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1.0E3 + ((double)tv.tv_usec) / 1.0E3;
}

/***** end of Utilities *****/

/***** Chronological Traverser *****/

typedef struct {
  void (*process_event)(chronological_traverser * ct, dr_event evt);
  dr_pi_dag * G;
  dr_clock_t cum_running;
  dr_clock_t cum_delay;
  dr_clock_t cum_no_work;
  dr_clock_t t;
  long n_running;
  long n_ready;
  long n_workers;
  dr_clock_t total_elapsed;
  dr_clock_t total_t_1;
  long * edge_counts;		/* kind,u,v */
} dr_basic_stat;

_static_unused_ void 
dr_basic_stat_process_event(chronological_traverser * ct, 
			    dr_event evt);

_static_unused_ void
dr_calc_inner_delay(dr_basic_stat * bs, dr_pi_dag * G) {
  long n = G->n;
  long i;
  dr_clock_t total_elapsed = 0;
  dr_clock_t total_t_1 = 0;
  dr_pi_dag_node * T = G->T;
  long n_negative_inner_delays = 0;
  for (i = 0; i < n; i++) {
    dr_pi_dag_node * t = &T[i];
    dr_clock_t t_1 = t->info.t_1;
    dr_clock_t elapsed = t->info.end.t - t->info.start.t;
    if (t->info.kind < dr_dag_node_kind_section
	|| t->subgraphs_begin_offset == t->subgraphs_end_offset) {
      total_elapsed += elapsed;
      total_t_1 += t_1;
      if (elapsed < t_1 && t->info.worker != -1) {
	if (1 || (n_negative_inner_delays == 0)) {
	  fprintf(stderr,
		  "warning: node %ld has negative"
		  " inner delay (worker=%d, start=%llu, end=%llu,"
		  " t_1=%llu, end - start - t_1 = -%llu\n",
		  i, t->info.worker,
		  t->info.start.t, t->info.end.t, t->info.t_1,
		  t_1 - elapsed);
	}
	n_negative_inner_delays++;
      }
    }
  }
  if (n_negative_inner_delays > 0) {
    fprintf(stderr,
	    "warning: there are %ld nodes that have negative delays",
	    n_negative_inner_delays);
  }
  bs->total_elapsed = total_elapsed;
  bs->total_t_1 = total_t_1;
}

_static_unused_ void
dr_calc_edges(dr_basic_stat * bs, dr_pi_dag * G) {
  long n = G->n;
  long m = G->m;
  long nw = G->num_workers;
  /* C : a three dimensional array
     C(kind,i,j) is the number of type k edges from 
     worker i to worker j.
     we may counter nodes with worker id = -1
     (executed by more than one workers);
     we use worker id = nw for such entries
  */
  long * C_ = (long *)dr_malloc(sizeof(long) * dr_dag_edge_kind_max * (nw + 1) * (nw + 1));
#define EDGE_COUNTS(k,i,j) C_[k*(nw+1)*(nw+1)+i*(nw+1)+j]
  dr_dag_edge_kind_t k;
  long i, j;
  for (k = 0; k < dr_dag_edge_kind_max; k++) {
    for (i = 0; i < nw + 1; i++) {
      for (j = 0; j < nw + 1; j++) {
	EDGE_COUNTS(k,i,j) = 0;
      }
    }
  }
  for (i = 0; i < n; i++) {
    dr_pi_dag_node * t = &G->T[i];
    if (t->info.kind >= dr_dag_node_kind_section
	&& t->subgraphs_begin_offset == t->subgraphs_end_offset) {
      for (k = 0; k < dr_dag_edge_kind_max; k++) {
	int w = t->info.worker;
	if (w == -1) {
#if 0
	  fprintf(stderr, 
		  "warning: node %ld (kind=%s) has worker = %d)\n",
		  i, dr_dag_node_kind_to_str(t->info.kind), w);
#endif
	  EDGE_COUNTS(k, nw, nw) += t->info.logical_edge_counts[k];
	} else {
	  (void)dr_check(w >= 0);
	  (void)dr_check(w < nw);
	  EDGE_COUNTS(k, w, w) += t->info.logical_edge_counts[k];
	}
      }
    }    
  }
  for (i = 0; i < m; i++) {
    dr_pi_dag_edge * e = &G->E[i];
    int uw = G->T[e->u].info.worker;
    int vw = G->T[e->v].info.worker;
    if (uw == -1) {
#if 0
      fprintf(stderr, "warning: source node (%ld) of edge %ld %ld (kind=%s) -> %ld (kind=%s) has worker = %d\n",
	      e->u,
	      i, 
	      e->u, dr_dag_node_kind_to_str(G->T[e->u].info.kind), 
	      e->v, dr_dag_node_kind_to_str(G->T[e->v].info.kind), uw);
#endif
      uw = nw;
    }
    if (vw == -1) {
#if 0
      fprintf(stderr, "warning: dest node (%ld) of edge %ld %ld (kind=%s) -> %ld (kind=%s) has worker = %d\n",
	      e->v,
	      i, 
	      e->u, dr_dag_node_kind_to_str(G->T[e->u].info.kind), 
	      e->v, dr_dag_node_kind_to_str(G->T[e->v].info.kind), vw);
#endif
      vw = nw;
    }
    (void)dr_check(uw >= 0);
    (void)dr_check(uw <= nw);
    (void)dr_check(vw >= 0);
    (void)dr_check(vw <= nw);
    EDGE_COUNTS(e->kind, uw, vw)++;
  }
#undef EDGE_COUNTS
  bs->edge_counts = C_;
}

_static_unused_ void 
dr_basic_stat_init(dr_basic_stat * bs, dr_pi_dag * G) {
  bs->process_event = dr_basic_stat_process_event;
  bs->G = G;
  bs->n_running = 0;
  bs->n_ready = 0;
  bs->n_workers = G->num_workers;
  bs->cum_running = 0;		/* cumulative running cpu time */
  bs->cum_delay = 0;		/* cumulative delay cpu time */
  bs->cum_no_work = 0;		/* cumulative no_work cpu time */
  bs->t = 0;			/* time of the last event */
}

_static_unused_ void 
dr_basic_stat_process_event(chronological_traverser * ct, 
			    dr_event evt) {
  dr_basic_stat * bs = (dr_basic_stat *)ct;
  dr_clock_t dt = evt.t - bs->t;

  int n_running = bs->n_running;
  int n_delay, n_no_work;
  if (bs->n_running >= bs->n_workers) {
    /* great, all workers are running */
    n_delay = 0;
    n_no_work = 0;
    if (bs->n_running > bs->n_workers) {
      fprintf(stderr, 
	      "warning: n_running = %ld"
	      " > n_workers = %ld (clock skew?)\n",
	      bs->n_running, bs->n_workers);
    }
    n_delay = 0;
    n_no_work = 0;
  } else if (bs->n_running + bs->n_ready <= bs->n_workers) {
    /* there were enough workers to run ALL ready tasks */
    n_delay = bs->n_ready;
    n_no_work = bs->n_workers - (bs->n_running + bs->n_ready);
  } else {
    n_delay = bs->n_workers - bs->n_running;
    n_no_work = 0;
  }
  bs->cum_running += n_running * dt;
  bs->cum_delay   += n_delay * dt;
  bs->cum_no_work += n_no_work * dt;

  switch (evt.kind) {
  case dr_event_kind_ready: {
    bs->n_ready++;
    break;
  }
  case dr_event_kind_start: {
    bs->n_running++;
    break;
  }
  case dr_event_kind_last_start: {
    bs->n_ready--;
    break;
  }
  case dr_event_kind_end: {
    bs->n_running--;
    break;
  }
  default:
    assert(0);
    break;
  }

  bs->t = evt.t;
}

/***** Chronological Traverser *****/
