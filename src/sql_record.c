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
	struct SQLRecord* record, char const* value, enum sql_literal_type_e type)
{
	record->values[record->nvalues].type = type;
	record->values[record->nvalues].value =
		sql_string_create_from_cstring(value);
	record->nvalues += 1;
}