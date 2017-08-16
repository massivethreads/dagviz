#include "dagrenderer.h"

DAGRenderer::DAGRenderer() {
}

PyObject *
DAGRenderer::compute_dag_statistics(PyObject * filename_obj) {
  /* parse arguments */
  char * filename;
  if (!PyArg_Parse(filename_obj, "s", &filename)) {
    fprintf(stderr, "Error (%s:%d): could not parse PyObject.\n", __FILE__, __LINE__);
    return NULL;
  }

  /* compute */
  dm_dag_t * D = dm_compute_dag_file(filename);
  int i = D - DMG->D;

  /* build result into a Python object */
  PyObject * ret = NULL;
  if (D) {
    dr_pi_dag_node * pi = dm_pidag_get_node_by_dag_node(D->P, D->rt);    
    ret = Py_BuildValue("{s:s,s:K,s:K,s:K,s:[L,L,L,L]}",
                        "name", D->name_on_graph,
                        "work", DMG->SBG->work[i],
                        "delay", DMG->SBG->delay[i],
                        "nowork", DMG->SBG->nowork[i],
                        "counters", pi->info.counters_1[0],
                        pi->info.counters_1[1], pi->info.counters_1[2],
                        pi->info.counters_1[3]);
  } else {
    fprintf(stderr, "Error: could not calculate DAG %s\n", filename);
  }
  return ret;
}

void
DAGRenderer::draw(long qp_ptr) {
  QPainter * qp = reinterpret_cast<QPainter*>(qp_ptr);
  qp->setPen(Qt::blue);
  qp->setBrush(Qt::blue);
  qp->drawRect(100, 120, 50, 30);
  qp->drawText(200, 135, "I'm drawn in C++");
}
