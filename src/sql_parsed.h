#ifndef SQL_PARSED_H_
#define SQL_PARSED_H_
#include "btint.h"
#include "sql_dt.h"
#include "sql_record.h"
#include "sql_string.h"

/**
 * @brief Insert
 *
 */

struct SQLParsedInsert
{
	struct SQLString* table_name;
	struct SQLString* columns[5];
	u32 ncolumns;
	struct SQLLiteralValue values[5];
	u32 nvalues;
};

/**
 * @brief Create Table
 *
 */

struct SQLParsedCreateTableColumn
{
	struct SQLString* name;
	enum sql_dt_e type;
	bool is_primary_key;
};

struct SQLParsedCreateTable
{
	struct SQLString* table_name;
	struct SQLParsedCreateTableColumn columns[5];
	u32 ncolumns;
};

#endif