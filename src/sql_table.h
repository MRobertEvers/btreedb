#ifndef SQL_TABLE_H_
#define SQL_TABLE_H_

#include "btint.h"
#include "sql_dt.h"
#include "sql_string.h"

#include <stdbool.h>

struct SQLTableColumn
{
	struct SQLString* name;
	enum sql_dt_e type;
	bool is_primary_key;
};

struct SQLTable
{
	u32 ncolumns;
	struct SQLTableColumn columns[8];

	struct SQLString* table_name;
};

void sql_column_init(
	struct SQLTableColumn* col,
	char const* name,
	enum sql_dt_e type,
	bool primary);
void sql_column_move(struct SQLTableColumn* l, struct SQLTableColumn* r);

void sql_table_init_empty(struct SQLTable* tbl, char const* name);
void sql_table_add_column(struct SQLTable* tbl, struct SQLTableColumn* col);
void sql_table_emplace_column(
	struct SQLTable* tbl, char const* name, enum sql_dt_e type, bool primary);

int sql_table_find_primary_key(struct SQLTable* tbl);

#endif