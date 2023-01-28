#include "sql_string.h"

#include <stdlib.h>
#include <string.h>

void
sql_string_emplace(struct SQLString* str, char const* s, u32 size)
{
	memcpy(str->ptr, s, size);
	str->size = size;
}

struct SQLString*
sql_string_create(u32 capacity)
{
	void* buf = malloc(sizeof(struct SQLString) + capacity);

	struct SQLString* str = (struct SQLString*)buf;

	// Cute trick to get a pointer just passed the end of the struct.
	str->ptr = (char*)&str[1];
	str->capacity = capacity;
	str->size = 0;

	return str;
}

struct SQLString*
sql_string_create_from(char const* s, u32 size)
{
	struct SQLString* str = sql_string_create(size);

	sql_string_emplace(str, s, size);

	return str;
}

struct SQLString*
sql_string_create_from_cstring(char const* s)
{
	return sql_string_create_from(s, strnlen(s, 512));
}

struct SQLString*
sql_string_copy(struct SQLString const* l)
{
	return sql_string_create_from(l->ptr, l->size);
}

void
sql_string_move(struct SQLString* l, struct SQLString* r)
{
	r->capacity = l->capacity;
	r->size = l->capacity;
	r->ptr = l->ptr;

	memset(l, 0x00, sizeof(struct SQLString));
}

bool
sql_string_equals(struct SQLString const* l, struct SQLString const* r)
{
	return l->size == r->size && memcmp(l->ptr, r->ptr, l->size) == 0;
}

char const*
sql_string_raw(struct SQLString const* l)
{
	return l->ptr;
}

u32
sql_string_len(struct SQLString const* l)
{
	return l->size;
}

void
sql_string_destroy(struct SQLString* str)
{
	free(str);
}