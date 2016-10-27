#include <Python.h>
#include "model.h"

static PyObject *
_wrap_dm_dag_get_characteristic_string(PyObject * self, PyObject * args) {
  /* parse arguments */
  char * input;
  if (!PyArg_ParseTuple(args, "s", &input)) {
    return NULL;
  }

  /* run the actual function */
  char * result;
  result = dm_dag_get_characteristic_string(input);

  /* build result into a Python object */
  PyObject * ret = NULL;
  if (result) {
    ret = PyString_FromString(result);
    free(result);
  }

  return ret;
}

static PyMethodDef PyDagModelMethods[] = {
 { "dm_dag_get_characteristic_string", _wrap_dm_dag_get_characteristic_string, METH_VARARGS, "Get a summary string of a DAG" },
 { NULL, NULL, 0, NULL }
};

DL_EXPORT(void) initpydagmodel(void)
{
  Py_InitModule("pydagmodel", PyDagModelMethods);
}
