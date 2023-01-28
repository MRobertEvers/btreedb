#include "sqldb_table.h"

#include <assert.h>

static void
emplace_schema_column(
	struct SQLRecordSchema* schema, struct SQLTableColumn* col)
{
	schema->columns[schema->ncolumns] = sql_string_copy(col->name);
	schema->ncolumns += 1;
}

enum sql_e
sqldb_table_prepare_record(
	struct SQLTable* tab,
	struct SQLRecordSchema* schema,
	struct SQLRecord* record)
{
	int pkey_ind = sql_table_find_primary_key(tab);
	assert(pkey_ind != -1);

	int schema_pkey_ind =
		sql_record_schema_indexof(schema, tab->columns[pkey_ind].name);
	if( schema_pkey_ind == -1 )
	{
		sql_record_emplace_literal_c(record, "1", SQL_LITERAL_TYPE_INT);
		emplace_schema_column(schema, &tab->columns[pkey_ind]);
	}

	return SQL_OK;
}