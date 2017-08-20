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

  PyObject * compute_dag_statistics(int D_id);
  void layout() { layout_(mDAG); };
  void draw() {
    QPainter * qp = new QPainter(mViewport);
    draw_(qp, mDAG);
    delete qp;
  };
  void draw(void * qp_ptr) {
    QPainter * qp = (QPainter *) qp_ptr;
    draw_(qp, mDAG);
  };
  void do_expanding_one() { do_expanding_one_(mDAG); };
  void do_collapsing_one() { do_collapsing_one_(mDAG); };
  double left_width() { int cid = 0; return mDAG->rt->c[cid].lw; };
  double right_width() { int cid = 0; return mDAG->rt->c[cid].rw; };
  double width() { return left_width() + right_width(); };
  double height() { int cid = 0; return mDAG->rt->c[cid].dw; };
                                                        
public slots:
  void do_animation_tick();

private:
  dm_dag_t * mDAG = NULL;      /* DAG of this renderer */
  QWidget * mViewport = NULL;  /* Viewport where the DAG of this renderer is drawn on */
  bool animation_on = false;
  QTimer * animation_timer = NULL;
  
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
  void do_expanding_one_1(dm_dag_t *, dm_dag_node_t *);
  void do_expanding_one_r(dm_dag_t *, dm_dag_node_t *);
  void do_expanding_one_(dm_dag_t *);
  void do_collapsing_one_1(dm_dag_t *, dm_dag_node_t *);
  void do_collapsing_one_depth_r(dm_dag_t *, dm_dag_node_t *, int);
  void do_collapsing_one_(dm_dag_t *);
  void draw_dag_(QPainter *, dm_dag_t *);
  void draw_(QPainter *, dm_dag_t *);
    
};

#endif
