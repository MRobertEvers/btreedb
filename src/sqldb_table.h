#ifndef SQLDB_TABLE_H_
#define SQLDB_TABLE_H_

// TODO:

#include "sql_defs.h"
#include "sql_record.h"
#include "sql_table.h"

// TODO: Modifies both schema and record
enum sql_e sqldb_table_prepare_record(struct SQLTable*, struct SQLRecord*);

#endif