#include "sql_literal.h"

#include "serialization.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

int
sql_literal_array_serialize(
	struct SQLLiteralValue** vals, u32 nvals, void* buf, u32 buf_size)
{
	byte* ptr = buf;
	u32 written = 0;
	for( int i = 0; i < nvals; i++ )
	{
		// TODO: Bounds checking
		written +=
			sql_literal_serialize(vals[i], ptr + written, buf_size - written);
	}

	return written;
}
int
sql_literal_serialize(struct SQLLiteralValue* val, void* buf, u32 size)
{
	byte* ptr = buf;
	switch( val->type )
	{
	case SQL_LITERAL_TYPE_STRING:
		ser_write_32bit_le(ptr, sql_string_len(val->value));
		ptr += 4;

		memcpy(ptr, sql_string_raw(val->value), sql_string_len(val->value));
		ptr += sql_string_len(val->value);
		break;
	case SQL_LITERAL_TYPE_INT:
	{
		int i = atoi(val->value);
		ser_write_32bit_le(ptr, i);
		ptr += 4;
	}
	break;

	default:
		assert(0);
		break;
	}

	return ptr - ((byte*)buf);
}

u32
sql_literal_array_ser_size(struct SQLLiteralValue** vals, u32 nvals)
{
	u32 size = 0;
	for( int i = 0; i < nvals; i++ )
	{
		struct SQLLiteralValue* val = vals[i];
		switch( val->type )
		{
		case SQL_LITERAL_TYPE_STRING:
			size += 4;
			size += sql_string_len(val->value);
			break;
		case SQL_LITERAL_TYPE_INT:
			size += 4;
			break;

		default:
			assert(0);
			break;
		}
	}

	return size;
}