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
sql_ibtree_serialize_record(
	struct SQLTable* tbl,
	struct SQLRecordSchema* schema,
	struct SQLRecord* record,
	struct SQLSerializedRecord* out_serialized)
{
	int pkey_ind = find_primary_key(tbl);
	assert(pkey_ind != -1);

	int schema_pkey_ind =
		find_pkey_in_schema(schema, tbl->columns[pkey_ind].name);
	if( schema_pkey_ind == -1 )
		return SQL_ERR_RECORD_MISSING_PKEY;

	u32 size = sql_literal_array_ser_size(&record->values, record->nvalues);

	byte* buffer = (byte*)malloc(size);

	byte* ptr = buffer;
	u32 written = 0;
	written += sql_literal_serialize(
		&record->values[schema_pkey_ind], ptr, size - written);

	for( int i = 0; i < record->nvalues; i++ )
	{
		if( i == schema_pkey_ind )
			continue;
		// TODO: Bounds checking
		written += sql_literal_serialize(
			&record->values[i], ptr + written, size - written);
	}

	out_serialized->buf = buffer;
	out_serialized->size = size;

	return SQL_OK;
}