#ifndef SQL_RECORD_H_
#define SQL_RECORD_H_

#include "btint.h"
#include "sql_string.h"
#include "sql_value.h"

// Column names for records; multiple records may reference this.
/**
 * @brief
 *
 */
struct SQLRecordSchema
{
	// TODO: Not fixed size.
	struct SQLString* columns[8];
	u32 ncolumns;
};

struct SQLRecord
{
	struct SQLRecordSchema* schema;
	struct SQLValue values[8];
	u32 nvalues;
};

int sql_record_schema_indexof(
	struct SQLRecordSchema* schema, struct SQLString* column_name);

void sql_record_emplace_literal_c(
	struct SQLRecord* schema, char const* value, enum sql_literalstr_type_e);

void sql_record_emplace_literal(
	struct SQLRecord* record,
	struct SQLString const* lit,
	enum sql_literalstr_type_e);

#endif