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
    dr_pi_dag_node * pi = dv_pidag_get_node_by_dag_node(D->P, D->rt);    
    ret = Py_BuildValue("{s:s,s:K,s:K,s:K,s:[L,L,L,L]}",
                        "name", D->name_on_graph,
                        "work", CS->SBG->work[i],
                        "delay", CS->SBG->delay[i],
                        "nowork", CS->SBG->nowork[i],
                        "counters", pi->info.counters_1[0],
                        pi->info.counters_1[1], pi->info.counters_1[2],
                        pi->info.counters_1[3]);
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
