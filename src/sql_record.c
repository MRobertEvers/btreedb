#include "sql_record.h"

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
sql_record_emplace_literal_c(
	struct SQLRecord* record,
	char const* value,
	enum sql_literalstr_type_e type)
{
	struct SQLLiteralValue literal = {
		.type = type, .value = sql_string_create_from_cstring(value)};

	// TODO: Error check
	sql_value_acquire_eval(&record->values[record->nvalues], &literal);
	record->nvalues += 1;

	sql_string_free(literal.value);
}

void
sql_record_emplace_literal(
	struct SQLRecord* record,
	struct SQLString const* value,
	enum sql_literalstr_type_e type)
{
	struct SQLLiteralValue literal = {
		.type = type, .value = sql_string_copy(value)};

	sql_value_acquire_eval(&record->values[record->nvalues], &literal);
	record->nvalues += 1;

	sql_string_free(literal.value);
}