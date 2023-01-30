#include "sql_table.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

void
sql_column_init_c(
	struct SQLTableColumn* col,
	char const* name,
	enum sql_dt_e type,
	bool primary)
{
	// TODO: Free
	col->name = sql_string_create_from_cstring(name);
	col->type = type;
	col->is_primary_key = primary;
}

void
sql_column_init(
	struct SQLTableColumn* col,
	struct SQLString* name,
	enum sql_dt_e type,
	bool primary)
{
	// TODO: Free
	col->name = sql_string_copy(name);
	col->type = type;
	col->is_primary_key = primary;
}

void
sql_column_move(struct SQLTableColumn* l, struct SQLTableColumn* r)
{
	sql_string_move(&l->name, &r->name);
	l->type = r->type;
	l->is_primary_key = r->is_primary_key;

	memset(r, 0x00, sizeof(*r));
}

static void
sql_table_init_empty(struct SQLTable* tbl, char const* name)
{
	assert(tbl->table_name == NULL);
	memset(tbl, 0x00, sizeof(struct SQLTable));
	tbl->table_name = sql_string_create_from_cstring(name);
}

struct SQLTable*
sql_table_create()
{
	struct SQLTable* table = (struct SQLTable*)malloc(sizeof(struct SQLTable));
	memset(table, 0x00, sizeof(*table));
	return table;
}

void
sql_table_destroy(struct SQLTable* table)
{
	if( !table )
		return;
	sql_string_destroy(table->table_name);
	for( int i = 0; i < table->ncolumns; i++ )
	{
		sql_string_destroy(table->columns[i].name);
	}
	free(table);
}

void
sql_table_add_column(struct SQLTable* tbl, struct SQLTableColumn* col)
{
	sql_column_move(&tbl->columns[tbl->ncolumns++], col);
}

void
sql_table_emplace_column_c(
	struct SQLTable* tbl, char const* name, enum sql_dt_e type, bool primary)
{
	sql_column_init_c(&tbl->columns[tbl->ncolumns++], name, type, primary);
}

void
sql_table_emplace_column(
	struct SQLTable* tbl,
	struct SQLString* name,
	enum sql_dt_e type,
	bool primary)
{
	sql_column_init(&tbl->columns[tbl->ncolumns++], name, type, primary);
}

int
sql_table_find_primary_key(struct SQLTable* tbl)
{
	bool has_primary_key = false;
	for( int i = 0; i < tbl->ncolumns; i++ )
	{
		has_primary_key = tbl->columns[i].is_primary_key;
		if( has_primary_key )
			return i;
	}
	return -1;
}