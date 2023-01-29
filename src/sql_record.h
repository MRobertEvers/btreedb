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

struct SQLRecordSchema* sql_record_schema_create(void);
void sql_record_schema_destroy(struct SQLRecordSchema*);

struct SQLRecord* sql_record_create(void);
void sql_record_destroy(struct SQLRecord*);

int sql_record_schema_indexof(
	struct SQLRecordSchema* schema, struct SQLString* column_name);

void sql_record_schema_emplace_colname_c(
	struct SQLRecordSchema* schema, char const* value);

void sql_record_emplace_literal_c(
	struct SQLRecord* schema, char const* value, enum sql_literalstr_type_e);

void sql_record_emplace_literal(
	struct SQLRecord* record,
	struct SQLString const* lit,
	enum sql_literalstr_type_e);

void sql_record_emplace_number(struct SQLRecord*, int);

#endif