#ifndef SQL_PARSE_H_
#define SQL_PARSE_H_
#include "sql_parsed.h"
#include "sql_string.h"

enum sql_parse_e
{
	SQL_PARSE_INVALID = 0,
	SQL_PARSE_INSERT,
	SQL_PARSE_CREATE_TABLE,
};

struct SQLParse
{
	enum sql_parse_e type;
	union
	{
		struct SQLParsedInsert insert;
		struct SQLParsedCreateTable create_table;
	} parse;
};

struct SQLParse sql_parse(struct SQLString* str);

// TODO: Parse release

#endif