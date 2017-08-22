/*
 * dagmodel.h
 */

#ifndef DAGMODEL_HEADER_
#define DAGMODEL_HEADER_
#pragma once


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <errno.h>
#include <stdarg.h>

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

#define _unused_ __attribute__((unused))
#define _static_unused_ static __attribute__((unused))

#define DM_MAX_DAG_FILE 1000
#define DM_MAX_DAG 1000
#define DM_MAX_VIEW 1000

#define DM_NUM_COORDINATES 8
#define DM_LAYOUT_DAG_COORDINATE 0
#define DM_LAYOUT_DAG_BOX_LOG_COORDINATE 1
#define DM_LAYOUT_DAG_BOX_POWER_COORDINATE 2
#define DM_LAYOUT_DAG_BOX_LINEAR_COORDINATE 3
#define DM_LAYOUT_TIMELINE_COORDINATE 4
#define DM_LAYOUT_TIMELINE_VER_COORDINATE 5
#define DM_LAYOUT_PARAPROF_COORDINATE 6
#define DM_LAYOUT_CRITICAL_PATH_COORDINATE 7

#define DM_NUM_CRITICAL_PATHS 3
#define DM_CRITICAL_PATH_0 0 /* most work */
#define DM_CRITICAL_PATH_1 1 /* last finished tails */
#define DM_CRITICAL_PATH_2 2 /* most problematic delay */
#define DM_CRITICAL_PATH_0_COLOR "red"
#define DM_CRITICAL_PATH_1_COLOR "green"
#define DM_CRITICAL_PATH_2_COLOR "blue"

#define DM_DEFAULT_PAGE_SIZE 1 << 20 // 1 MB
#define DM_DEFAULT_PAGE_MIN_NODES 2
#define DM_DEFAULT_PAGE_MIN_ENTRIES 2

#define DM_NODE_FLAG_NONE         0        /* none */
#define DM_NODE_FLAG_SET          1        /* none - set */
#define DM_NODE_FLAG_UNION        (1 << 1) /* single - union */
#define DM_NODE_FLAG_INNER_LOADED (1 << 2) /* no inner - inner loaded */
#define DM_NODE_FLAG_SHRINKED     (1 << 3) /* expanded - shrinked */
#define DM_NODE_FLAG_EXPANDING    (1 << 4) /* expanding */
#define DM_NODE_FLAG_SHRINKING    (1 << 5) /* shrinking */

#define DM_OK 0
#define DM_ERROR_OONP 1 /* out of node pool */

#define DM_STACK_CELL_SZ 1
#define DM_LLIST_CELL_SZ 1

#define DM_NODE_FLAG_CRITICAL_PATH_0 (1 << 6) /* node is on critical path of work */
#define DM_NODE_FLAG_CRITICAL_PATH_1 (1 << 7) /* node is on critical path of work & delay */
#define DM_NODE_FLAG_CRITICAL_PATH_2 (1 << 8) /* node is on critical path of weighted work & delay */

#define DM_RADIX_LOG 1.8
#define DM_RADIX_POWER 0.42
#define DM_RADIX_LINEAR 100000 //100000

#define DM_PARAPROF_MIN_ENTRY_INTERVAL 2000

#define DM_ANIMATION_DURATION 400 /* milliseconds */
#define DM_ANIMATION_STEP 80 /* milliseconds */


/* Stack & linked list */

typedef struct dm_stack_cell {
  void * item;
  struct dm_stack_cell * next;
} dm_stack_cell_t;

typedef struct dm_stack {
  dm_stack_cell_t * freelist;
  dm_stack_cell_t * top;
} dm_stack_t;

typedef struct dm_llist_cell {
  void * item;
  struct dm_llist_cell * next;
} dm_llist_cell_t;

typedef struct dm_llist {
  int sz;
  dm_llist_cell_t * top;
} dm_llist_t;


/* DAG structures */

typedef struct dm_pidag {
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
  dm_llist_t itl[1]; /* list of pii's of nodes that have info tag */
  dr_pi_dag * G;
  long sz;

  /* DAG management window */
  char * name;
  void * g; /* pointer to a structure holding GUI-dependent elements, unused inside dagmodel */
} dm_pidag_t;

typedef struct dm_node_coordinate {
  double x, y; /* coordinates */
  double xp, xpre; /* coordinates based on parent, pre */
  double lw, rw, dw; /* left/right/down widths */
  double link_lw, link_rw, link_dw;
} dm_node_coordinate_t;

typedef struct dm_critical_path_stat {
  double work;
  double delay;
  double sched_delay;
  double sched_delays[dr_dag_edge_kind_max];
  double sched_delay_nowork; /* total no-work during scheduler delays */
  double sched_delay_delay; /* total delay during scheduler delays */
} dm_critical_path_stat_t;

typedef struct dm_dag_node {
  /* task-parallel data */
  long pii;

  /* state data */  
  int f[1]; /* node flags, 0x0: single, 0x01: union/collapsed, 0x11: union/expanded */
  int d; /* depth */
  
  /* linking structure */
  struct dm_dag_node * parent;
  struct dm_dag_node * pre;
  struct dm_dag_node * next; /* is used for linking in pool before node is really initialized */
  struct dm_dag_node * spawn;
  struct dm_dag_node * head; /* inner head node */
  struct dm_dag_node * tail; /* inner tail node */

  /* layout */
  dm_node_coordinate_t c[DM_NUM_COORDINATES]; /* 0:dag, 1:dagbox, 2:timeline_ver, 3:timeline, 4:paraprof */

  /* animation */
  double started; /* started time of animation */

  long long r; /* remarks */
  long long link_r; /* summed remarks with linked paths */

  char highlight; /* to highlight the node when drawing */

  /* statistics of inner subgraphs */
  dm_critical_path_stat_t cpss[DM_NUM_CRITICAL_PATHS];
} dm_dag_node_t;

typedef struct dm_dag dm_dag_t;
typedef struct dm_histogram dm_histogram_t;

typedef struct dm_animation {
  int on; /* on/off */
  double duration; /* milliseconds */
  double step; /* milliseconds */
  dm_llist_t movings[1]; /* list of moving nodes */
  dm_dag_t * D; /* DAG that this animation belongs to */
} dm_animation_t;

typedef struct dm_motion {
  int on;
  double duration;
  double step;
  dm_dag_t * D;
  long target_pii;
  double xfrom, yfrom;
  double zrxfrom, zryfrom;
  double xto, yto;
  double zrxto, zryto;
  double start_t;
} dm_motion_t;

typedef struct dm_dag {
  char * name;
  char * name_on_graph;
  /* PIDAG */
  dm_pidag_t * P;

  /* DAG's skeleton */
  //dm_dag_node_t * T;  /* array of all nodes */
  //char * To;
  //long Tsz;
  //long Tn;

  /* DAG */
  dm_dag_node_t * rt;  /* root task */
  int dmax; /* depth max */
  double bt; /* begin time */
  double et; /* end time */
  long n;

  /* expansion state */
  int cur_d; /* current depth */
  int cur_d_ex; /* current depth of extensible union nodes */
  int collapsing_d; /* current depth excluding children of collapsing nodes */

  /* layout parameters */
  //int sdt; /* scale down type: 0 (log), 1 (power), 2 (linear) */
  double log_radix;
  double power_radix;
  double linear_radix;
  int frombt;
  double radius;

  /* other */
  dm_llist_t itl[1]; /* list of nodes that have info tag */
  dm_histogram_t * H; /* structure for the paraprof view (5th) */

  int draw_with_current_time;
  double current_time;
  double time_step;
  int show_critical_paths[DM_NUM_CRITICAL_PATHS];
  int critical_paths_computed;

  void * g; /* pointer to a structure holding GUI-dependent elements, unused inside dagmodel */
  dm_animation_t anim[1]; /* parameters for node animations (expanding, collapsing) */
  dm_motion_t move[1]; /* parameters for node movings */
} dm_dag_t;


/* Histogram structures */

typedef enum {
  dm_histogram_layer_running,
  dm_histogram_layer_ready_end,
  dm_histogram_layer_ready_create,
  dm_histogram_layer_ready_create_cont,
  dm_histogram_layer_ready_wait_cont,
  dm_histogram_layer_ready_other_cont,
  dm_histogram_layer_max,
} dm_histogram_layer_t;

typedef struct dm_histogram_entry {
  double t;
  struct dm_histogram_entry * next;
  double h[dm_histogram_layer_max];
  double value_1;
  double cumul_value_1;
  double value_2;
  double cumul_value_2;
  double value_3;
  double cumul_value_3;
} dm_histogram_entry_t;

typedef struct dm_histogram {
  dm_histogram_entry_t * head_e;
  dm_histogram_entry_t * tail_e;
  long n_e;
  dm_dag_t * D;
  double work, delay, nowork;
  double min_entry_interval;
  double unit_thick;
  dm_histogram_entry_t * tallest_e;
  double max_h;
} dm_histogram_t;


/* Memory pool structures */

typedef struct dm_histogram_entry_page {
  struct dm_histogram_entry_page * next;
  long sz;
  dm_histogram_entry_t entries[DM_DEFAULT_PAGE_MIN_ENTRIES];
} dm_histogram_entry_page_t;

typedef struct dm_histogram_entry_pool {
  dm_histogram_entry_t * head;
  dm_histogram_entry_t * tail;
  dm_histogram_entry_page_t * pages;
  long sz; /* size in bytes */
  long N;  /* total number of entries */
  long n;  /* number of free entries */
} dm_histogram_entry_pool_t;


typedef struct dm_dag_node_page {
  struct dm_dag_node_page * next;
  long sz;
  dm_dag_node_t nodes[DM_DEFAULT_PAGE_MIN_NODES];
} dm_dag_node_page_t;

typedef struct dm_dag_node_pool {
  dm_dag_node_t * head;
  dm_dag_node_t * tail;
  dm_dag_node_page_t * pages;
  long sz; /* size in bytes */
  long N;  /* total number of nodes */
  long n;  /* number of free nodes */
} dm_dag_node_pool_t;


/* Global state structure */
typedef long long dm_clock_t;

typedef struct dm_stat_breakdown_graph {
  int checked_D[DM_MAX_DAG]; /* show or not show */
  char * fn;
  dm_clock_t work[DM_MAX_DAG];
  dm_clock_t delay[DM_MAX_DAG];
  dm_clock_t nowork[DM_MAX_DAG];
  char * fn_2;
  int checked_cp[DM_NUM_CRITICAL_PATHS];
} dm_stat_breakdown_graph_t;

/* runtime-adjustable options */
typedef struct dm_options {
  /* Layout */
  double radius; /* node radius */
  double hnd; /* horizontal node distance */
  double vnd; /* vertical node distance */
  double nlw; /* node line width */
  double nlw_collective_node_factor; /* node line width's multiplying factor for collective nodes */

  double union_node_puffing_margin;
} dm_options_t;

/* default values for runtime-adjustable options */
_static_unused_ dm_options_t dm_options_default_values = {
  20.0, /* radius */
  70.0, /* hnd: horizontal node distance */
  70.0, /* vnd: vertical node distance */
  3.0,  /* nlw: node line width */
  1.5,  /* nlw_collective_node_factor */

  6.0,  /* union_node_puffing_margin */
};

typedef struct dm_global_state {
  /* DAG */
  dm_pidag_t P[DM_MAX_DAG_FILE];
  dm_dag_t D[DM_MAX_DAG];
  int nP;
  int nD;
  dm_llist_cell_t * FL;
  int error;
  
  /* Memory Pools */
  dm_dag_node_pool_t pool[1];
  dm_histogram_entry_pool_t epool[1];

  /* Statistics */
  dm_stat_breakdown_graph_t SBG[1];

  /* Options */
  dm_options_t opts;

  /* Flags */
  int verbose_level;
  int oncp_flags[DM_NUM_CRITICAL_PATHS];
  int initialized;
} dm_global_state_t;

extern dm_global_state_t DMG[]; /* global common state */


/* PIDAG */
dm_pidag_t *     dm_pidag_read_new_file(char *);
dr_pi_dag_node * dm_pidag_get_node_by_id(dm_pidag_t *, long);
dr_pi_dag_node * dm_pidag_get_node_by_dag_node(dm_pidag_t *, dm_dag_node_t *);

/* DAG */
void       dm_dag_node_init(dm_dag_node_t *, dm_dag_node_t *, long);
int        dm_dag_node_set(dm_dag_t *, dm_dag_node_t *);
int        dm_dag_build_node_inner(dm_dag_t *, dm_dag_node_t *);
int        dm_dag_collapse_node_inner(dm_dag_t *, dm_dag_node_t *);
void       dm_dag_clear_shrinked_nodes(dm_dag_t *);
void       dm_dag_init(dm_dag_t *, dm_pidag_t *);
dm_dag_t * dm_dag_create_new_with_pidag(dm_pidag_t *);
dm_dag_t * dm_add_dag(char *);
int        dm_get_dag_id(dm_dag_t *);
dm_dag_t * dm_get_dag(int);

dm_dag_node_t * dm_dag_node_traverse_children(dm_dag_node_t *, dm_dag_node_t *);
dm_dag_node_t * dm_dag_node_traverse_children_inorder(dm_dag_node_t *, dm_dag_node_t *);
dm_dag_node_t * dm_dag_node_traverse_tails(dm_dag_node_t *, dm_dag_node_t *);
dm_dag_node_t * dm_dag_node_traverse_nexts(dm_dag_node_t *, dm_dag_node_t *);

int             dm_dag_node_count_nexts(dm_dag_node_t *);
dm_dag_node_t * dm_dag_node_get_next(dm_dag_node_t *);
dm_dag_node_t * dm_dag_node_get_single_head(dm_dag_node_t *);
dm_dag_node_t * dm_dag_node_get_single_last(dm_dag_node_t *);

/* Memory pools */
void dm_dag_node_pool_init(dm_dag_node_pool_t *);
dm_dag_node_t * dm_dag_node_pool_pop(dm_dag_node_pool_t *);
void dm_dag_node_pool_push(dm_dag_node_pool_t *, dm_dag_node_t *);
dm_dag_node_t * dm_dag_node_pool_pop_contiguous(dm_dag_node_pool_t *, long);

void dm_histogram_entry_pool_init(dm_histogram_entry_pool_t *);
dm_histogram_entry_t * dm_histogram_entry_pool_pop(dm_histogram_entry_pool_t *);
void dm_histogram_entry_pool_push(dm_histogram_entry_pool_t *, dm_histogram_entry_t *);

/* Basic data structures */
void dm_stack_init(dm_stack_t *);
void dm_stack_fini(dm_stack_t *);
void dm_stack_push(dm_stack_t *, void *);
void * dm_stack_pop(dm_stack_t *);

void dm_llist_init(dm_llist_t *);
void dm_llist_fini(dm_llist_t *);
dm_llist_t * dm_llist_create();
void dm_llist_destroy(dm_llist_t *);
int dm_llist_is_empty(dm_llist_t *);
void dm_llist_add(dm_llist_t *, void *);
void * dm_llist_pop(dm_llist_t *);
void * dm_llist_get(dm_llist_t *, int);
void * dm_llist_get_top(dm_llist_t *);
void * dm_llist_remove(dm_llist_t *, void *);
int dm_llist_has(dm_llist_t *, void *);
void * dm_llist_iterate_next(dm_llist_t *, void *);
int dm_llist_size(dm_llist_t *);

/* Histogram */
void dm_histogram_init(dm_histogram_t *);
double dm_histogram_get_max_height(dm_histogram_t *);
dm_histogram_entry_t * dm_histogram_insert_entry(dm_histogram_t *, double, dm_histogram_entry_t *);
void dm_histogram_add_node(dm_histogram_t *, dm_dag_node_t *, dm_histogram_entry_t **);
void dm_histogram_remove_node(dm_histogram_t *, dm_dag_node_t *, dm_histogram_entry_t **);
void dm_histogram_clean(dm_histogram_t *);
void dm_histogram_fini(dm_histogram_t *);
void dm_histogram_reset(dm_histogram_t *);
void dm_histogram_build_all(dm_histogram_t *);
void dm_histogram_compute_significant_intervals(dm_histogram_t *);

/* Compute */
char * dm_filename_get_short_name(char *);
void dm_global_state_init();
int dm_global_state_initialized();
void dm_dag_build_inner_all(dm_dag_t *);
void dm_dag_compute_critical_paths(dm_dag_t *);

void dm_compute_dag(dm_dag_t *);
dm_dag_t * dm_compute_dag_file(char *);

/* Layout DAG */
void dm_do_expanding_one(dm_dag_t *);
void dm_do_collapsing_one(dm_dag_t *);
double dm_calculate_animation_rate(dm_dag_t *, dm_dag_node_t *);
double dm_calculate_animation_reverse_rate(dm_dag_t *, dm_dag_node_t *);
void dm_animation_init(dm_animation_t *, dm_dag_t *);
void dm_animation_tick(dm_animation_t *);
void dm_animation_add_node(dm_animation_t *, dm_dag_node_t *);
void dm_animation_remove_node(dm_animation_t *, dm_dag_node_t *);
void dm_animation_reverse_node(dm_animation_t *, dm_dag_node_t *);
double dm_get_alpha_fading_out(dm_dag_t *, dm_dag_node_t *);
double dm_get_alpha_fading_in(dm_dag_t *, dm_dag_node_t *);
void dm_motion_init(dm_motion_t *, dm_dag_t *);
dm_dag_node_t * dm_dag_find_node(dm_dag_t *, double, double, int);
void dm_dag_layout1(dm_dag_t *);
void dm_dag_layout2(dm_dag_t *);


/***** Utilities *****/

_static_unused_ int
dm_check_(int condition, const char * condition_s, 
                     const char * __file__, int __line__, 
                     const char * func) {
  if (!condition) {
    fprintf(stderr, "%s:%d:%s: check failed : %s\n", 
            __file__, __line__, func, condition_s);
    exit(1);
  }
  return 1;
}

#define dm_check(x) (dm_check_(((x)?1:0), #x, __FILE__, __LINE__, __func__))

_static_unused_ void *
dm_malloc(size_t sz) {
  void * a = malloc(sz);
  dm_check(a);
  return a;
}

_static_unused_ void
dm_free(void * a, size_t sz) {
  if (a) {
    free(a);
  } else {
    dm_check(sz == 0);
  }
}

/* Node flag */

_static_unused_ void
dm_node_flag_init(int * f) {
  *f = DM_NODE_FLAG_NONE;
}

_static_unused_ int
dm_node_flag_check(int * f, int t) {
  int ret = ((*f & t) == t);
  return ret;
}

_static_unused_ void
dm_node_flag_set(int * f, int t) {
  if (!dm_node_flag_check(f,t))
    *f += t;
}

_static_unused_ void
dm_node_flag_remove(int * f, int t) {
  if (dm_node_flag_check(f,t))
    *f -= t;
}

_static_unused_ int
dm_is_set(dm_dag_node_t * node) {
  if (!node) return 0;
  return dm_node_flag_check(node->f, DM_NODE_FLAG_SET);
}

_static_unused_ int
dm_is_union(dm_dag_node_t * node) {
  if (!node) return 0;
  return dm_node_flag_check(node->f, DM_NODE_FLAG_UNION);
}

_static_unused_ int
dm_is_inner_loaded(dm_dag_node_t * node) {
  if (!node) return 0;
  return dm_node_flag_check(node->f, DM_NODE_FLAG_INNER_LOADED);
}

_static_unused_ int
dm_is_shrinked(dm_dag_node_t * node) {
  if (!node) return 0;
  return dm_node_flag_check(node->f, DM_NODE_FLAG_SHRINKED);
}

_static_unused_ int
dm_is_expanded(dm_dag_node_t * node) {
  if (!node) return 0;
  return !dm_is_shrinked(node);
}

_static_unused_ int
dm_is_expanding(dm_dag_node_t * node) {
  if (!node) return 0;
  return dm_node_flag_check(node->f, DM_NODE_FLAG_EXPANDING);
}

_static_unused_ int
dm_is_shrinking(dm_dag_node_t * node) {
  if (!node) return 0;
  return dm_node_flag_check(node->f, DM_NODE_FLAG_SHRINKING);
}

_static_unused_ int
dm_is_single(dm_dag_node_t * node) {
  if (!node) return 0;
  if (!dm_is_set(node)
      || !dm_is_union(node) || !dm_is_inner_loaded(node)
      || (dm_is_shrinked(node) && !dm_is_expanding(node)))
    return 1;
  
  return 0;
}

_static_unused_ int
dm_is_visible(dm_dag_node_t * node) {
  if (!node) return 0;
  dm_check(dm_is_set(node));
  if (!node->parent || dm_is_expanded(node->parent)) {
    if (!dm_is_union(node) || !dm_is_inner_loaded(node) || dm_is_shrinked(node))
      return 1;
  }
  return 0;
}

_static_unused_ int
dm_is_inward_callable(dm_dag_node_t * node) {
  if (!node) return 0;
  dm_check(dm_is_set(node));
  if (dm_is_union(node) && dm_is_inner_loaded(node)
      && ( dm_is_expanded(node) || dm_is_expanding(node) ))
    return 1;
  return 0;
}

_static_unused_ void
dm_log_print_info(const char * file, int line, const char * format, ...) {
  char s[1024];
  va_list args;
  va_start(args, format);
  vsprintf(s, format, args);
  fprintf(stdout, "[Info] (%s:%d): %s\n", file, line, s);
  va_end(args);
}

_static_unused_ void
dm_log_print_warning(const char * file, int line, const char * format, ...) {
  char s[1024];
  va_list args;
  va_start(args, format);
  vsprintf(s, format, args);
  fprintf(stdout, "[Warning] (%s:%d): %s\n", file, line, s);
  va_end(args);
}

_static_unused_ void
dm_log_print_error(const char * file, int line, const char * format, ...) {
  char s[1024];
  va_list args;
  va_start(args, format);
  vsprintf(s, format, args);
  fprintf(stderr, "[Error] (%s:%d): %s\n", file, line, s);
  va_end(args);
}

#define dm_pinfo(s, ...) dm_log_print_info(__FILE__, __LINE__, s, ##__VA_ARGS__)
#define dm_pwarning(s, ...) dm_log_print_warning(__FILE__, __LINE__, s, ##__VA_ARGS__)
#define dm_perror(s, ...) dm_log_print_error(__FILE__, __LINE__, s, ##__VA_ARGS__)

_static_unused_ int
dm_log_set_error(int err) {
  DMG->error = err;
  fprintf(stderr, "DMG->error = %d\n", err);
  return err;
}

/* Environment variables */

_static_unused_ int
dm_get_env_int(char * s, int * t) {
  char * v = getenv(s);
  if (v) {
    *t = atoi(v);
    return 1;
  }
  return 0;
}

_static_unused_ int
dm_get_env_long(char * s, long * t) {
  char * v = getenv(s);
  if (v) {
    *t = atol(v);
    return 1;
  }
  return 0;
}

_static_unused_ int
dm_get_env_string(char * s, char ** t) {
  char * v = getenv(s);
  if (v) {
    *t = strdup(v);
    return 1;
  }
  return 0;
}

_static_unused_ double
dm_get_time() {
  /* millisecond */
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return tv.tv_sec * 1.0E3 + ((double)tv.tv_usec) / 1.0E3;
}

#define dm_max(d1,d2) (((d1)>=(d2))?(d1):(d2))
#define dm_min(d1,d2) (((d1)<=(d2))?(d1):(d2))

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

_unused_ void dr_calc_inner_delay(dr_basic_stat *, dr_pi_dag *);
_unused_ void dr_calc_edges(dr_basic_stat *, dr_pi_dag *);
_unused_ void dr_basic_stat_init(dr_basic_stat *, dr_pi_dag *);
_unused_ void dr_basic_stat_process_event(chronological_traverser *, dr_event);

/***** end of Chronological Traverser *****/


#endif 
