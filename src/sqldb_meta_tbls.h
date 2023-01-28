#ifndef SQLDB_META_TBLS_H_
#define SQLDB_META_TBLS_H_

#include "btint.h"
#include "sql_defs.h"
#include "sql_table.h"
#include "sqldb_defs.h"

enum sql_e sqldb_meta_tables_create(
	struct SQLDBMetaTables* sqldb_meta, char const* filename);

enum sql_e sqldb_meta_deserialize_table_def(
	void* buf, u32 buf_size, struct SQLTable* out_table);
enum sql_e sqldb_meta_serialize_table_def_size(struct SQLTable* table);
enum sql_e
sqldb_meta_serialize_table_def(struct SQLTable* table, void* buf, u32 buf_size);

#endif