#include "sql_table.h"

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
	sql_string_move(l->name, r->name);
	r->type = l->type;
	r->is_primary_key = l->is_primary_key;

	memset(l, 0x00, sizeof(*l));
}

void
sql_table_init_empty(struct SQLTable* tbl, char const* name)
{
	memset(tbl, 0x00, sizeof(struct SQLTable));
	tbl->table_name = sql_string_create_from_cstring(name);
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