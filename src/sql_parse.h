#ifndef SQL_PARSE_H_
#define SQL_PARSE_H_
#include "sql_parsed.h"
#include "sql_string.h"

enum sql_parse_e
{
	SQL_PARSE_INVALID = 0,
	SQL_PARSE_INSERT,
	SQL_PARSE_CREATE_TABLE,
	SQL_PARSE_SELECT,
	SQL_PARSE_UPDATE,
};

struct SQLParse
{
	enum sql_parse_e type;
	union
	{
		struct SQLParsedInsert insert;
		struct SQLParsedCreateTable create_table;
		struct SQLParsedUpdate update;
		struct SQLParsedSelect select;
	} parse;
};

struct SQLParse* sql_parse_create(struct SQLString const* str);
void sql_parse_destroy(struct SQLParse* parse);

// TODO: Parse release

#endif