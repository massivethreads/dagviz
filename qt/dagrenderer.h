#ifndef _DAGRENDERER_H_
#define _DAGRENDERER_H_

#include <Python.h>

extern "C" {
#include "../dagmodel.h"
}

#include <QPainter>

class DAGRenderer {
public:
  DAGRenderer();
  PyObject * compute_dag_statistics(PyObject *);
  void draw(long);
};

#endif
