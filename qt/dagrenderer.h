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
  DAGRenderer();
  void setDAG(char * filename);
  void setViewport_(QWidget *);
  void setViewport(void * VP) { setViewport_((QWidget *) VP); };
  dm_dag_t * dag() { return mDAG; };
  QWidget * viewport() { return mViewport; };
  int dagId() { return dm_get_dag_id(mDAG); };
  int animationOn() { if (animation_on) return 1; else return 0; };
  double getDx() { double ret = mDx; mDx = 0.0; return ret; }
  double getDy() { double ret = mDy; mDy = 0.0; return ret; }

  PyObject * compute_dag_statistics(int D_id);
  void layout() { layout_(mDAG); };
  void draw() {
    this->anchorEnabled = false;
    QPainter * qp = new QPainter(mViewport);
    draw_(qp, mDAG);
    delete qp;
  };
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


  double left_width() { int cid = 0; return mDAG->rt->c[cid].lw; };
  double right_width() { int cid = 0; return mDAG->rt->c[cid].rw; };
  double width() { return left_width() + right_width(); };
  double height() { int cid = 0; return mDAG->rt->c[cid].dw; };
  dm_dag_node_t * find_node(double x, double y) { return dm_dag_find_clicked_node(mDAG, x, y); };
  PyObject * get_dag_node_info(void * node) { return get_dag_node_info_((dm_dag_node_t *) node); };

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
  double mDx = 0.0;
  double mDy = 0.0;
  
  char * parse_python_string(PyObject *);
  int parse_python_int(PyObject *);
  void repaint() { draw(); };
  void update() { mViewport->update(); };
  void layout_dag_(dm_dag_t *);
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
  void draw_dag_node_1(QPainter *, dm_dag_t *, dm_dag_node_t *, _unused_ int *);
  void draw_dag_node_r(QPainter *, dm_dag_t *, dm_dag_node_t *, int *);  
  void draw_dag_(QPainter *, dm_dag_t *);
  void draw_(QPainter *, dm_dag_t *);
  PyObject * get_dag_node_info_(dm_dag_node_t *);
    
};

#endif
