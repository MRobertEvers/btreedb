#include "sqldb_meta_tbls.h"

#include "btree_factory.h"
#include "serialization.h"
#include "sql_value.h"
#include "sqldb_seq_tbl.h"

enum sql_e
sqldb_meta_tables_create(
	struct SQLDBMetaTables* sqldb_meta, char const* filename)
{
	struct Pager* pager = btree_factory_pager_create(filename);

	// TODO: Free
	sqldb_meta->tb_table_definitions =
		btree_factory_create_ex(pager, BTREE_TBL, 1);
	sqldb_meta->tb_sequences = sqldb_seq_tbl_create(pager);

	// TODO: Error

	return SQL_OK;
}

// Meta table columns
// table_name (string)
// columns  (string) As Array of type,name

enum sql_e
sqldb_meta_deserialize_table_def(
	void* buf, u32 buf_size, struct SQLTable* out_table)
{
	// TODO: Bounds checks.
	byte* start = buf;
	byte* ptr = buf;
	// sql_value_array_ser_size()
	struct SQLValue value = {0};
	ptr +=
		sql_value_deserialize_as(&value, SQL_VALUE_TYPE_INT, ptr, ptr - start);

	ptr += sql_value_deserialize_as(
		&value, SQL_VALUE_TYPE_STRING, ptr, ptr - start);
	sql_string_move(out_table->table_name, value.value.string);

	ptr += sql_value_deserialize_as(
		&value, SQL_VALUE_TYPE_STRING, ptr, ptr - start);

	u32 column_array_len = 0;
	byte* arr_ptr = (byte*)sql_string_raw(value.value.string);
	ser_read_32bit_le(&column_array_len, arr_ptr);
	arr_ptr += 4;
	for( int i = 0; i < column_array_len; i++ )
	{
		// TODO: Dangerous cast.
		enum sql_value_type_e type = (enum sql_value_type_e)arr_ptr[0];
		arr_ptr += 1;

		struct SQLValue temp = {0};
		arr_ptr += sql_value_deserialize_as(
			&value, SQL_VALUE_TYPE_STRING, ptr, ptr - start);

		sql_string_move(out_table->columns[i].name, temp.value.string);
	}
	out_table->ncolumns = column_array_len;

	return SQL_OK;
}

enum sql_e
sqldb_meta_serialize_table_def_size(struct SQLTable* table)
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
sqldb_meta_serialize_table_def(struct SQLTable* table, void* buf, u32 buf_size)
{
	// TODO: Bounds checks.
	byte* start = buf;
	byte* ptr = buf;
	// sql_value_array_ser_size()
	// TODO: Table id
	ptr += sql_value_serialize_int(1, ptr, ptr - start);
	ptr += sql_value_serialize_string(table->table_name, ptr, ptr - start);

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