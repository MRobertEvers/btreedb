#include "sql_parsed.h"

#include <string.h>

static void
where_clause_cleanup(struct SQLParsedWhereClause* where)
{
	//
}

void
sql_parsed_update_cleanup(struct SQLParsedUpdate* update)
{
	//
}

void
sql_parsed_select_cleanup(struct SQLParsedSelect* select)
{
	//
}

void
sql_parsed_insert_cleanup(struct SQLParsedInsert* insert)
{
	if( insert->table_name )
		sql_string_destroy(insert->table_name);

	for( int i = 0; i < insert->ncolumns; i++ )
	{
		if( insert->columns[i] )
			sql_string_destroy(insert->columns[i]);
	}

	for( int i = 0; i < insert->nvalues; i++ )
	{
		if( insert->values[i].value )
			sql_string_destroy(insert->values[i].value);
	}

	memset(insert, 0x00, sizeof(*insert));
}

void
sql_parsed_create_table_cleanup(struct SQLParsedCreateTable* tbl)
{
	if( tbl->table_name )
		sql_string_destroy(tbl->table_name);

	for( int i = 0; i < tbl->ncolumns; i++ )
	{
		if( tbl->columns[i].name )
			sql_string_destroy(tbl->columns[i].name);
	}

	memset(tbl, 0x00, sizeof(*tbl));
}
