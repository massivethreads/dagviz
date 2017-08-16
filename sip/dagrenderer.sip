%Module dagrenderer

%Import QtCore/QtCoremod.sip

class DAGRenderer {

%TypeHeaderCode
#include "../qt/dagrenderer.h"
%End

public:
  DAGRenderer();
  void draw(long);
};
