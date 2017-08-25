#ifndef _DAGRENDERER_H_
#define _DAGRENDERER_H_

#include <Python.h>

extern "C" {
#include "../dagmodel.h"
}

#include <QtCore>
#include <QtWidgets>

class DAGRenderer : public QObject {
  Q_OBJECT

public:
  static const int LAYOUT_TYPE_1 = DM_LAYOUT_DAG_COORDINATE;
  static const int LAYOUT_TYPE_2 = DM_LAYOUT_DAG_BOX_LINEAR_COORDINATE;
  static const int LAYOUT_TYPE_3 = DM_LAYOUT_DAG_BOX_POWER_COORDINATE;
  static const int LAYOUT_TYPE_4 = DM_LAYOUT_DAG_BOX_LOG_COORDINATE;
  static const int LAYOUT_TYPE_5 = DM_LAYOUT_PARAPROF_COORDINATE;
  char * fileName;
  char * shortFileName;
  char * shortName;
  
  DAGRenderer();
  void setDAG(char * filename);
  void addViewport_(QWidget *, int);
  void addViewport(void * VP, int cid = DM_LAYOUT_DAG_COORDINATE) { addViewport_((QWidget *) VP, cid); };
  void removeViewport_(QWidget * VP, int cid) { mViewports[cid].removeAll(VP); };
  void removeViewport(void * VP, int cid) { removeViewport_((QWidget *) VP, cid); };
  void removeViewport(void * VP) {
    for (int cid = 0; cid < DM_NUM_COORDINATES; cid++) {
      removeViewport_((QWidget *) VP, cid);
    }
  };
  dm_dag_t * dag() { return mDAG; };
  int dagId() { return dm_get_dag_id(mDAG); };
  int animationOn() { if (animation_on) return 1; else return 0; };
  double getDx() { double ret = mDx; mDx = 0.0; return ret; }
  double getDy() { double ret = mDy; mDy = 0.0; return ret; }
  double getLinearRadix() { return mDAG->linear_radix; }
  double getPowerRadix() { return mDAG->power_radix; }
  double getLogRadix() { return mDAG->log_radix; }
  void setLinearRadix(double r) { mDAG->linear_radix = r; }
  void setPowerRadix(double r) { mDAG->power_radix = r; }
  void setLogRadix(double r) { mDAG->log_radix = r; }
  void update();

  PyObject * compute_dag_statistics(int D_id);
  void layout() { layout_(mDAG); };
  void layout(int cid) { layout__(mDAG, cid); };
  void draw(void * qp_ptr) {
    this->anchorEnabled = false;
    QPainter * qp = (QPainter *) qp_ptr;
    draw_(qp, mDAG);
  };
  void draw(void * qp_ptr, double anchor_x, double anchor_y) {
    if (!this->anchorEnabled) {
      this->anchorEnabled = true;
      this->anchor_x_node = this->anchor_y_node = NULL;
    }
    this->anchor_x = anchor_x;
    this->anchor_y = anchor_y;
    QPainter * qp = (QPainter *) qp_ptr;
    draw_(qp, mDAG);
  };
  void do_expanding_one_1(void * node) { do_expanding_one_1_(mDAG, (dm_dag_node_t *) node); };
  void do_expanding_one_r(void * node) { do_expanding_one_r_(mDAG, (dm_dag_node_t *) node); };
  void do_expanding_one() { do_expanding_one_(mDAG); };

  void do_collapsing_one_1(void * node) { do_collapsing_one_1_(mDAG, (dm_dag_node_t *) node); };
  void do_collapsing_one_r(void * node) { do_collapsing_one_r_(mDAG, (dm_dag_node_t *) node); };
  void do_collapsing_one_depth_r(void * node, int depth) { do_collapsing_one_depth_r_(mDAG, (dm_dag_node_t *) node, depth); };
  void do_collapsing_one() { do_collapsing_one_(mDAG); };
  void do_collapsing_parent(void * node) { do_collapsing_one_r_(mDAG, ((dm_dag_node_t *) node)->parent); };

  double left_width(int cid) { return mDAG->rt->c[cid].lw; };
  double right_width(int cid) { return mDAG->rt->c[cid].rw; };
  double width(int cid) { return left_width(cid) + right_width(cid); };
  double up_height(int);
  double down_height(int);
  double height(int cid) { return up_height(cid) + down_height(cid); };
  dm_dag_node_t * find_node(double x, double y, int cid) { return dm_dag_find_node(mDAG, x, y, cid); };
  dm_dag_node_t * find_node_with_node_index(int node_id) { return dm_dag_find_node_with_pi_index(mDAG, node_id); };
  PyObject * get_dag_node_info(void * node) { return get_dag_node_info_((dm_dag_node_t *) node); };
  void highlight_node(void * node) { ((dm_dag_node_t *) node)->highlight = 1; };
  void unhighlight_node(void * node) { ((dm_dag_node_t *) node)->highlight = 0; };
  void switch_node_highlight(void * node) { ((dm_dag_node_t *) node)->highlight = 1 - ((dm_dag_node_t *) node)->highlight; };
  int node_is_highlighted(void * node) { return ((dm_dag_node_t *) node)->highlight; };

public slots:
  void do_animation_tick();

private:
  dm_dag_t * mDAG = NULL;      /* DAG of this renderer */
  QWidget * mViewport = NULL;  /* Viewport where the DAG of this renderer is drawn on */
  bool animation_on = false;
  QTimer * animation_timer = NULL;
  bool anchorEnabled = false;
  double anchor_x, anchor_y;
  dm_dag_node_t * anchor_x_node = NULL;
  dm_dag_node_t * anchor_y_node = NULL;
  dm_dag_node_t * anchor_nodes[DM_NUM_COORDINATES] = { NULL };
  double mDx = 0.0;
  double mDy = 0.0;
  QVector<QWidget *> mViewports[DM_NUM_COORDINATES];
  
  char * parse_python_string(PyObject *);
  int parse_python_int(PyObject *);
  void layout1_(dm_dag_t *, int);
  void layout2_(dm_dag_t *, int);
  void layout3_(dm_dag_t *, int);
  void layout__(dm_dag_t *, int);
  void layout_(dm_dag_t *);
  void do_animation_start();
  void do_animation_stop();
  void do_animation_add_node(dm_dag_node_t *);
  void do_animation_remove_node(dm_dag_node_t *);
  void do_animation_reverse_node(dm_dag_node_t *);
  void do_expanding_one_1_(dm_dag_t *, dm_dag_node_t *);
  void do_expanding_one_r_(dm_dag_t *, dm_dag_node_t *);
  void do_expanding_one_(dm_dag_t *);
  void do_collapsing_one_1_(dm_dag_t *, dm_dag_node_t *);
  void do_collapsing_one_r_(dm_dag_t *, dm_dag_node_t *);
  void do_collapsing_one_depth_r_(dm_dag_t *, dm_dag_node_t *, int);
  void do_collapsing_one_(dm_dag_t *);
  void draw1_node_1(QPainter *, dm_dag_t *, dm_dag_node_t *, _unused_ int *, int);
  void draw1_node_r(QPainter *, dm_dag_t *, dm_dag_node_t *, int *, int);  
  void draw1_(QPainter *, dm_dag_t *, int);
  void draw2_node_1(QPainter *, dm_dag_t *, dm_dag_node_t *, _unused_ int *, int);
  void draw2_node_r(QPainter *, dm_dag_t *, dm_dag_node_t *, int *, int);  
  void draw2_(QPainter *, dm_dag_t *, int);
  void draw3_node_1(QPainter *, dm_dag_t *, dm_dag_node_t *, _unused_ int *, int);
  void draw3_node_r(QPainter *, dm_dag_t *, dm_dag_node_t *, int *, int);  
  void draw3_(QPainter *, dm_dag_t *, int);
  void draw_paraprof(QPainter *, dm_dag_t *, int);
  int get_cid_from_qpainter(QPainter *);
  void draw_(QPainter *, dm_dag_t *);
  PyObject * get_dag_node_info_(dm_dag_node_t *);
    
};

#endif
