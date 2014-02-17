#include <string>
#include "DataMediator.h"
#include <cstdio>

DataMediator::DataMediator()
{
  db_name = (char *) "db_tuna_n1024_app.sqlite";

  int r;
  r = sqlite3_open(db_name, &db);

  if (r) {
	fprintf(stderr, "Can't open database: %s\n", db_name);
  } else {
	fprintf(stderr, "Opened database successfully.\n");
  }
}

DataMediator::~DataMediator()
{
  sqlite3_close(db);
}


 long DataMediator::query_sum_time()
 {
   sqlite3_stmt * stmt;
   char * sql_str = (char *) "SELECT SUM(c.time) FROM interval_c24 c WHERE c.level=3";
   int r;

   r = sqlite3_prepare_v2(db, sql_str, -1, &stmt, NULL);
   if (r != SQLITE_OK)
	 throw std::string(sqlite3_errmsg(db));

   r = sqlite3_step(stmt);
   if (r != SQLITE_ROW && r != SQLITE_DONE) {
	 std::string errmsg(sqlite3_errmsg(db));
	 sqlite3_finalize(stmt);
	 throw errmsg;
   }
   if (r == SQLITE_DONE) {
	 sqlite3_finalize(stmt);
	 throw std::string("not found.\n");
   }
   
   long sum = sqlite3_column_int(stmt, 0);
   printf("sum = %ld\n", sum);
   return sum;
 }

 int DataMediator::query_row()
 {
   char * sql_str = (char *) "SELECT * FROM interval_c24 c WHERE c.level=3";
   int count = 0;
   //int r = sqlite3_exec(db, sql_str, &DataMediator::query_row_callback, &count, NULL);
   //fprintf(stderr, "query_row(): sqlite3_exec returns %d\n", r);
   return count;
 }

/*int main()
{
  DataMediator dm;
  long s = dm.query_sum_time();
  printf("main(): s = %ld\n", s);
  return 0;
}
*/
