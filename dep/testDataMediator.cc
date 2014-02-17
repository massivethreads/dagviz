#include <iostream>
#include <sqlite3.h>
#include <cstring>
#include <cstdio>

using namespace std;

int callback_fun(void * count, int num_col, char ** col_strs, char ** col_names)
{
  int * c = (int *) count;
  printf("Row %d\n", *c);
  *c += 1;
  int i;
  for (i=0; i<num_col; i++)
	if (i < num_col-1)
	  printf("%s, ", col_strs[i]);
	else
	  printf("%s\n", col_strs[i]);
  return 0;
}

int callback_fun_2(void * _count, int num_col, char ** col_strs, char ** col_names)
{
  int i;
  for (i=0; i<num_col; i++)
	if (i < num_col-1)
	  printf("%s:%s, ", col_names[i], col_strs[i]);
	else
	  printf("%s:%s\n", col_names[i], col_strs[i]);
  return 0;
}

int main(int argc, char ** argv)
{
  sqlite3 * db;
  char * db_name = (char *) "db_tuna_n1024_app.sqlite";
  int r;

  r = sqlite3_open(db_name, &db);

  if (r) {
	cout << "Can't open database: " << db_name << endl;	
  } else {
	cout << "Open database successfully" << endl;

	char * sql_str = (char *) "SELECT * FROM interval_c24 c WHERE c.level=3";
	char * err_msg;
	int count = 0;
	int r2;
	printf("before exec\n");
	r2 = sqlite3_exec(db, sql_str, callback_fun, &count, &err_msg);
	printf("after exec\n");
	printf("sqlite3_exec returns %d\n", r2);

	sql_str = (char *) "SELECT SUM(c.time) FROM interval_c24 c WHERE c.level=3";
	printf("before exec\n");
	r2 = sqlite3_exec(db, sql_str, callback_fun_2, NULL, &err_msg);
	printf("after exec\n");
	printf("sqlite3_exec returns %d\n", r2);
	
  }

  sqlite3_close(db);
  return 0;
}
