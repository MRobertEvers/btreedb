#include "sqldb.h"

#include "btree.h"
#include "btree_defs.h"
#include "btree_node_debug.h"
#include "ibtree_layout_schema.h"
#include "ibtree_layout_schema_ctx.h"
#include "sqldb_meta_tbls.h"
#include "sqldb_seq_tbl.h"

#include <stdlib.h>
#include <string.h>

enum sql_e
sqldb_create(struct SQLDB** out_sqldb, char const* filename)
{
	enum sql_e result = SQL_OK;
	struct SQLDB* db = (struct SQLDB*)malloc(sizeof(struct SQLDB));

	result = sqldb_meta_tables_create(&db->meta, filename);

	*out_sqldb = db;

	return result;
}

enum sql_e
sqldb_create_table(struct SQLDB* sqldb, struct SQLTable* table)
{
	u32 ser_size = sqldb_meta_serialize_table_def_size(table);

	byte* buffer = (byte*)malloc(ser_size);
	sqldb_meta_serialize_table_def(table, buffer, ser_size);

	dbg_print_buffer(buffer, ser_size);

	int seq = 0;
	struct SQLString* str = sql_string_create_from_cstring("tables");
	sqldb_seq_tbl_next(sqldb, str, &seq);
	btree_insert(sqldb->meta.tb_table_definitions, seq, buffer, ser_size);

	sql_string_destroy(str);
}