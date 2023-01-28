#ifndef SQL_VALUE_H_
#define SQL_VALUE_H_

#include "sql_defs.h"
#include "sql_literalstr.h"
#include "sql_string.h"

enum sql_value_type_e
{
	SQL_VALUE_TYPE_INVAL,
	SQL_VALUE_TYPE_STRING,
	SQL_VALUE_TYPE_INT,
};

struct SQLNumber
{
	long long num;
	u8 width;
};

struct SQLValue
{
	union
	{
		struct SQLNumber num;
		struct SQLString* string;
	} value;
	enum sql_value_type_e type;
};

enum sql_e sql_value_acquire_eval(
	struct SQLValue* value, struct SQLLiteralStr const* sql_value);

enum sql_e sql_value_release(struct SQLValue* value);

int sql_value_serialize_int(int, void* buf, u32 buf_size);
int sql_value_serialize_string(
	struct SQLString const* str, void* buf, u32 buf_size);
int sql_value_array_serialize(
	struct SQLValue* vals, u32 nvals, void* buf, u32 size);
u32 sql_value_array_ser_size(struct SQLValue* vals, u32 nvals);

int sql_value_serialize(struct SQLValue* val, void* buf, u32 size);
int sql_value_deserialize_as(
	struct SQLValue* val, enum sql_value_type_e type, void* buf, u32 size);

#endif