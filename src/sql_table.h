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
	bool is_primary_key; // TODO: Autoincrement not implied
};

struct SQLTableMeta
{
	u32 root_page;
	u32 table_id;
};

struct SQLTable
{
	u32 ncolumns;
	struct SQLTableColumn columns[8];

	struct SQLString* table_name;

	struct SQLTableMeta meta;
};

void sql_column_init_c(
	struct SQLTableColumn* col,
	char const* name,
	enum sql_dt_e type,
	bool primary);
void sql_column_init(
	struct SQLTableColumn* col,
	struct SQLString* name,
	enum sql_dt_e type,
	bool primary);
// void sql_column_move(struct SQLTableColumn** l, struct SQLTableColumn** r);

struct SQLTable* sql_table_create(void);
void sql_table_destroy(struct SQLTable*);
void sql_table_move(struct SQLTable** l, struct SQLTable** r);
// void sql_table_add_column(struct SQLTable* tbl, struct SQLTableColumn* col);
void sql_table_emplace_column_c(
	struct SQLTable* tbl, char const* name, enum sql_dt_e type, bool primary);
void sql_table_emplace_column(
	struct SQLTable* tbl,
	struct SQLString* name,
	enum sql_dt_e type,
	bool primary);

int sql_table_find_primary_key(struct SQLTable* tbl);

#endif
