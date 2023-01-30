#ifndef SQLDB_META_TBLS_H_
#define SQLDB_META_TBLS_H_

#include "btint.h"
#include "sql_defs.h"
#include "sql_table.h"
#include "sqldb_defs.h"

enum sql_e sqldb_meta_tables_create(struct SQLDB* sqldb, char const* filename);

enum sql_e sqldb_meta_deserialize_table_def(
	void* buf, u32 buf_size, struct SQLTable* out_table);

#endif