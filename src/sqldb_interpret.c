#include "sqldb_interpret.h"

#include "btree_node_debug.h"
#include "btree_op_scan.h"
#include "btree_op_select.h"
#include "btree_op_update.h"
#include "sql_ibtree.h"
#include "sql_parsegen.h"
#include "sql_utils.h"
#include "sql_value.h"
#include "sqldb.h"
#include "sqldb_scan.h"
#include "sqldb_seq_tbl.h"
#include "sqldb_table.h"
#include "sqldb_table_tbl.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

static enum sql_e
insert(struct SQLDB* db, struct SQLParsedInsert* insert)
{
	enum sql_e result = SQL_OK;

	struct OpUpdate upsert = {0};
	struct BTreeView tv = {0};
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

	result = sqldb_table_btree_acquire(db, table, &tv);
	if( result != SQL_OK )
		goto end;

	result =
		sqlbt_err(btree_op_update_acquire_tbl(tv.tree, &upsert, row_id, NULL));
	if( result != SQL_OK )
		goto end;

	result = sqlbt_err(btree_op_update_prepare(&upsert));
	// TODO: on conflict.
	if( result != SQL_ERR_NOT_FOUND )
		goto end;

	result = sql_ibtree_serialize_record_acquire(&serred, table, record);
	if( result != SQL_OK )
		goto end;

	// dbg_print_buffer(serred.buf, serred.size);

	result =
		sqlbt_err(btree_op_update_commit(&upsert, serred.buf, serred.size));
	if( result != SQL_OK )
		goto end;

end:
	sql_ibtree_serialize_record_release(&serred);
	btree_op_update_release(&upsert);
	sqldb_table_btree_release(db, table, &tv);
	sql_record_schema_destroy(record_schema);
	sql_record_destroy(record);
	sql_table_destroy(table);
	return result;
}

struct ScanBuffer
{
	byte* buffer;
	u32 size;
};

static void
scanbuffer_resize(struct ScanBuffer* buf, u32 min_size)
{
	if( buf->size < min_size )
	{
		if( buf->buffer )
			free(buf->buffer);

		buf->buffer = (byte*)malloc(min_size);
		buf->size = min_size;
	}

	memset(buf->buffer, 0x00, buf->size);
}

static void
scanbuffer_free(struct ScanBuffer* buf)
{
	if( buf->buffer )
		free(buf->buffer);
}

bool
match_where(struct SQLRecord const* record, struct SQLParsedWhereClause* where)
{
	struct SQLValue value = {0};
	bool result = false;
	enum sql_e sqle = SQL_OK;
	if( !where->field )
		return true;

	int i = sql_record_schema_indexof(record->schema, where->field);
	if( i == -1 ) // TODO: Err?
		return false;

	// TODO: Needs to convert where clause to value
	sqle = sql_value_acquire_eval(&value, &where->value);
	if( sqle != SQL_OK )
		goto fail;

	result = sql_value_equals(&record->values[i], &value);

end:
	sql_value_release(&value);
	return result;

fail:
	result = false;
	goto end;
}

static void
print_record(struct SQLRecord* record)
{
	for( int i = 0; i < record->nvalues; i++ )
	{
		printf(
			"(%.*s, ",
			record->schema->columns[i]->size,
			record->schema->columns[i]->ptr);
		sql_value_print(&record->values[i]);
		printf("),");
	}
	printf("\n");
}

static enum sql_e
select_s(struct SQLDB* db, struct SQLParsedSelect* select)
{
	//
	enum sql_e result = SQL_OK;

	struct SQLDBScan scan = {0};
	result = sqldb_scan_acquire(db, select->table_name, &scan);
	if( result != SQL_OK )
		goto end;

	do
	{
		result = sqldb_scan_next(&scan);
		if( result != SQL_OK )
			goto end;

		struct SQLRecord* record = sqldb_scan_record(&scan);
		if( !record )
			goto end;

		if( match_where(record, &select->where) )
			print_record(record);
	} while( !sqldb_scan_done(&scan) && result == SQL_OK );

end:
	sqldb_scan_release(&scan);
	return result;
}

static enum sql_e
update_record_value(
	struct SQLRecord* record,
	struct SQLString* column,
	struct SQLValue* newvalue)
{
	int i = sql_record_schema_indexof(record->schema, column);
	assert(i != -1);

	sql_value_release(&record->values[i]);

	sql_value_move(&record->values[i], newvalue);
	return SQL_OK;
}

static enum sql_e
update_record(
	struct SQLDBScan* scan,
	struct SQLParsedUpdate* update,
	struct SQLRecord* record)
{
	enum sql_e result = SQL_OK;
	struct SQLValue newvalue = {0};

	print_record(record);

	for( int i = 0; i < update->ncolumns; i++ )
	{
		// TODO: The value gets moved but this looks sketchy.
		sql_value_acquire_eval(&newvalue, &update->values[i]);

		result = update_record_value(record, update->columns[i], &newvalue);
		sql_value_release(&newvalue);

		if( result != SQL_OK )
			goto end;
	}

	result = sqldb_scan_update(scan, record);
	if( result != SQL_OK )
		goto end;
end:
	return result;
}

static enum sql_e
update(struct SQLDB* db, struct SQLParsedUpdate* update)
{
	enum sql_e result = SQL_OK;

	struct SQLDBScan scan = {0};
	result = sqldb_scan_acquire(db, update->table_name, &scan);
	if( result != SQL_OK )
		goto end;

	do
	{
		result = sqldb_scan_next(&scan);
		if( result != SQL_OK )
			goto end;

		struct SQLRecord* record = sqldb_scan_record(&scan);
		if( !record )
			goto end;

		if( match_where(record, &update->where) )
			update_record(&scan, update, record);
	} while( !sqldb_scan_done(&scan) && result == SQL_OK );

end:
	sqldb_scan_release(&scan);
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
	case SQL_PARSE_SELECT:
		result = select_s(db, &parsed->parse.select);
		break;
	case SQL_PARSE_UPDATE:
		result = update(db, &parsed->parse.update);
		break;
	}

	return result;
}