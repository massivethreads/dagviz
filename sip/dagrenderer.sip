%Module dagrenderer

%Import QtCore/QtCoremod.sip
%Import QtWidgets/QtWidgetsmod.sip

class DAGRenderer : public QObject {

%TypeHeaderCode
#include "../qt/dagrenderer.h"
%End

public:
  DAGRenderer();
  void setDAG(char *);
  void setViewport(void *);
  void * dag();
  QWidget * viewport();
  int dagId();

  PyObject * compute_dag_statistics(int);
  void layout();
  void draw();
  void draw(void *);
  void do_expanding_one();
  void do_collapsing_one();
  double left_width();
  double right_width();
  double width();
  double height();
};
