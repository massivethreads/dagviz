#include <Python.h>
#include "model.h"

static PyObject *
get_dag_stat_from_dag_file(PyObject * self, PyObject * args) {
  /* parse arguments */
  char * input;
  if (!PyArg_ParseTuple(args, "s", &input)) {
    return NULL;
  }

  /* compute */
  dv_dag_t * D = dm_compute_dag_file(input);
  int i = D - CS->D;

  /* build result into a Python object */
  PyObject * ret = NULL;
  if (D) {
    ret = Py_BuildValue("{s:K,s:K,s:K}",
                        "work", CS->SBG->work[i],
                        "delay", CS->SBG->delay[i],
                        "nowork", CS->SBG->nowork[i]
                        );
  }
  return ret;
}

static PyMethodDef PyDagModelMethods[] = {
 { "get_dag_stat_from_dag_file", get_dag_stat_from_dag_file, METH_VARARGS, "Get a DAG's stat dict from its DAG filename" },
 { NULL, NULL, 0, NULL }
};

DL_EXPORT(void) initpydagmodel(void)
{
  Py_InitModule("pydagmodel", PyDagModelMethods);
}
