#include "sql_value.h"

#include "serialization.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

enum sql_e
sql_value_acquire_eval(
	struct SQLValue* value, struct SQLLiteralValue const* sql_value)
{
	switch( sql_value->type )
	{
	case SQL_LITERALSTR_TYPE_STRING:
		value->value.string = sql_string_copy(sql_value->value);
		value->type = SQL_VALUE_TYPE_STRING;
		break;
	case SQL_LITERALSTR_TYPE_INT:
		value->value.num.num = atoi(sql_string_raw(sql_value->value));
		value->value.num.width = 4;
		value->type = SQL_VALUE_TYPE_INT;
		break;

	default:
		assert(0);
		break;
	}

	return SQL_OK;
}

enum sql_e
sql_value_release(struct SQLValue* value)
{
	switch( value->type )
	{
	case SQL_VALUE_TYPE_STRING:
		sql_string_free(value->value.string);
		break;
	case SQL_VALUE_TYPE_INT:
		break;
	case SQL_VALUE_TYPE_INVAL:
		assert(0);
		break;
	}

	memset(value, 0x00, sizeof(*value));
	return SQL_OK;
}

int
sql_value_array_serialize(
	struct SQLValue* vals, u32 nvals, void* buf, u32 buf_size)
{
	byte* ptr = buf;
	u32 written = 0;
	for( int i = 0; i < nvals; i++ )
	{
		// TODO: Bounds checking
		written +=
			sql_value_serialize(&vals[i], ptr + written, buf_size - written);
	}

	return written;
}

u32
sql_value_array_ser_size(struct SQLValue* vals, u32 nvals)
{
	u32 size = 0;
	for( int i = 0; i < nvals; i++ )
	{
		struct SQLValue* val = &vals[i];
		switch( val->type )
		{
		case SQL_VALUE_TYPE_STRING:
			size += 4;
			size += sql_string_len(val->value.string);
			break;
		case SQL_VALUE_TYPE_INT:
			size += 4;
			break;

		case SQL_VALUE_TYPE_INVAL:
			assert(0);
			break;

		default:
			assert(0);
			break;
		}
	}

	return size;
}

int
sql_string_ser(struct SQLString* val, void* buf, u32 size)
{
	byte* ptr = buf;
	ser_write_32bit_le(ptr, sql_string_len(val));
	ptr += 4;

	memcpy(ptr, sql_string_raw(val), sql_string_len(val));
	ptr += sql_string_len(val);

	return ptr - ((byte*)buf);
}

int
sql_value_serialize_int(int val, void* buf, u32 buf_size)
{
	ser_write_32bit_le(buf, val);
	return 4;
}

int
sql_value_serialize_string(struct SQLString* str, void* buf, u32 buf_size)
{
	return sql_string_ser(str, buf, buf_size);
}

int
sql_string_deser(struct SQLString** out_str, void* buf, u32 size)
{
	byte* ptr = buf;
	u32 len = 0;
	ser_read_32bit_le(&len, ptr);
	ptr += 4;

	*out_str = sql_string_create_from(ptr, len);
	ptr += len;

	return ptr - ((byte*)buf);
}

int
sql_value_serialize(struct SQLValue* val, void* buf, u32 size)
{
	byte* ptr = buf;
	switch( val->type )
	{
	case SQL_VALUE_TYPE_STRING:
		ptr +=
			sql_string_ser(val->value.string, ptr, size - (ptr - ((byte*)buf)));
		break;
	case SQL_VALUE_TYPE_INT:
		ser_write_32bit_le(ptr, val->value.num.num);
		ptr += 4;
		break;

	case SQL_VALUE_TYPE_INVAL:
		assert(0);
		break;

	default:
		assert(0);
		break;
	}

	return ptr - ((byte*)buf);
}

int
sql_value_deserialize_as(
	struct SQLValue* val, enum sql_value_type_e type, void* buf, u32 size)
{
	byte* ptr = buf;
	switch( type )
	{
	case SQL_VALUE_TYPE_STRING:
		ptr += sql_string_deser(&val->value.string, buf, size);
		val->type = type;
		break;
	case SQL_VALUE_TYPE_INT:
		ser_read_32bit_le(&val->value.num.num, buf);
		val->type = type;
		ptr += 4;
		break;

	case SQL_VALUE_TYPE_INVAL:
		assert(0);
		break;

	default:
		assert(0);
		break;
	}

	return ptr - ((byte*)buf);
}