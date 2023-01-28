#include "sql_ibtree.h"

#include "stdbool.h"

#include <assert.h>
#include <stdlib.h>

static int
find_primary_key(struct SQLTable* tbl)
{
	return sql_table_find_primary_key(tbl);
}

static enum ibtlsk_type_e
convert_to_schema_type(enum sql_dt_e dt)
{
	switch( dt )
	{
	case SQL_DT_INT:
		return IBTLSK_TYPE_FIXED;
	case SQL_DT_STRING:
		return IBTLSK_TYPE_VARSIZE;
	default:
		assert(0);
	}

	return IBTLSK_TYPE_INVALID;
}

static enum sql_value_type_e
convert_to_value_type(enum sql_dt_e dt)
{
	switch( dt )
	{
	case SQL_DT_INT:
		return SQL_VALUE_TYPE_INT;
	case SQL_DT_STRING:
		return SQL_VALUE_TYPE_STRING;
	default:
		assert(0);
	}

	return SQL_VALUE_TYPE_INVAL;
}

static void
emplace_schema_def(struct IBTLSKeyDef* def, struct SQLTableColumn* col)
{
	def->type = convert_to_schema_type(col->type);
	def->size = def->type == IBTLSK_TYPE_FIXED ? sizeof(u32) : 0;
}

enum sql_e
sql_ibtree_create_ibt_layout_schema(
	struct SQLTable* tbl, struct IBTreeLayoutSchema* out_schema)
{
	int pkey_ind = find_primary_key(tbl);
	assert(pkey_ind != -1);

	// TODO: Appropriate key offset?
	out_schema->key_offset = 0;

	// Place the pkey at the front.
	emplace_schema_def(
		&out_schema->key_definitions[0], &tbl->columns[pkey_ind]);
	out_schema->nkey_definitions += 1;

	for( int i = 0; i < tbl->ncolumns; i++ )
	{
		if( i == pkey_ind )
			continue;

		emplace_schema_def(
			&out_schema->key_definitions[out_schema->nkey_definitions],
			&tbl->columns[i]);

		out_schema->nkey_definitions += 1;
	}

	return SQL_OK;
}

static int
find_pkey_in_schema(
	struct SQLRecordSchema* schema, struct SQLString* pkey_string)
{
	return sql_record_schema_indexof(schema, pkey_string);
}

enum sql_e
sql_ibtree_serialize_record_size(struct SQLRecord* record)
{
	return sql_value_array_ser_size(record->values, record->nvalues);
}

enum sql_e
sql_ibtree_serialize_record(
	struct SQLTable* tbl,

	struct SQLRecord* record,
	struct SQLSerializedRecord* out_serialized)
{
	int pkey_ind = find_primary_key(tbl);
	assert(pkey_ind != -1);

	struct SQLRecordSchema* schema = record->schema;

	int schema_pkey_ind =
		find_pkey_in_schema(schema, tbl->columns[pkey_ind].name);
	if( schema_pkey_ind == -1 )
		return SQL_ERR_RECORD_MISSING_PKEY;

	u32 size = sql_value_array_ser_size(&record->values, record->nvalues);

	byte* buffer = (byte*)malloc(size);

	byte* ptr = buffer;
	u32 written = 0;
	written += sql_value_serialize(
		&record->values[schema_pkey_ind], ptr, size - written);

	for( int i = 0; i < record->nvalues; i++ )
	{
		if( i == schema_pkey_ind )
			continue;
		// TODO: Bounds checking
		written += sql_value_serialize(
			&record->values[i], ptr + written, size - written);
	}

	out_serialized->buf = buffer;
	out_serialized->size = size;

	return SQL_OK;
}

enum sql_e
sql_ibtree_deserialize_record(
	struct SQLTable* tbl,
	struct SQLRecordSchema* out_schema,
	struct SQLRecord* out_record,
	void* buf,
	u32 size)
{
	byte* start = buf;
	byte* ptr = buf;

	int pkey_ind = sql_table_find_primary_key(tbl);
	assert(pkey_ind != -1);

	// First col is always pkey
	ptr += sql_value_deserialize_as(
		&out_record->values[out_record->nvalues],
		convert_to_value_type(tbl->columns[pkey_ind].type),
		ptr,
		size - (ptr - start));
	out_record->nvalues += 1;

	out_schema->columns[0] = sql_string_copy(tbl->columns[pkey_ind].name);
	out_schema->ncolumns += 1;

	for( int i = 0; i < tbl->ncolumns; i++ )
	{
		if( i == pkey_ind )
			continue;
		ptr += sql_value_deserialize_as(
			&out_record->values[out_record->nvalues],
			convert_to_value_type(tbl->columns[i].type),
			ptr,
			size - (ptr - start));
		out_record->nvalues += 1;

		out_schema->columns[out_schema->ncolumns] =
			sql_string_copy(tbl->columns[i].name);
		out_schema->ncolumns += 1;
	}

	out_record->schema = out_schema;

	return SQL_OK;
}