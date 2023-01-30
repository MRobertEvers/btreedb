#ifndef SQL_STRING_H_
#define SQL_STRING_H_
#include "btint.h"

#include <stdbool.h>

struct SQLString
{
	char* ptr;
	u32 size;
	u32 capacity;
};

void sql_string_emplace(struct SQLString* str, char const* s, u32 size);
struct SQLString* sql_string_create(u32 capacity);
struct SQLString* sql_string_create_from(char const* s, u32 size);
struct SQLString* sql_string_create_from_cstring(char const* s);
struct SQLString* sql_string_copy(struct SQLString const* l);
void sql_string_move(struct SQLString** l, struct SQLString** r);

bool sql_string_equals(struct SQLString const* l, struct SQLString const* r);
char const* sql_string_raw(struct SQLString const* l);
u32 sql_string_len(struct SQLString const* l);

void sql_string_destroy(struct SQLString*);

#endif