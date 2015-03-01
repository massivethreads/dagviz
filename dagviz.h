/*
 * dagviz.h
 */

#ifndef DAGVIZ_HEADER_
#define DAGVIZ_HEADER_
#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <cairo.h>
#include <cairo-ps.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <dag_recorder_impl.h>

#include <execinfo.h>

#ifdef DV_ENABLE_LIBUNWIND
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

#include <sys/time.h>
#include <unistd.h>
#include <signal.h>

#ifdef DV_ENABLE_BFD
#include <bfd.h>
#endif


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
  int sz;
  dv_llist_cell_t * top;
} dv_llist_t;


/*-----------------Constants-----------------*/

#define DV_ZOOM_INCREMENT 1.20
#define DV_SCALE_INCREMENT 1.07
#define DV_HDIS 70
#define DV_VDIS 60
#define DV_RADIUS 15
#define NUM_COLORS 34
#define DV_UNION_NODE_DOUBLE_BORDER 3

#define DV_NODE_FLAG_NONE         0        /* none */
#define DV_NODE_FLAG_SET          1        /* none - set */
#define DV_NODE_FLAG_UNION        (1 << 1) /* single - union */
#define DV_NODE_FLAG_INNER_LOADED (1 << 2) /* no inner - inner loaded */
#define DV_NODE_FLAG_SHRINKED     (1 << 3) /* expanded - shrinked */
#define DV_NODE_FLAG_EXPANDING    (1 << 4) /* expanding */
#define DV_NODE_FLAG_SHRINKING    (1 << 5) /* shrinking */

#define DV_ZOOM_TO_FIT_MARGIN 20
#define DV_STRING_LENGTH 100
#define DV_STATUS_PADDING 7
#define DV_SAFE_CLICK_RANGE 1
#define DV_UNION_NODE_MARGIN 6
#define DV_NODE_LINE_WIDTH 1.5
#define DV_NODE_LINE_WIDTH_COLLECTIVE_FACTOR 2.0
//#define DV_EDGE_LINE_WIDTH 1.5
#define DV_RADIX_LOG 1.8
#define DV_RADIX_POWER 0.42
#define DV_RADIX_LINEAR 100000

#define DV_ANIMATION_DURATION 400 // milliseconds
#define DV_ANIMATION_STEP 30 // milliseconds

#define DV_NUM_LAYOUT_TYPES 5
#define DV_LAYOUT_TYPE_INIT 0 // not paraprof coz need to check H of D
#define DV_NODE_COLOR_INIT 0 // 0:worker, 1:cpu, 2:kind, 3:code start, 4:code end, 5: code segment
#define DV_SCALE_TYPE_INIT 2
#define DV_FROMBT_INIT 0
#define DV_EDGE_TYPE_INIT 3
#define DV_EDGE_AFFIX_LENGTH 10
#define DV_CLICK_MODE_INIT 0

#define DV_COLOR_POOL_SIZE 100
#define DV_NUM_COLOR_POOLS 6

#define DV_TIMELINE_NODE_WITH_BORDER 0
#define DV_ENTRY_RADIX_MAX_LENGTH 20

#define DV_DAG_NODE_POOL_SIZE 100000

#define DV_MAX_DAG_FILE 5
#define DV_MAX_DAG 10
#define DV_MAX_VIEW 10
#define DV_MAX_VIEWPORT 10
#define DV_MAX_HISTOGRAM 5

#define DV_OK 0
#define DV_ERROR_OONP 1 /* out of node pool */

#define backtrace_sample_sz 15

#define DV_HISTOGRAM_PIECE_POOL_SIZE 10000000
#define DV_HISTOGRAM_ENTRY_POOL_SIZE 40000

#define DV_CLIPPING_FRAME_MARGIN 0
#define DV_HISTOGRAM_MARGIN_DOWN 20
#define DV_HISTOGRAM_MARGIN_SIDE 50

#define DV_HISTOGRAM_DIVIDE_TO_PIECES 0

#define DV_CAIRO_BOUND_MIN -8e6 //-(1<<23)
#define DV_CAIRO_BOUND_MAX 8e6 //(1<<23)

/*-----------------Data Structures-----------------*/

/* a single record of backtrace */
typedef struct {
  unsigned long long tsc;
  int worker;
  int n;
  void * frames[backtrace_sample_sz];
} bt_sample_t;

typedef struct dv_pidag {
  long n;			/* length of T */
  long m;			/* length of E */
  long start_clock;             /* absolute clock time of start */
  long num_workers;		/* number of workers */
  dr_pi_dag_node * T;		/* all nodes in a contiguous array */
  dr_pi_dag_edge * E;		/* all edges in a contiguous array */
  dr_pi_string_table S[1];
  char * fn; /* dag file name */
  struct stat stat[1]; /* file stat structure */
  dv_llist_t itl[1]; /* list of pii's of nodes that have info tag */
} dv_pidag_t;

typedef struct dv_node_coordinate {
  double x, y; /* coordinates */
  double xp, xpre; /* coordinates based on parent, pre */
  double lw, rw, dw; /* left/right/down widths */
  double link_lw, link_rw, link_dw;
} dv_node_coordinate_t;

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

  /* layout */
  dv_node_coordinate_t c[DV_NUM_LAYOUT_TYPES]; /* 0:grid, 1:bbox, 2:timeline, 3:timeline2 */

  /* animation */
  double started; /* started time of animation */

} dv_dag_node_t;

typedef struct dv_histogram dv_histogram_t;

typedef struct dv_dag {
  /* PIDAG */
  dv_pidag_t * P;

  /* DAG's skeleton */
  dv_dag_node_t * T;  /* array of all nodes */
  char * To;
  long Tsz;
  long Tn;

  /* DAG's content */
  dv_dag_node_t * rt;  /* root task */
  int dmax; /* depth max */
  double bt; /* begin time */
  double et; /* end time */

  /* expansion state */
  int cur_d; /* current depth */
  int cur_d_ex; /* current depth of extensible union nodes */

  /* layout parameters */
  int sdt; /* scale down type: 0 (log), 1 (power), 2 (linear) */
  double log_radix;
  double power_radix;
  double linear_radix;
  int frombt;
  double radius;

  /* other */
  dv_llist_t itl[1]; /* list of nodes that have info tag */
  dv_histogram_t * H; /* structure for the paraprof view (5th) */
  char tolayout[DV_NUM_LAYOUT_TYPES];
} dv_dag_t;

typedef struct dv_view dv_view_t;

typedef struct dv_animation {
  int on; /* on/off */
  double duration; /* milliseconds */
  double step; /* milliseconds */
  dv_llist_t movings[1]; /* list of moving nodes */
  dv_view_t *V; /* view that possesses this animation */
} dv_animation_t;

typedef struct dv_motion {
  int on;
  double duration;
  double step;
  dv_view_t *V;
  long target_pii;
  double xfrom, yfrom;
  double zrxfrom, zryfrom;
  double xto, yto;
  double zrxto, zryto;
  double start_t;
} dv_motion_t;

typedef struct dv_view_status {
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
  int et; /* edge type */
  int edge_affix; /* edge affix length */
  int cm; /* click mode */
  long ndh; /* number of nodes including hidden ones */
  int focused;

  /* drawing parameters */
  char do_zoomfit;     /* flag to do zoomfit when drawing view */
  double x, y;        /* current coordinates of the central point */
  double basex, basey;
  double zoom_ratio_x; /* horizontal zoom ratio */
  double zoom_ratio_y; /* vertical zoom ratio */
  int do_zoom_x;
  int do_zoom_y;
  int do_scale_radix;
  int do_scale_radius;

  /* moving animation */
  dv_motion_t m[1];
} dv_view_status_t;

typedef struct dv_viewport dv_viewport_t;
typedef struct dv_view dv_view_t;

typedef struct dv_view_interface {
  dv_view_t * V;
  dv_viewport_t * VP;
  GtkWidget * toolbar;
  GtkWidget * togg_focused;
  GtkWidget * combobox_lt;
  GtkWidget * combobox_nc;
  GtkWidget * combobox_sdt;
  GtkWidget * entry_radix;
  GtkWidget * combobox_frombt;
  GtkWidget * combobox_et;
  GtkWidget * togg_eaffix;
  GtkWidget * entry_search;
  GtkWidget * checkbox_xzoom;
  GtkWidget * checkbox_yzoom;
  GtkWidget * checkbox_scale_radix;
  GtkWidget * checkbox_scale_radius;
  GtkWidget * grid;
} dv_view_interface_t;

typedef struct dv_view {
  dv_dag_t * D; /* DV DAG */
  dv_view_status_t S[1]; /* layout/drawing attributes */
  dv_view_interface_t * I[DV_MAX_VIEWPORT]; /* interfaces to viewports */
  dv_viewport_t * mainVP; /* main VP that this V is assosiated with */
} dv_view_t;

typedef struct dv_viewport {
  int split; /* split into two nested paned viewports or not */
  GtkWidget * frame; /* contains [paned | box]*/
  /* Split */
  GtkWidget * paned; /* GtkPaned */
  GtkOrientation orientation; /* split orientation */
  struct dv_viewport * vp1; /* first child viewport */
  struct dv_viewport * vp2; /* second child viewport */
  /* No split */
  GtkWidget * box; /* contains toolbars and darea */
  GtkWidget * darea; /* drawing area */
  dv_view_interface_t * I[DV_MAX_VIEW]; /* interfaces to view */
  double vpw, vph;  /* viewport's size */
} dv_viewport_t;

typedef struct dv_btsample_viewer {
  dv_pidag_t * P;
  GtkWidget * label_dag_file_name;
  GtkWidget * entry_bt_file_name;
  GtkWidget * entry_binary_file_name;
  GtkWidget * combobox_worker;
  GtkWidget * entry_time_from;
  GtkWidget * entry_time_to;
  GtkWidget * text_view;
  GtkWidget * entry_node_id;
} dv_btsample_viewer_t;


typedef enum {
  dv_histogram_layer_running,
  dv_histogram_layer_ready_end,
  dv_histogram_layer_ready_create,
  dv_histogram_layer_ready_create_cont,
  dv_histogram_layer_ready_wait_cont,
  dv_histogram_layer_ready_other_cont,
  dv_histogram_layer_max,
} dv_histogram_layer_t;

typedef struct dv_histogram_entry dv_histogram_entry_t;

typedef struct dv_histogram_piece {
  dv_histogram_entry_t * e;
  double h;
  struct dv_histogram_piece * next;
  dv_dag_node_t * node;
} dv_histogram_piece_t;

typedef struct dv_histogram_piece_pool {
  dv_histogram_piece_t T[DV_HISTOGRAM_PIECE_POOL_SIZE];
  char avail[DV_HISTOGRAM_PIECE_POOL_SIZE];
  long sz;
  long n;
} dv_histogram_piece_pool_t;

typedef struct dv_histogram_entry {
  double t;
  dv_histogram_piece_t * pieces[dv_histogram_layer_max];
  struct dv_histogram_entry * next;
  double h[dv_histogram_layer_max];
  double sum_h;
} dv_histogram_entry_t;

typedef struct dv_histogram_entry_pool {
  dv_histogram_entry_t T[DV_HISTOGRAM_ENTRY_POOL_SIZE];
  char avail[DV_HISTOGRAM_ENTRY_POOL_SIZE];
  long sz;
  long n;
} dv_histogram_entry_pool_t;

typedef struct dv_histogram {
  dv_histogram_entry_pool_t epool[1];
  dv_histogram_piece_pool_t ppool[1];
  dv_histogram_entry_t * head_e;
  dv_histogram_entry_t * tail_e;
  long n_e;
  char added[DV_DAG_NODE_POOL_SIZE];
  dv_dag_t * D;
  dv_histogram_entry_t * max_e;
} dv_histogram_t;


typedef struct dv_global_state {
  /* DAG */
  dv_pidag_t P[DV_MAX_DAG_FILE];
  dv_dag_t  D[DV_MAX_DAG];
  dv_view_t V[DV_MAX_VIEW];
  int nP;
  int nD;
  int nV;
  dv_llist_cell_t * FL;  
  dv_view_t * activeV;
  int err;
  
  /* Color pools */
  int CP[DV_NUM_COLOR_POOLS][DV_COLOR_POOL_SIZE][4]; // worker, cpu, nodekind, cp1, cp2, cp3
  int CP_sizes[DV_NUM_COLOR_POOLS];
  
  /* GUI */
  GtkWidget * window;
  GtkWidget * vbox0;
  GtkWidget * menubar;
  dv_viewport_t VP[DV_MAX_VIEWPORT];
  int nVP;

  /* Dialogs */
  dv_btsample_viewer_t btviewer[1];
  GtkWidget * box_viewport_configure;

  /* Histograms */
  dv_histogram_t H[DV_MAX_HISTOGRAM];
  int nH;
} dv_global_state_t;

extern const char * const DV_COLORS[];
extern const char * const DV_HISTOGRAM_COLORS[];

extern dv_global_state_t  CS[]; /* global common state */

extern const char * const DV_LINEAR_PATTERN_STOPS[];
extern const int DV_LINEAR_PATTERN_STOPS_NUM;

extern const char * const DV_RADIAL_PATTERN_STOPS[];
extern const int DV_RADIAL_PATTERN_STOPS_NUM;

/*-----------------Headers-----------------*/

/* dagviz.c */
void dv_global_state_init(dv_global_state_t *);
void dv_global_state_set_active_view(dv_view_t *);
dv_view_t * dv_global_state_get_active_view();

void dv_queue_draw(dv_view_t *);
void dv_queue_draw_d(dv_view_t *);
void dv_queue_draw_d_p(dv_view_t *);

void dv_view_clip(dv_view_t *, cairo_t *);
double dv_view_clip_get_bound_left(dv_view_t *);
double dv_view_clip_get_bound_right(dv_view_t *);
double dv_view_clip_get_bound_up(dv_view_t *);
double dv_view_clip_get_bound_down(dv_view_t *);
  
double dv_view_cairo_coordinate_bound_left(dv_view_t *);
double dv_view_cairo_coordinate_bound_right(dv_view_t *);
double dv_view_cairo_coordinate_bound_up(dv_view_t *);
double dv_view_cairo_coordinate_bound_down(dv_view_t *);

void dv_view_status_init(dv_view_t *, dv_view_status_t *);

void dv_view_init(dv_view_t *);
dv_view_t * dv_view_create_new_with_dag(dv_dag_t *);
void * dv_view_interface_set_values(dv_view_t *, dv_view_interface_t *);
dv_view_interface_t * dv_view_interface_create_new(dv_view_t *, dv_viewport_t *);
void dv_view_interface_destroy(dv_view_interface_t *);
void dv_view_change_mainvp(dv_view_t *, dv_viewport_t *);
void dv_view_add_viewport(dv_view_t *, dv_viewport_t *);
void dv_view_remove_viewport(dv_view_t *, dv_viewport_t *);
void dv_view_switch_viewport(dv_view_t *, dv_viewport_t *, dv_viewport_t *);
dv_view_interface_t * dv_view_get_interface_to_viewport(dv_view_t *, dv_viewport_t *);

void dv_viewport_init(dv_viewport_t *);
void dv_viewport_add_interface(dv_viewport_t *, dv_view_interface_t *);
void dv_viewport_remove_interface(dv_viewport_t *, dv_view_interface_t *);
dv_view_interface_t * dv_viewport_get_interface_to_view(dv_viewport_t *, dv_view_t *);

void dv_signal_handler(int);

/* print.c */
char * dv_get_node_kind_name(dr_dag_node_kind_t);
char * dv_get_edge_kind_name(dr_dag_edge_kind_t);
void dv_print_pidag_node(dr_pi_dag_node *, int);
void dv_print_pidag_edge(dr_pi_dag_edge *, int);
void dv_print_pi_string_table(dr_pi_string_table *, int);
void dv_print_dvdag(dv_dag_t *);
void dv_print_layout(dv_dag_t *);
void dv_print_dag_file(char *);
void dv_check_layout(dv_dag_t *);


/* read.c */
dv_pidag_t *     dv_pidag_read_new_file(char *);
dr_pi_dag_node * dv_pidag_get_node_with_id(dv_pidag_t *, long);
dr_pi_dag_node * dv_pidag_get_node(dv_pidag_t *, dv_dag_node_t *);

void            dv_dag_node_pool_init(dv_dag_t *);
int             dv_dag_node_pool_is_empty(dv_dag_t *);
dv_dag_node_t * dv_dag_node_pool_pop(dv_dag_t *);
void            dv_dag_node_pool_push(dv_dag_t *, dv_dag_node_t *);
int             dv_dag_node_pool_avail(dv_dag_t *);
dv_dag_node_t * dv_dag_node_pool_pop_contiguous(dv_dag_t *, long);

void dv_dag_node_init(dv_dag_node_t *, dv_dag_node_t *, long);
int dv_dag_node_set(dv_dag_t *, dv_dag_node_t *);
int  dv_dag_build_node_inner(dv_dag_t *, dv_dag_node_t *);
int dv_dag_destroy_node_innner(dv_dag_t *, dv_dag_node_t *);
void dv_dag_clear_shrinked_nodes(dv_dag_t *);
void dv_dag_init(dv_dag_t *, dv_pidag_t *);
dv_dag_t * dv_dag_create_new_with_pidag(dv_pidag_t *);
double dv_dag_get_radix(dv_dag_t *);
void dv_dag_set_radix(dv_dag_t *, double);

void dv_btsample_viewer_init(dv_btsample_viewer_t *);
int dv_btsample_viewer_extract_interval(dv_btsample_viewer_t *, int, unsigned long long, unsigned long long);


/* layout.c */
void dv_view_layout(dv_view_t *);
double dv_view_calculate_rate(dv_view_t *, dv_dag_node_t *);
double dv_view_calculate_reverse_rate(dv_view_t *, dv_dag_node_t *);
double dv_get_time();
void dv_animation_init(dv_view_t *V, dv_animation_t *);
void dv_animation_start(dv_animation_t *);
void dv_animation_stop(dv_animation_t *);
void dv_animation_add(dv_animation_t *, dv_dag_node_t *);
void dv_animation_remove(dv_animation_t *, dv_dag_node_t *);
void dv_animation_reverse(dv_animation_t *, dv_dag_node_t *);

void dv_motion_init(dv_motion_t *, dv_view_t *);
void dv_motion_reset_target(dv_motion_t *, long, double, double, double, double);
void dv_motion_start(dv_motion_t *, long, double, double, double, double);
void dv_motion_stop(dv_motion_t *);

/* draw.c */
void dv_draw_text(cairo_t *);
void dv_draw_rounded_rectangle(cairo_t *, double, double, double, double);
void dv_draw_path_isosceles_triangle(cairo_t *, double, double, double, double);
void dv_draw_path_isosceles_triangle_upside_down(cairo_t *, double, double, double, double);
void dv_draw_path_rectangle(cairo_t *, double, double, double, double);
void dv_draw_path_rounded_rectangle(cairo_t *, double, double, double, double);
void dv_draw_path_circle(cairo_t *, double, double, double);
void dv_lookup_color_value(int, double *, double *, double *, double *);
void dv_lookup_color(dr_pi_dag_node *, int, double *, double *, double *, double *);
cairo_pattern_t * dv_create_color_linear_pattern(int *, int, double, double);
cairo_pattern_t * dv_get_color_linear_pattern(double, double);
cairo_pattern_t * dv_get_color_radial_pattern(double, double);
double dv_view_get_alpha_fading_out(dv_view_t *, dv_dag_node_t *);
double dv_view_get_alpha_fading_in(dv_view_t *, dv_dag_node_t *);
void dv_view_draw_edge_1(dv_view_t *, cairo_t *, dv_dag_node_t *, dv_dag_node_t *);
void dv_view_draw_status(dv_view_t *, cairo_t *, int);
void dv_view_draw_infotag_1(dv_view_t *, cairo_t *, cairo_matrix_t *, dv_dag_node_t *);
void dv_view_draw_infotags(dv_view_t *, cairo_t *, cairo_matrix_t *);
void dv_view_draw(dv_view_t *, cairo_t *);
void dv_viewport_draw_label(dv_viewport_t *, cairo_t *);

/* view_glike.c */
void dv_view_layout_glike(dv_view_t *);
void dv_view_draw_glike(dv_view_t *, cairo_t *);

/* view_bbox.c */
double dv_view_calculate_hsize(dv_view_t *, dv_dag_node_t *);
double dv_view_scale_down(dv_view_t *, double);
double dv_dag_scale_down(dv_dag_t *, double);
double dv_view_calculate_vsize(dv_view_t *, dv_dag_node_t *);
void dv_view_layout_bbox(dv_view_t *);
void dv_view_draw_bbox(dv_view_t *, cairo_t *);

/* view_timeline.c */
void dv_view_layout_timeline(dv_view_t *);
void dv_view_draw_timeline(dv_view_t *, cairo_t *);

/* view_timeline2.c */
void dv_view_layout_timeline2(dv_view_t *);
void dv_view_draw_timeline2(dv_view_t *, cairo_t *);

/* view_paraprof.c */
void dv_histogram_init(dv_histogram_t *);
void dv_histogram_add_node(dv_histogram_t *, dv_dag_node_t *);
void dv_histogram_draw(dv_histogram_t *, cairo_t *);
void dv_histogram_reset(dv_histogram_t *);

void dv_view_layout_paraprof(dv_view_t *);
void dv_view_draw_paraprof(dv_view_t *, cairo_t *);


/* utils.c */
dv_linked_list_t * dv_linked_list_create();
void dv_linked_list_destroy(dv_linked_list_t *);
void dv_linked_list_init(dv_linked_list_t *);
void * dv_linked_list_remove(dv_linked_list_t *, void *);
void dv_linked_list_add(dv_linked_list_t *, void *);
  
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

const char * dv_convert_char_to_binary(int );
double dv_max(double, double);
double dv_min(double, double);


/*-----------------Inlines-----------------*/

static void * dv_malloc(size_t);
static void dv_free(void *, size_t);

static void dv_get_callpath_by_backtrace() {
  fprintf(stderr, "Call path by backtrace:\n");
  
  int len = 15;
  void * s[len];// = (void **) dv_malloc(sizeof(void *) * size);
  int n, i;    
  n = backtrace(s, len);
  char ** ss = backtrace_symbols(s, n);
  for (i=0; i<n; i++)
    fprintf(stderr, " %d: %s\n", i, ss[i]);
}

static void dv_get_callpath_by_libunwind() {
#ifdef ENABLE_LIBUNWIND
  fprintf(stderr, "Call path by libunwind:\n");
  
  unw_cursor_t cursor; unw_context_t uc;
  unw_word_t ip, sp, offp;
  int i = 0;
  int len = 30;
  char bufp[len];
  int ret;
    
  unw_getcontext(&uc);
  unw_init_local(&cursor, &uc);
  while (ret = unw_step(&cursor) > 0) {
    unw_get_proc_name(&cursor, bufp, len, &offp);
    unw_get_reg(&cursor, UNW_REG_IP, &ip);
    unw_get_reg(&cursor, UNW_REG_SP, &sp);
    fprintf (stderr, " %d: %s, ip = %p, sp = %p\n", i++, bufp, (void *) ip, (void *) sp);
    if (i == 15) break;
  }
#endif  
}

static int dv_check_(int condition, const char * condition_s, 
                     const char * __file__, int __line__, 
                     const char * func) {
  if (!condition) {
    fprintf(stderr, "%s:%d:%s: check failed : %s\n", 
            __file__, __line__, func, condition_s);
    //dv_get_callpath_by_backtrace();
    dv_get_callpath_by_libunwind();
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

static int dv_is_set(dv_dag_node_t *node) {
  if (!node) return 0;
  return dv_node_flag_check(node->f, DV_NODE_FLAG_SET);
}

static int dv_is_union(dv_dag_node_t *node) {
  if (!node) return 0;
  return dv_node_flag_check(node->f, DV_NODE_FLAG_UNION);
}

static int dv_is_inner_loaded(dv_dag_node_t *node) {
  if (!node) return 0;
  return dv_node_flag_check(node->f, DV_NODE_FLAG_INNER_LOADED);
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
  if (!dv_is_set(node)
      || !dv_is_union(node) || !dv_is_inner_loaded(node)
      || (dv_is_shrinked(node) && !dv_is_expanding(node)))
    return 1;
  
  return 0;
}

static int dv_is_visible(dv_dag_node_t *node) {
  if (!node) return 0;
  dv_check(dv_is_set(node));
  if (!node->parent || dv_is_expanded(node->parent)) {
    if (!dv_is_union(node) || !dv_is_inner_loaded(node) || dv_is_shrinked(node))
      return 1;
  }
  return 0;
}

static int dv_is_inward_callable(dv_dag_node_t *node) {
  if (!node) return 0;
  dv_check(dv_is_set(node));
  if (dv_is_union(node) && dv_is_inner_loaded(node)
      && ( dv_is_expanded(node) || dv_is_expanding(node) ))
    return 1;
  return 0;
}

static int dv_log_set_error(int err) {
  CS->err = err;
  fprintf(stderr, "CS->err = %d\n", err);
  return err;
}

#endif /* DAGVIZ_HEADER_ */
