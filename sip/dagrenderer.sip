%Module dagrenderer

%Import QtCore/QtCoremod.sip
%Import QtWidgets/QtWidgetsmod.sip

class DAGRenderer : public QObject {

%TypeHeaderCode
#include "../qt/dagrenderer.h"
%End

public:
  static const int LAYOUT_TYPE_1;
  static const int LAYOUT_TYPE_2;
  static const int LAYOUT_TYPE_3;
  static const int LAYOUT_TYPE_4;
  static const int LAYOUT_TYPE_5;
  char * fileName;
  char * shortFileName;
  char * shortName;

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
  void setLinearRadix(double);
  void setPowerRadix(double);
  void setLogRadix(double);
  
  PyObject * compute_dag_statistics(int);
  void layout();
  void layout(int);
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
  double left_width(int);
  double right_width(int);
  double width(int);
  double down_height(int);
  double up_height(int);
  double height(int);
  void * find_node(double, double, int);
  PyObject * get_dag_node_info(void *);
};
