#include "Model.h"

Model:Model()
{
  rootnode.id = "0_0";
  rootnode.agg = 0; //datmed.query_sum_time(rootnode.id);
  rootnode.spl_len = 0;
  rootnode.spl = NULL;
}

Model:~Model()
{
}
