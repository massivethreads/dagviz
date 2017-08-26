/*
 * dagviz-gtk.h
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
#include <gdk/gdkkeysyms.h>
#include <cairo.h>
#include <cairo-ps.h>
#include <cairo-svg.h>
#include <cairo-pdf.h>

#include <fcntl.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>

#include <dag_recorder_impl.h>

#include <execinfo.h>

#ifdef DV_ENABLE_LIBWIND
#define UNW_LOCAL_ONLY
#include <libunwind.h>
#endif

#include <sys/time.h>
#include <unistd.h>
#include <signal.h>
#include <ctype.h> /* for isdigit() */

#ifdef DV_ENABLE_BFD
#include <bfd.h>
#endif

#include <glob.h>

#include "dagmodel.h"


/*-----------------Constants-----------------*/

#define NUM_COLORS 34

#define DV_NODE_FLAG_NONE         0        /* none */
#define DV_NODE_FLAG_SET          1        /* none - set */
#define DV_NODE_FLAG_UNION        (1 << 1) /* single - union */
#define DV_NODE_FLAG_INNER_LOADED (1 << 2) /* no inner - inner loaded */
#define DV_NODE_FLAG_SHRINKED     (1 << 3) /* expanded - shrinked */
#define DV_NODE_FLAG_EXPANDING    (1 << 4) /* expanding */
#define DV_NODE_FLAG_SHRINKING    (1 << 5) /* shrinking */

#define DV_NODE_FLAG_CRITICAL_PATH_0 (1 << 6) /* node is on critical path of work */
#define DV_NODE_FLAG_CRITICAL_PATH_1 (1 << 7) /* node is on critical path of work & delay */
#define DV_NODE_FLAG_CRITICAL_PATH_2 (1 << 8) /* node is on critical path of weighted work & delay */

#define DV_STRING_LENGTH 1000
#define DV_STATUS_PADDING 7
#define DV_SAFE_CLICK_RANGE 1

#define DV_RADIX_LOG 1.8
#define DV_RADIX_POWER 0.42
#define DV_RADIX_LINEAR 100000 //100000

#define DV_ANIMATION_DURATION 400 // milliseconds
#define DV_ANIMATION_STEP 80 // milliseconds

#define DV_NUM_LAYOUT_TYPES 8
#define DV_LAYOUT_TYPE_DAG 0
#define DV_LAYOUT_TYPE_DAG_BOX_LOG 1
#define DV_LAYOUT_TYPE_DAG_BOX_POWER 2
#define DV_LAYOUT_TYPE_DAG_BOX_LINEAR 3
#define DV_LAYOUT_TYPE_TIMELINE 4
#define DV_LAYOUT_TYPE_TIMELINE_VER 5
#define DV_LAYOUT_TYPE_PARAPROF 6
#define DV_LAYOUT_TYPE_CRITICAL_PATH 7
#define DV_LAYOUT_TYPE_INIT 0 // not paraprof coz need to check H of D

/*
#define DV_LAYOUT_TYPE_DAG_BOX_LOG 1
#define DV_LAYOUT_TYPE_TIMELINE 3
#define DV_LAYOUT_TYPE_TIMELINE_VER 2
#define DV_LAYOUT_TYPE_PARAPROF 4
#define DV_LAYOUT_TYPE_CRITICAL_PATH 5
#define DV_LAYOUT_TYPE_INIT 0 // not paraprof coz need to check H of D
#define DV_LAYOUT_TYPE_DAG_BOX_POWER 6
#define DV_LAYOUT_TYPE_DAG_BOX_LINEAR 7
*/

#define DV_NODE_COLOR_INIT 0 // 0:worker, 1:cpu, 2:kind, 3:code start, 4:code end, 5: code segment
//#define DV_SCALE_TYPE_INIT 0 // 0:log, 1:power, 2:linear
#define DV_FROMBT_INIT 0
#define DV_EDGE_TYPE_INIT 3
#define DV_EDGE_AFFIX_LENGTH 0//10
#define DV_CLICK_MODE_INIT 2 // 0:none, 1:info, 2:expand/collapse
#define DV_HOVER_MODE_INIT 6 // 0:none, 1:info, 2:expand, 3:collapse, 4:expand/collapse, 5:remark similar nodes, 6:highlight
#define DV_SHOW_LEGEND_INIT 0
#define DV_SHOW_STATUS_INIT 0
#define DV_REMAIN_INNER_INIT 1
#define DV_COLOR_REMARKED_ONLY 1

#define DV_COLOR_POOL_SIZE 100
#define DV_NUM_COLOR_POOLS 6

#define DV_TIMELINE_NODE_WITH_BORDER 0
#define DV_ENTRY_RADIX_MAX_LENGTH 20

#define DV_MAX_VIEW 100
#define DV_MAX_VIEWPORT 100

#define DV_OK 0

#define backtrace_sample_sz 15

#define DV_HISTOGRAM_PIECE_POOL_SIZE 100000
#define DV_HISTOGRAM_ENTRY_POOL_SIZE 40000

#define DV_HISTOGRAM_DIVIDE_TO_PIECES 0

#define DV_CAIRO_BOUND_MIN -8e6 //-(1<<23)
#define DV_CAIRO_BOUND_MAX 8e6 //(1<<23)

#define DV_MAX_NUM_REMARKS 32

#define DV_DEFAULT_PAGE_SIZE 1 << 20 // 1 MB
#define DV_DEFAULT_PAGE_MIN_ENTRIES 2

#define DV_MAX_DISTRIBUTION 10

#define DV_STAT_DISTRIBUTION_OUTPUT_DEFAULT_NAME "00dv_stat_distribution.gpl"
#define DV_STAT_BREAKDOWN_OUTPUT_DEFAULT_NAME "00dv_stat_breakdown.gpl"
#define DV_STAT_BREAKDOWN_OUTPUT_DEFAULT_NAME_2 "00dv_stat_critical_path_breakdown.gpl"

#define DV_VIEW_AUTO_ZOOMFIT_INIT 4 /* 0:none, 1:hor, 2:ver, 3:based, 4:full */
#define DV_VIEW_ADJUST_AUTO_ZOOMFIT_INIT 1

#define _unused_ __attribute__((unused))
#define _static_unused_ static __attribute__((unused))

#define DV_PARAPROF_MIN_ENTRY_INTERVAL 20000

#define DV_VERBOSE_LEVEL_DEFAULT 0

#define DV_DAG_NODE_HIDDEN_ABOVE 0b0001
#define DV_DAG_NODE_HIDDEN_RIGHT 0b0010
#define DV_DAG_NODE_HIDDEN_BELOW 0b0100
#define DV_DAG_NODE_HIDDEN_LEFT  0b1000

#define DV_NUM_CRITICAL_PATHS 3
#define DV_CRITICAL_PATH_0 0 /* most work */
#define DV_CRITICAL_PATH_1 1 /* last finished tails */
#define DV_CRITICAL_PATH_2 2 /* most problematic delay */
#define DV_CRITICAL_PATH_0_COLOR "red"
#define DV_CRITICAL_PATH_1_COLOR "green"
#define DV_CRITICAL_PATH_2_COLOR "blue"


/*-----------------Data Structures-----------------*/

/* a single record of backtrace */
typedef struct {
  unsigned long long tsc;
  int worker;
  int n;
  void * frames[backtrace_sample_sz];
} bt_sample_t;

typedef struct dv_pidag {
  GtkWidget * mini_frame;
} dv_pidag_t;

typedef struct dv_dag {
  int mV[DV_MAX_VIEW]; /* mark Vs that are bound to this D */
  int nviews[DV_NUM_LAYOUT_TYPES]; /* num of views referencing each layout type */
  int ar[DV_MAX_NUM_REMARKS];
  int nr;

  /* DAG management window */
  GtkWidget * mini_frame;
  GtkWidget * views_box;
  GtkWidget * status_label;
} dv_dag_t;

typedef struct dv_view dv_view_t;

typedef struct dv_animation {
  int on; /* on/off */
  double duration; /* milliseconds */
  double step; /* milliseconds */
  dm_llist_t movings[1]; /* list of moving nodes */
  dv_view_t * V; /* view that possesses this animation */
} dv_animation_t;

typedef struct dv_motion {
  int on;
  double duration;
  double step;
  dv_view_t * V;
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
  int coord; /* index of coordinate to use in node structure's c array */
  int et; /* edge type */
  int edge_affix; /* edge affix length */
  int cm; /* click mode */
  long ndh; /* number of nodes including hidden ones */
  int focused;
  long nl; /* number of nodes laid-out */
  long ntr; /* number of nodes travered */

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
  int auto_zoomfit; /* 0:none, 1:hor, 2:ver, 3:based, 4:full */
  int adjust_auto_zoomfit;

  /* moving animation */
  dv_motion_t m[1];

  dm_dag_node_t * last_hovered_node;
  int hm; /* hovering mode: 0:info, 1:expand, 2:collapse, 3:expand/collapse */
  int show_legend;
  int show_status;
  int remain_inner;
  int color_remarked_only;
  
  int nviewports; /* num of viewports */
} dv_view_status_t;

typedef struct dv_viewport dv_viewport_t;
typedef struct dv_view dv_view_t;

typedef struct dv_view_toolbox {
  dv_view_t * V;
  GtkWidget * window;

  /* Common */
  GtkWidget * combobox_lt;
  GtkWidget * combobox_nc;
  //GtkWidget * combobox_sdt;
  GtkWidget * entry_radix;
  GtkWidget * checkbox_scale_radix;
  GtkWidget * combobox_cm;
  GtkWidget * combobox_hm;
  GtkWidget * checkbox_legend;
  GtkWidget * checkbox_status;
  GtkWidget * combobox_azf;
  GtkWidget * checkbox_azf;
  GtkWidget * entry_paraprof_min_interval;

  /* Advance */
  GtkWidget * entry_remark;
  GtkWidget * checkbox_remain_inner;
  GtkWidget * checkbox_color_remarked_only;

  /* Developer */
  GtkWidget * entry_search;
  GtkWidget * checkbox_xzoom;
  GtkWidget * checkbox_yzoom;
  GtkWidget * checkbox_scale_radius;  
  GtkWidget * combobox_frombt;
  GtkWidget * combobox_et;
  GtkWidget * togg_eaffix;
} dv_view_toolbox_t;

typedef struct dv_view {
  char * name;
  dm_dag_t * D; /* DV DAG */
  dv_view_status_t S[1]; /* layout/drawing attributes */
  dv_viewport_t * mainVP; /* main VP that this V is assosiated with */
  dv_view_toolbox_t T[1];
  int mVP[DV_MAX_VIEWPORT]; /* mark VPs on which this V is displayed */
} dv_view_t;

typedef struct dv_viewport_toolbox {
  GtkWidget * toolbar;
  GtkWidget * label;
} dv_viewport_toolbox_t;

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
  double vpw, vph;  /* viewport's size */
  int mV[DV_MAX_VIEW]; /* mark Vs that this VP contains */
  dv_view_t * mainV;
  dv_viewport_toolbox_t T[1];

  /* Viewport management window */
  /* left */
  GtkWidget * mini_frame;
  GtkWidget * mini_paned;  
  /* right */
  GtkWidget * mini_frame_2;
  GtkWidget * mini_frame_2_box;
  GtkWidget * split_combobox;
  GtkWidget * orient_combobox;
  GtkWidget * dag_menubutton;
  GtkWidget * dag_menu;
  double x, y; /* current position of mouse pointer */
} dv_viewport_t;

typedef struct dv_btsample_viewer {
  dm_pidag_t * P;
  GtkWidget * label_dag_file_name;
  GtkWidget * entry_bt_file_name;
  GtkWidget * entry_binary_file_name;
  GtkWidget * combobox_worker;
  GtkWidget * entry_time_from;
  GtkWidget * entry_time_to;
  GtkWidget * text_view;
  GtkWidget * entry_node_id;
} dv_btsample_viewer_t;


typedef struct dv_stat_distribution_entry {
  int dag_id;
  int type; /* 0:spawn, 1:cont */
  int stolen; /* stolen condition */
  char * title;
  GtkWidget * title_entry;
} dv_stat_distribution_entry_t;

typedef struct dv_stat_distribution {
  int ne;
  dv_stat_distribution_entry_t e[DV_MAX_DISTRIBUTION];
  long xrange_from, xrange_to;
  GtkWidget * node_pool_label;
  GtkWidget * entry_pool_label;
  char * fn;
  int bar_width;
} dv_stat_distribution_t;


typedef struct dv_gui {
  /* Main window */
  GtkBuilder * builder;
  GtkWidget * window;
  GtkWidget * window_box;
  GtkWidget * menubar;
  GtkWidget * main_box;
  GtkAccelGroup * accel_group;
  GtkWidget * context_menu;
  GtkWidget * left_sidebar;

  /* Toolbar */
  GtkWidget * toolbar;
  GtkWidget * dag_menu;
  GtkWidget * division_menu;
  
  /* Status bars */
  GtkWidget * statusbar1; // interaction statuses
  GtkWidget * statusbar2; // selection statuses
  GtkWidget * statusbar3; // pool status
  GtkWidget * status_x;
  GtkWidget * status_y;
  GtkWidget * status_zx;
  GtkWidget * status_zy;

  /* Management window */
  GtkWidget * management_window;
  GtkWidget * notebook;
  GtkWidget * scrolled_box; // DAGs tab's scrolled box
  GtkWidget * dag_file_scrolled_box; // DAG files tab's scrolled box
  GtkWidget * create_dag_menu;

  /* Replay sidebox */
  struct replay {
    GtkWidget * sidebox;
    GtkWidget * enable;
    GtkWidget * scale;
    GtkWidget * entry;
    GtkWidget * time_step_entry;
    GtkWidget * dag_menu;
    int mD[DM_MAX_DAG];
  } replay;
  
  /* Node info sidebox */
  struct nodeinfo {
    GtkWidget * sidebox;
    GtkWidget * offset;
    GtkWidget * type;
    GtkWidget * depth;
    GtkWidget * cur_node_count;
    GtkWidget * start_time;
    GtkWidget * end_time;
    GtkWidget * duration;    
    GtkWidget * first_ready_t;
    GtkWidget * est;
    //GtkWidget * t1inf;
    GtkWidget * t1;
    GtkWidget * tinf;
    GtkWidget * counters;
    GtkWidget * worker;
    GtkWidget * cpu;
    GtkWidget * start_pos;
    GtkWidget * end_pos;
  } nodeinfo;
  
} dv_gui_t;

/* runtime-adjustable options */
typedef struct dv_options {
  double zoom_step_ratio;
  double scale_step_ratio;
  
  double hnd; /* horizontal node distance */
  double vnd; /* vertical node distance */
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
  dv_view_t V[DV_MAX_VIEW];
  int nV;
  dv_view_t * activeV;
  dv_viewport_t * activeVP;
  int error;
  
  /* Color pools */
  int CP[DV_NUM_COLOR_POOLS][DV_COLOR_POOL_SIZE][4]; // worker, cpu, nodekind, cp1, cp2, cp3
  int CP_sizes[DV_NUM_COLOR_POOLS];
  
  /* GUI */
  dv_viewport_t VP[DV_MAX_VIEWPORT];
  int nVP;

  /* Backtrace viewer */
  dv_btsample_viewer_t btviewer[1];

  /* Statistics */
  dv_stat_distribution_t SD[1];

  dv_view_t * context_view;
  dm_dag_node_t * context_node;

  int verbose_level;

  char * cp_colors[DV_NUM_CRITICAL_PATHS];

  dv_options_t opts;
} dv_global_state_t;

extern const char * const DV_COLORS[];
extern const char * const DV_HISTOGRAM_COLORS[];

extern dv_global_state_t  DVG[]; /* dagviz's global state */
extern dv_gui_t GUI[]; /* dagviz's global GUI structure */

extern const char * const DV_LINEAR_PATTERN_STOPS[];
extern const int DV_LINEAR_PATTERN_STOPS_NUM;

extern const char * const DV_RADIAL_PATTERN_STOPS[];
extern const int DV_RADIAL_PATTERN_STOPS_NUM;

/*-----------------Headers-----------------*/


/* dagviz-gtk.c */
/* interface.c */
void dv_global_state_init();

void dv_queue_draw_viewport(dv_viewport_t *);
void dv_queue_draw_view(dv_view_t *);
void dv_queue_draw_dag(dm_dag_t *);
void dv_queue_draw_pidag(dm_pidag_t *);

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

void dv_viewport_draw(dv_viewport_t *, cairo_t *, int);

void dv_view_toolbox_init(dv_view_toolbox_t *, dv_view_t *);

void dv_view_init(dv_view_t *);
dv_view_t * dv_view_create_new_with_dag(dm_dag_t *);
void dv_view_open_toolbox_window(dv_view_t *);
void dv_view_change_mainvp(dv_view_t *, dv_viewport_t *);
void dv_view_add_viewport(dv_view_t *, dv_viewport_t *);
void dv_view_remove_viewport(dv_view_t *, dv_viewport_t *);
void dv_view_switch_viewport(dv_view_t *, dv_viewport_t *, dv_viewport_t *);

void override_background_color(GtkWidget *, GdkRGBA *);
void dv_viewport_init(dv_viewport_t *);
void dv_viewport_miniver2_show(dv_viewport_t *);
void dv_viewport_miniver_show(dv_viewport_t *);
void dv_viewport_show(dv_viewport_t *);
void dv_viewport_change_split(dv_viewport_t *, int);
void dv_viewport_change_orientation(dv_viewport_t *, GtkOrientation);
void dv_viewport_change_mainv(dv_viewport_t *, dv_view_t *);
void dv_viewport_add_view(dv_viewport_t *, dv_view_t *);
void dv_viewport_remove_view(dv_viewport_t *, dv_view_t *);

void dv_viewport_divide_onedag_1(dv_viewport_t *, dm_dag_t *);
void dv_viewport_divide_onedag_2(dv_viewport_t *, dm_dag_t *);
void dv_viewport_divide_onedag_3(dv_viewport_t *, dm_dag_t *);
void dv_viewport_divide_onedag_4(dv_viewport_t *, dm_dag_t *);
void dv_viewport_divide_onedag_5(dv_viewport_t *, dm_dag_t *);
void dv_viewport_divide_onedag_6(dv_viewport_t *, dm_dag_t *);
void dv_viewport_divide_onedag_7(dv_viewport_t *, dm_dag_t *);

void dv_viewport_divide_twodags_1(dv_viewport_t *, dm_dag_t *, dm_dag_t *);
void dv_viewport_divide_twodags_2(dv_viewport_t *, dm_dag_t *, dm_dag_t *);
void dv_viewport_divide_twodags_3(dv_viewport_t *, dm_dag_t *, dm_dag_t *);
void dv_viewport_divide_twodags_4(dv_viewport_t *, dm_dag_t *, dm_dag_t *);
void dv_viewport_divide_twodags_5(dv_viewport_t *, dm_dag_t *, dm_dag_t *);
void dv_viewport_divide_twodags_6(dv_viewport_t *, dm_dag_t *, dm_dag_t *);
void dv_viewport_divide_twodags_7(dv_viewport_t *, dm_dag_t *, dm_dag_t *);

void dv_viewport_divide_threedags_1(dv_viewport_t *, dm_dag_t *, dm_dag_t *, dm_dag_t *);

void dv_open_statistics_dialog();
char * dv_choose_a_new_dag_file();

void dv_change_focused_viewport(dv_viewport_t *);
void dv_switch_focused_viewport();
void dv_switch_focused_view();
void dv_switch_focused_view_inside_viewport();

void dv_open_dr_stat_file(dm_pidag_t *);
void dv_open_dr_pp_file(dm_pidag_t *);

dv_view_t * dv_create_new_view(dm_dag_t *);
dm_dag_t * dv_create_new_dag(dm_pidag_t *);
dm_pidag_t * dv_create_new_pidag(char *);

void dv_gui_init(dv_gui_t *);
GtkWidget * dv_gui_get_management_window(dv_gui_t *);
GtkWidget * dv_gui_get_main_window(dv_gui_t *, GtkApplication *);
void dv_dag_set_time_step(dm_dag_t *, double);
void dv_dag_set_current_time(dm_dag_t *, double);
void dv_dag_set_draw_current_time_active(dm_dag_t *, int);
void dv_gui_replay_sidebox_set_dag(dm_dag_t *, int);
GtkWidget * dv_gui_get_replay_sidebox(dv_gui_t *);
void dv_gui_nodeinfo_set_node(dv_gui_t *, dm_dag_node_t *, dm_dag_t *);
GtkWidget * dv_gui_get_nodeinfo_sidebox(dv_gui_t *);

void dv_signal_handler(int);

void dv_btsample_viewer_init(dv_btsample_viewer_t *);
int dv_btsample_viewer_extract_interval(dv_btsample_viewer_t *, int, unsigned long long, unsigned long long);


/* control.c */
char * dv_filename_get_short_name(char *);
void dv_dag_collect_delays_r(dm_dag_t *, dm_dag_node_t *, FILE *, dv_stat_distribution_entry_t *);
void dv_dag_collect_sync_delays_r(dm_dag_t *, dm_dag_node_t *, FILE *, dv_stat_distribution_entry_t *);
void dv_dag_collect_intervals_r(dm_dag_t *, dm_dag_node_t *, FILE *, dv_stat_distribution_entry_t *);
void dv_dag_expand_implicitly(dm_dag_t *);
void dv_dag_update_status_label(dm_dag_t *);

void dv_dag_node_pool_set_status_label(dm_dag_node_pool_t *, GtkWidget *);
void dv_histogram_entry_pool_set_status_label(dm_histogram_entry_pool_t *, GtkWidget *);

void dv_view_get_zoomfit_hor(dv_view_t *, double *, double *, double *, double *);
void dv_view_do_zoomfit_hor(dv_view_t *);
void dv_view_get_zoomfit_ver(dv_view_t *, double *, double *, double *, double *);
void dv_view_do_zoomfit_ver(dv_view_t *);
void dv_view_do_zoomfit_based_on_lt(dv_view_t *);
void dv_view_do_zoomfit_full(dv_view_t *);
double dv_view_get_radix(dv_view_t *);
void dv_view_change_radix(dv_view_t *, double);
void dv_view_set_entry_radix_text(dv_view_t *);
void dv_view_set_entry_paraprof_resolution(dv_view_t *);
void dv_view_set_entry_remark_text(dv_view_t *, char *);
//void dv_view_change_sdt(dv_view_t *, int);
void dv_view_change_eaffix(dv_view_t *, int);
void dv_view_change_nc(dv_view_t *, int);
void dv_view_change_lt(dv_view_t *, int);
void dv_view_change_azf(dv_view_t *, int);
void dv_view_auto_zoomfit(dv_view_t *);
void dv_view_status_set_coord(dv_view_status_t *);
void dv_view_status_init(dv_view_t *, dv_view_status_t *);

void dv_view_scan(dv_view_t *);
void dv_view_remark_similar_nodes(dv_view_t *, dm_dag_node_t *);

void dv_statusbar_update(int, int, char *);
void dv_statusbar_remove(int, int);
void dv_statusbar_update_selection_status();
void dv_statusbar_update_pool_status();
void dv_statusbar_update_pointer_status();

void dv_export_viewport();
void dv_export_all_viewports();

void dv_do_scrolling(dv_view_t *, GdkEventScroll *);
void dv_do_expanding_one(dv_view_t *);
void dv_do_collapsing_one(dv_view_t *);
void dv_do_button_event(dv_view_t *, GdkEventButton *);
void dv_do_motion_event(dv_view_t *, GdkEventMotion *);
dm_dag_node_t * dv_find_node_with_pii_r(dv_view_t *, long, dm_dag_node_t *);

void dv_dag_build_inner_all(dm_dag_t *);
void dv_dag_compute_critical_paths(dm_dag_t *);



/* graphs.c */
void dv_statistics_graph_delay_distribution();
void dv_statistics_graph_execution_time_breakdown();
void dv_statistics_graph_critical_path_breakdown(char *);
void dv_statistics_graph_critical_path_delay_breakdown(char *);
void dv_statistics_graph_critical_path_edge_based_delay_breakdown(char *);


/* layout.c */
void dv_view_layout(dv_view_t *);
double dv_view_calculate_rate(dv_view_t *, dm_dag_node_t *);
double dv_view_calculate_reverse_rate(dv_view_t *, dm_dag_node_t *);
void dv_animation_init(dv_view_t *V, dv_animation_t *);
void dv_animation_start(dv_animation_t *);
void dv_animation_stop(dv_animation_t *);
void dv_animation_add(dv_animation_t *, dm_dag_node_t *);
void dv_animation_remove(dv_animation_t *, dm_dag_node_t *);
void dv_animation_reverse(dv_animation_t *, dm_dag_node_t *);

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
cairo_pattern_t * dv_get_color_linear_pattern(dv_view_t *, double, double, long long);
cairo_pattern_t * dv_get_color_radial_pattern(double, double);
double dv_view_get_alpha_fading_out(dv_view_t *, dm_dag_node_t *);
double dv_view_get_alpha_fading_in(dv_view_t *, dm_dag_node_t *);
void dv_view_draw_edge_1(dv_view_t *, cairo_t *, dm_dag_node_t *, dm_dag_node_t *);
void dv_view_draw_status(dv_view_t *, cairo_t *, int);
void dv_view_draw_infotag_1(dv_view_t *, cairo_t *, cairo_matrix_t *, dm_dag_node_t *);
void dv_view_draw_infotags(dv_view_t *, cairo_t *, cairo_matrix_t *);
void dv_view_draw(dv_view_t *, cairo_t *);
void dv_viewport_draw_label(dv_viewport_t *, cairo_t *);
void dv_view_draw_legend(dv_view_t *, cairo_t *);
void dv_viewport_draw_focused_mark(dv_viewport_t *, cairo_t *);
double dv_view_convert_graph_x_to_viewport_x(dv_view_t *, double);
double dv_view_convert_graph_y_to_viewport_y(dv_view_t *, double);
double dv_view_convert_viewport_x_to_graph_x(dv_view_t *, double);
double dv_view_convert_viewport_y_to_graph_y(dv_view_t *, double);
void dv_convert_tick_value_to_simplified_string(double, char *);
void dv_viewport_draw_rulers(dv_viewport_t *, cairo_t *);


/* view_dag.c */
dm_dag_node_t * dv_view_dag_find_clicked_node(dv_view_t *, double, double);
int dv_rectangle_is_invisible(dv_view_t *, double, double, double, double);
int dv_rectangle_trim(dv_view_t *, double *, double *, double *, double *);
int dm_dag_node_is_invisible(dv_view_t *, dm_dag_node_t *);
int dm_dag_node_link_is_invisible(dv_view_t *, dm_dag_node_t *);
void dv_view_layout_dag(dv_view_t *);
void dv_view_draw_dag(dv_view_t *, cairo_t *);
void dv_view_draw_legend_dag(dv_view_t *, cairo_t *);


/* view_dag_box.c */
//double dv_dag_scale_down(dm_dag_t *, double);
double dv_dag_scale_down_linear(dm_dag_t *, double);
double dv_view_scale_down_linear(dv_view_t *, double);
double dv_view_scale_down(dv_view_t *, double);
double dv_view_calculate_vsize(dv_view_t *, dm_dag_node_t *);
void dv_view_layout_dagbox(dv_view_t *);
void dv_view_draw_dagbox(dv_view_t *, cairo_t *);


/* view_timeline.c */
dm_dag_node_t * dv_timeline_find_clicked_node(dv_view_t *, double, double);
void dv_timeline_trim_rectangle(dv_view_t *, double *, double *, double *, double *);
int dv_timeline_node_is_invisible(dv_view_t *, dm_dag_node_t *);
void dv_view_layout_timeline(dv_view_t *);
void dv_view_draw_timeline(dv_view_t *, cairo_t *);


/* view_timeline_ver.c */
void dv_view_layout_timeline_ver(dv_view_t *);
void dv_view_draw_timeline_ver(dv_view_t *, cairo_t *);


/* view_paraprof.c */
void dv_histogram_draw(dm_histogram_t *, cairo_t *, dv_view_t *);
void dv_view_layout_paraprof(dv_view_t *);
void dv_paraprof_draw_rulers(dv_viewport_t *, dv_view_t *, cairo_t *);
void dv_view_draw_paraprof(dv_view_t *, cairo_t *);

dm_dag_node_t * dv_critical_path_find_clicked_node(dv_view_t *, double, double);
void dv_view_layout_critical_path(dv_view_t *);
void dv_view_draw_critical_path(dv_view_t *, cairo_t *);



/*-----------------Inlines-----------------*/

#define dv_malloc dm_malloc
#define dv_free dm_free

_static_unused_ void
dv_get_callpath_by_backtrace() {
  fprintf(stderr, "Call path by backtrace:\n");
  
  int len = 15;
  void * s[len];// = (void **) dv_malloc(sizeof(void *) * size);
  int n, i;    
  n = backtrace(s, len);
  char ** ss = backtrace_symbols(s, n);
  for (i=0; i<n; i++)
    fprintf(stderr, " %d: %s\n", i, ss[i]);
}

_static_unused_ void
dv_get_callpath_by_libunwind() {
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

_static_unused_ int
dv_check_(int condition, const char * condition_s, 
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

_static_unused_ int
dv_log_set_error(int err) {
  DVG->error = err;
  fprintf(stderr, "DVG->error = %d\n", err);
  return err;
}


_static_unused_ void
dv_longlong_init(long long * w) {
  int i;
  for (i=0; i<64; i++) {
    long long v = 1 << i;
    if ((*w & v) == v)
      *w -= v;
  }
}

_static_unused_ void
dv_set_bit(long long * w, int v) {
  long long vv = 1 << v;
  if ((*w & vv) != vv)
    *w += vv;
}

_static_unused_ int
dv_get_bit(long long w, int v) {
  long long vv = 1 << v;
  return ((w & vv) == vv);
}


_static_unused_ int
dv_get_color_pool_index(int t, int v0, int v1, int v2, int v3) {
  int n = DVG->CP_sizes[t];
  int i;
  for (i=0; i<n; i++) {
    if (DVG->CP[t][i][0] == v0 && DVG->CP[t][i][1] == v1
        && DVG->CP[t][i][2] == v2 && DVG->CP[t][i][3] == v3)
      return i;
  }
  dv_check(n < DV_COLOR_POOL_SIZE);
  DVG->CP[t][n][0] = v0;
  DVG->CP[t][n][1] = v1;
  DVG->CP[t][n][2] = v2;
  DVG->CP[t][n][3] = v3;
  DVG->CP_sizes[t]++;
  return n;
}

_static_unused_ int
dv_pidag_node_lookup_value(dr_pi_dag_node * pi, int nc) {
  int v = 0;
  dv_check(nc < DV_NUM_COLOR_POOLS);
  switch (nc) {
  case 0:
    //v = dv_get_color_pool_index(nc, 0, 0, 0, pi->info.worker);
    v = pi->info.worker;
    break;
  case 1:
    //v = dv_get_color_pool_index(nc, 0, 0, 0, pi->info.cpu);
    v = pi->info.cpu;
    break;
  case 2:
    //v = dv_get_color_pool_index(nc, 0, 0, 0, pi->info.kind);
    v = (int) pi->info.kind;
    v += 10;
    break;
  case 3:
    v = dv_get_color_pool_index(nc, 0, 0, pi->info.start.pos.file_idx, pi->info.start.pos.line);
    break;
  case 4:
    v = dv_get_color_pool_index(nc, 0, 0, pi->info.end.pos.file_idx, pi->info.end.pos.line);
    break;
  case 5:
    v = dv_get_color_pool_index(nc, pi->info.start.pos.file_idx, pi->info.start.pos.line, pi->info.end.pos.file_idx, pi->info.end.pos.line);
    break;
  default:
    dv_check(0);
    break;
  }
  return v;
}

_static_unused_ int
dm_dag_node_lookup_value(dm_dag_t * D, dm_dag_node_t * node, int nc) {
  dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, node);
  return dv_pidag_node_lookup_value(pi, nc);
}


/*----------Convenient Functions-------------------------------*/

_static_unused_ const char *
dv_convert_char_to_binary(int x) {
  static char b[9];
  b[0] = '\0';
  int z;
  for (z=1; z<=1<<3; z<<=1)
    strcat(b, ((x & z) == z) ? "1" : "0");
  return b;
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

_static_unused_ char *
dv_get_node_kind_name(dr_dag_node_kind_t kind) {
  switch (kind) {
  case dr_dag_node_kind_create_task:
    return "create";
  case dr_dag_node_kind_wait_tasks:
    return "wait";
  case dr_dag_node_kind_other:
    return "other";
  case dr_dag_node_kind_end_task:
    return "end";
  case dr_dag_node_kind_section:
    return "section";
  case dr_dag_node_kind_task:
    return "task";
  default:
    return "unknown";
  }
}

_static_unused_ char *
dv_get_edge_kind_name(dr_dag_edge_kind_t kind) {
  switch (kind) {
  case dr_dag_edge_kind_end:
    return "end";
  case dr_dag_edge_kind_create:
    return "create";
  case dr_dag_edge_kind_create_cont:
    return "create_cont";
  case dr_dag_edge_kind_wait_cont:
    return "wait_cont";
  case dr_dag_edge_kind_other_cont:
    return "other_cont";
  case dr_dag_edge_kind_max:
    return "max";
  default:
    return "unknown";
  }
}

/*----------end of Convenient Functions-----------------------*/



#endif /* DAGVIZ_HEADER_ */
