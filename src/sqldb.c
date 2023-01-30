#include "sqldb.h"

#include "btree.h"
#include "btree_defs.h"
#include "btree_node_debug.h"
#include "ibtree_layout_schema.h"
#include "ibtree_layout_schema_ctx.h"
#include "sql_utils.h"
#include "sqldb_meta_tbls.h"
#include "sqldb_seq_tbl.h"
#include "sqldb_table_tbl.h"

#include <stdlib.h>
#include <string.h>

enum sql_e
sqldb_create(struct SQLDB** out_sqldb, char const* filename)
{
	enum sql_e result = SQL_OK;
	struct SQLDB* db = (struct SQLDB*)malloc(sizeof(struct SQLDB));
	memset(db, 0x00, sizeof(*db));

	result = sqldb_meta_tables_create(db, filename);

	*out_sqldb = db;

	return result;
}

enum sql_e
sqldb_create_table(struct SQLDB* sqldb, struct SQLTable* table)
{
	enum sql_e result = SQL_OK;
	struct SQLString* str = NULL;
	int seq = 0;

	u32 ser_size = sqldb_table_tbl_serialize_table_def_size(table);
	byte* buffer = (byte*)malloc(ser_size);

	str = sql_string_create_from_cstring("tables");
	result = sqldb_seq_tbl_next(sqldb, str, &seq);
	if( result != SQL_OK )
		goto end;

	result = sqldb_table_tbl_serialize_table_def(table, seq, buffer, ser_size);
	if( result != SQL_OK )
		goto end;
	dbg_print_buffer(buffer, ser_size);

	result =
		sqlbt_err(btree_insert(sqldb->tb_tables.tree, seq, buffer, ser_size));
	if( result != SQL_OK )
		goto end;

end:
	sql_string_destroy(str);
	if( buffer )
		free(buffer);
	return result;
}

enum sql_e
sqldb_load_table(
	struct SQLDB* sqldb, struct SQLString* name, struct SQLTable** out_table)
{
	enum sql_e result = SQL_OK;
	return result;
}