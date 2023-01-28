#include "sql_parsegen.h"

#include <stdbool.h>

static int
find_primary_key(struct SQLParsedCreateTable* parsed)
{
	bool has_primary_key = false;
	for( int i = 0; i < parsed->ncolumns; i++ )
	{
		has_primary_key = parsed->columns[i].is_primary_key;
		if( has_primary_key )
			return i;
	}
	return -1;
}

void
sql_parsegen_table_from_create_table(
	struct SQLParsedCreateTable const* create_table, struct SQLTable* out_tbl)
{
	out_tbl->table_name = sql_string_copy(create_table->table_name);

	// TODO: This pkey behavior should not be here.
	int pkey_ind = find_primary_key(create_table);

	if( pkey_ind == -1 )
	{
		sql_table_emplace_column(out_tbl, "id", SQL_DT_INT, true);
	}

	// TODO: Put pkey in the front.
	for( int i = 0; i < create_table->ncolumns; i++ )
	{
		out_tbl->columns[out_tbl->ncolumns].name =
			sql_string_copy(create_table->columns[i].name);
		out_tbl->columns[out_tbl->ncolumns].type =
			create_table->columns[i].type;
		out_tbl->columns[out_tbl->ncolumns].is_primary_key = false;

		out_tbl->ncolumns += 1;
	}
}

enum sql_e
sql_parsegen_record_schema_from_insert(
	struct SQLParsedInsert const* insert, struct SQLRecordSchema* out_schema)
{
	for( int i = 0; i < insert->ncolumns; i++ )
	{
		out_schema->columns[i] = sql_string_copy(insert->columns[i]);
	}
	out_schema->ncolumns = insert->ncolumns;

	return SQL_OK;
}

enum sql_e
sql_parsegen_record_from_insert(
	struct SQLParsedInsert const* insert,
	struct SQLRecordSchema* schema,
	struct SQLRecord* out_record)
{
	// TODO: Multiple values.
	if( insert->ncolumns != insert->nvalues )
		return SQL_ERR_INVALID_SQL;

	out_record->schema = schema;
	for( int i = 0; i < insert->ncolumns; i++ )
	{
		out_record->values[i].type = insert->values[i].type;
		out_record->values[i].value = sql_string_copy(insert->values[i].value);
	}
	out_record->nvalues = insert->ncolumns;
}