%Module dagrenderer

%Import QtCore/QtCoremod.sip

class DAGRenderer {

%TypeHeaderCode
#include "../qt/dagrenderer.h"
%End

public:
  DAGRenderer();
  PyObject * compute_dag_statistics(PyObject *);
  void draw(long);
};
