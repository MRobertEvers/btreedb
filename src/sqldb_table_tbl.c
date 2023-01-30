#include "sqldb_table_tbl.h"

#include "btree_factory.h"
#include "btree_node_debug.h"
#include "btree_op_scan.h"
#include "serialization.h"
#include "sql_ibtree.h"
#include "sql_utils.h"

#include <stdlib.h>
#include <string.h>

struct SQLDBMetaTable
sqldb_table_tbl_create(struct Pager* pager)
{
	return (struct SQLDBMetaTable){
		.table = NULL, .tree = btree_factory_create_ex(pager, BTREE_TBL, 1)};
}

struct ScanBuffer
{
	byte* buffer;
	u32 size;
};

void
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

void
scanbuffer_free(struct ScanBuffer* buf)
{
	if( buf->buffer )
		free(buf->buffer);
}

enum sql_e
sqldb_table_tbl_find(
	struct SQLDB* sqldb, struct SQLString* name, struct SQLTable* out_table)
{
	enum sql_e result = SQL_OK;
	struct ScanBuffer buffer = {0};
	struct OpScan scan = {0};
	struct SQLTable* table = NULL;
	result = sqlbt_err(btree_op_scan_acquire(sqldb->tb_tables.tree, &scan));
	if( result != SQL_OK )
		goto end;

	result = sqlbt_err(btree_op_scan_prepare(&scan));
	if( result != SQL_OK )
		goto end;

	while( !btree_op_scan_done(&scan) )
	{
		scanbuffer_resize(&buffer, scan.data_size);

		result =
			sqlbt_err(btree_op_scan_current(&scan, buffer.buffer, buffer.size));
		if( result != SQL_OK )
			goto end;

		table = sql_table_create();

		result = sqldb_table_tbl_deserialize_table_def(
			buffer.buffer, scan.data_size, table);
		if( result != SQL_OK )
			goto end;

		if( sql_string_equals(table->table_name, name) )

		{
			*out_table = *table;
			table = NULL;
			// sql_table_move(&out_table, &table);
			break;
		}

		sql_table_destroy(table);
		table = NULL;

		result = sqlbt_err(btree_op_scan_next(&scan));
		if( result != SQL_OK )
			goto end;
	}

end:
	scanbuffer_free(&buffer);
	sql_table_destroy(table);
	btree_op_scan_release(&scan);

	return result;
}

// Meta table columns
// table_name (string)
// first page
// columns  (string) As Array of type,name

enum sql_e
sqldb_table_tbl_deserialize_table_def(
	void* buf, u32 buf_size, struct SQLTable* out_table)
{
	// TODO: Bounds checks.
	byte* start = buf;
	byte* ptr = buf;
	// sql_value_array_ser_size()
	struct SQLValue value = {0};
	ptr +=
		sql_value_deserialize_as(&value, SQL_VALUE_TYPE_INT, ptr, ptr - start);
	out_table->meta.table_id = value.value.num.num;

	ptr += sql_value_deserialize_as(
		&value, SQL_VALUE_TYPE_STRING, ptr, ptr - start);

	sql_string_move(&out_table->table_name, &value.value.string);

	ptr +=
		sql_value_deserialize_as(&value, SQL_VALUE_TYPE_INT, ptr, ptr - start);
	out_table->meta.root_page = value.value.num.num;

	ptr +=
		sql_value_deserialize_as(&value, SQL_VALUE_TYPE_INT, ptr, ptr - start);

	u32 column_array_len = value.value.num.num;
	for( int i = 0; i < column_array_len; i++ )
	{
		// TODO: Dangerous cast.
		enum sql_dt_e type = (enum sql_dt_e)ptr[0];
		ptr += 1;

		struct SQLValue temp = {0};
		ptr += sql_value_deserialize_as(
			&temp, SQL_VALUE_TYPE_STRING, ptr, ptr - start);

		sql_string_move(&out_table->columns[i].name, &temp.value.string);
		out_table->columns[i].type = type;

		// TODO: Appropriate check for pkey.
		out_table->columns[i].is_primary_key = i == 0;
	}
	out_table->ncolumns = column_array_len;

end:

	return SQL_OK;
}

enum sql_e
sqldb_table_tbl_serialize_table_def_size(struct SQLTable* table)
{
	u32 size = 0;

	size += 4; // table id

	// name
	size += 4 + sql_string_len(table->table_name);

	// Col array
	u32 sizeof_col_array = 0;
	for( int i = 0; i < table->ncolumns; i++ )
	{
		// +4 for sizeof string. +1 for type
		sizeof_col_array += sql_string_len(table->columns[i].name) + 4 + 1;
	}

	size += 4 + sizeof_col_array;

	return size;
}

enum sql_e
sqldb_table_tbl_serialize_table_def(
	struct SQLTable* table, void* buf, u32 buf_size)
{
	// TODO: Bounds checks.
	byte* start = buf;
	byte* ptr = buf;
	ptr += sql_value_serialize_int(table->meta.table_id, ptr, ptr - start);
	ptr += sql_value_serialize_string(table->table_name, ptr, ptr - start);
	ptr += sql_value_serialize_int(table->meta.root_page, ptr, ptr - start);

	ptr += sql_value_serialize_int(table->ncolumns, ptr, ptr - start);
	for( int i = 0; i < table->ncolumns; i++ )
	{
		*ptr = table->columns[i].type;
		ptr += 1;

		ptr += sql_value_serialize_string(
			table->columns[i].name, ptr, ptr - start);
	}

	return SQL_OK;
}