#ifndef SQL_LITERAL_H_
#define SQL_LITERAL_H_

#include "btint.h"
#include "sql_string.h"

enum sql_literal_type_e
{
	SQL_LITERAL_TYPE_INVAL,
	SQL_LITERAL_TYPE_STRING,
	SQL_LITERAL_TYPE_INT,
};

struct SQLLiteralValue
{
	struct SQLString* value;
	enum sql_literal_type_e type;
};

int sql_literal_array_serialize(
	struct SQLLiteralValue* vals, u32 nvals, void* buf, u32 size);

int sql_literal_serialize(struct SQLLiteralValue* val, void* buf, u32 size);
u32 sql_literal_array_ser_size(struct SQLLiteralValue* vals, u32 nvals);

#endif