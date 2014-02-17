#include <string>
#include "DataMediator.h"

using namespace std;

typedef struct {
  string id;
  long agg;
  int spl_len;
  ComNode * spl;
} Node;

typedef struct {
  int num_spawns;
  Node * spawn_nodes;
  int * spawn_ids;
  int num_syncs;
  Node * sync_nodes;
  int * sync_ids;
} ComNode;

class Model
{
 public:
  Model();
  virtual ~Model();

 private:
  DataMediator datmed;
  Node rootnode;
  
}
