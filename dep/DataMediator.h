#include <sqlite3.h>

class DataMediator
{
 public:
  DataMediator();
  virtual ~DataMediator();
  long query_sum_time();
  int query_row();

 private:
  sqlite3 * db;
  char * db_name;
};
