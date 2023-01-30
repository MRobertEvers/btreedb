#include "sqldb_interpret.h"

#include "btree_node_debug.h"
#include "btree_op_update.h"
#include "sql_ibtree.h"
#include "sql_parsegen.h"
#include "sql_utils.h"
#include "sqldb.h"
#include "sqldb_seq_tbl.h"
#include "sqldb_table.h"
#include "sqldb_table_tbl.h"

#include <stdlib.h>

static enum sql_e
create_table(struct SQLDB* db, struct SQLParsedCreateTable* create_table)
{
	enum sql_e result = SQL_OK;
	struct SQLTable* table = sql_table_create();

	// Create table struct
	sql_parsegen_table_from_create_table(create_table, table);

	result = sqldb_create_table(db, table);
	if( result != SQL_OK )
		goto end;

end:
	sql_table_destroy(table);
	return result;
}

// static enum sql_e
// insert_bt()

static enum sql_e
insert(struct SQLDB* db, struct SQLParsedInsert* insert)
{
	enum sql_e result = SQL_OK;

	struct OpUpdate upsert = {0};
	struct BTree* tree = NULL;
	u32 row_id = 0;
	struct SQLSerializedRecord serred = {0};
	struct SQLTable* table = sql_table_create();
	struct SQLRecordSchema* record_schema = sql_record_schema_create();
	struct SQLRecord* record = sql_record_create();

	result = sqldb_table_tbl_find(db, insert->table_name, table);
	if( result != SQL_OK )
		goto end;

	// All columns except nullable, or default columns (pkey is autoincrement)
	result = sql_parsegen_record_schema_from_insert(insert, record_schema);
	if( result != SQL_OK )
		goto end;

	result = sql_parsegen_record_from_insert(insert, record_schema, record);
	if( result != SQL_OK )
		goto end;

	result = sqldb_table_prepare_record(db, table, record, &row_id);
	if( result != SQL_OK )
		goto end;

	result = sqldb_table_btree_create(db, table, &tree);
	if( result != SQL_OK )
		goto end;

	result =
		sqlbt_err(btree_op_update_acquire_tbl(tree, &upsert, row_id, NULL));
	if( result != SQL_OK )
		goto end;

	result = sqlbt_err(btree_op_update_prepare(&upsert));
	// TODO: on conflict.
	if( result != SQL_ERR_NOT_FOUND )
		goto end;

	result = sql_ibtree_serialize_record_acquire(&serred, table, record);
	if( result != SQL_OK )
		goto end;

	dbg_print_buffer(serred.buf, serred.size);

	result =
		sqlbt_err(btree_op_update_commit(&upsert, serred.buf, serred.size));
	if( result != SQL_OK )
		goto end;

end:
	sql_ibtree_serialize_record_release(&serred);
	btree_op_update_release(&upsert);
	if( tree )
		sqldb_table_btree_destroy(db, table, tree);
	sql_record_schema_destroy(record_schema);
	sql_record_destroy(record);
	sql_table_destroy(table);
	return result;
}

enum sql_e
sqldb_interpret(struct SQLDB* db, struct SQLParse* parsed)
{
	enum sql_e result = SQL_OK;
	switch( parsed->type )
	{
	case SQL_PARSE_INVALID:
		break;
	case SQL_PARSE_CREATE_TABLE:
		result = create_table(db, &parsed->parse.create_table);
		break;
	case SQL_PARSE_INSERT:
		result = insert(db, &parsed->parse.insert);
		break;
	}

	return result;
}