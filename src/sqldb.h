#ifndef SQLDB_H_
#define SQLDB_H_

#include "sql_defs.h"
#include "sql_table.h"
#include "sqldb_defs.h"

enum sql_e sqldb_create(struct SQLDB** out_sqldb, char const* filename);

// TODO: Not sure if this belongs here.
// TODO: RecordRC
// enum sql_e sqldb_prepare_record(struct SQLDB* sqldb, );

enum sql_e sqldb_create_table(struct SQLDB* sqldb, struct SQLTable* table);
enum sql_e sqldb_load_table(
	struct SQLDB* sqldb, struct SQLString* name, struct SQLTable** out_table);

// enum sql_e sqldb_insert(struct SQLDB* sqldb);

#endif