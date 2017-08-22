%Module dagrenderer

%Import QtCore/QtCoremod.sip
%Import QtWidgets/QtWidgetsmod.sip

class DAGRenderer : public QObject {

%TypeHeaderCode
#include "../qt/dagrenderer.h"
%End

public:
  const int LAYOUT_TYPE_1;
  const int LAYOUT_TYPE_2;

  DAGRenderer();
  void setDAG(char *);
  void addViewport(void *, int = DM_LAYOUT_DAG_COORDINATE);
  void removeViewport(void *, int);
  void removeViewport(void *);
  void * dag();
  int dagId();
  int animationOn();
  double getDx();
  double getDy();
  double getLinearRadix();
  double getPowerRadix();
  double getLogRadix();
  
  PyObject * compute_dag_statistics(int);
  void layout();
  void draw(void *);
  void draw(void *, double, double);
  void do_expanding_one_1(void *);
  void do_expanding_one_r(void *);
  void do_expanding_one();
  void do_collapsing_one_1(void *);
  void do_collapsing_one_r(void *);
  void do_collapsing_one_depth_r(void *, int);
  void do_collapsing_one();
  void do_collapsing_parent(void *);
  double left_width();
  double right_width();
  double width();
  double height();
  void * find_node(double, double);
  PyObject * get_dag_node_info(void *);
};
