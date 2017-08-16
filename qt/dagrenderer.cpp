#include "dagrenderer.h"

DAGRenderer::DAGRenderer() {
}

void
DAGRenderer::draw(long qp_ptr) {
  QPainter * qp = reinterpret_cast<QPainter*>(qp_ptr);
  qp->setPen(Qt::blue);
  qp->setBrush(Qt::blue);
  qp->drawRect(100, 120, 50, 30);
  qp->drawText(200, 135, "I'm drawn in C++");
}
