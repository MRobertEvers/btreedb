#include "sql_record.h"

#include <stdlib.h>
#include <string.h>

struct SQLRecordSchema*
sql_record_schema_create(void)
{
	struct SQLRecordSchema* rs =
		(struct SQLRecordSchema*)malloc(sizeof(struct SQLRecordSchema));
	memset(rs, 0x00, sizeof(*rs));
	return rs;
}

void
sql_record_schema_destroy(struct SQLRecordSchema* rs)
{
	if( !rs )
		return;
	for( int i = 0; i < rs->ncolumns; i++ )
		sql_string_destroy(rs->columns[i]);
	if( rs )
		free(rs);
}

struct SQLRecord*
sql_record_create(void)
{
	struct SQLRecord* record =
		(struct SQLRecord*)malloc(sizeof(struct SQLRecord));
	memset(record, 0x00, sizeof(*record));
	return record;
}
void
sql_record_destroy(struct SQLRecord* record)
{
	if( !record )
		return;
	for( int i = 0; i < record->nvalues; i++ )
		sql_value_release(&record->values[i]);
	if( record )
		free(record);
}

int
sql_record_schema_indexof(
	struct SQLRecordSchema* schema, struct SQLString* column_name)
{
	for( int i = 0; i < schema->ncolumns; i++ )
	{
		if( sql_string_equals(column_name, schema->columns[i]) )
			return i;
	}
	return -1;
}

void
sql_record_schema_emplace_colname_c(
	struct SQLRecordSchema* schema, char const* value)
{
	schema->columns[schema->ncolumns] = sql_string_create_from_cstring(value);
	schema->ncolumns += 1;
}

void
sql_record_emplace_literal_c(
	struct SQLRecord* record,
	char const* value,
	enum sql_literalstr_type_e type)
{
	struct SQLLiteralStr literal = {
		.type = type, .value = sql_string_create_from_cstring(value)};

	// TODO: Error check
	sql_value_acquire_eval(&record->values[record->nvalues], &literal);
	record->nvalues += 1;

	sql_string_destroy(literal.value);
}

void
sql_record_emplace_literal(
	struct SQLRecord* record,
	struct SQLString const* value,
	enum sql_literalstr_type_e type)
{
	struct SQLLiteralStr literal = {
		.type = type, .value = sql_string_copy(value)};

	sql_value_acquire_eval(&record->values[record->nvalues], &literal);
	record->nvalues += 1;

	sql_string_destroy(literal.value);
}

void
sql_record_emplace_number(struct SQLRecord* record, int val)
{
	record->values[record->nvalues].type = SQL_VALUE_TYPE_INT;
	record->values[record->nvalues].value.num.num = val;
	record->values[record->nvalues].value.num.width = 4;
	record->nvalues += 1;
}
