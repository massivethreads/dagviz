#ifndef _DAGRENDERER_H_
#define _DAGRENDERER_H_

#include <QString>
#include <QByteArray>
#include <QPainter>

class DAGRenderer {
public:
  DAGRenderer();
  void draw(long qp_ptr);
};

#endif
