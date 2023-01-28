#ifndef SQL_LITERALSTR_H_
#define SQL_LITERALSTR_H_

#include "btint.h"
#include "sql_string.h"

enum sql_literalstr_type_e
{
	SQL_LITERALSTR_TYPE_INVAL,
	SQL_LITERALSTR_TYPE_STRING,
	SQL_LITERALSTR_TYPE_INT,
};

struct SQLLiteralValue
{
	struct SQLString* value;
	enum sql_literalstr_type_e type;
};

#endif