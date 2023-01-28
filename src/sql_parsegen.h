#ifndef SQL_PARSEGEN_H_
#define SQL_PARSEGEN_H_

#include "sql_defs.h"
#include "sql_parsed.h"
#include "sql_record.h"
#include "sql_table.h"

void sql_parsegen_table_from_create_table(
	struct SQLParsedCreateTable const*, struct SQLTable*);

enum sql_e sql_parsegen_record_schema_from_insert(
	struct SQLParsedInsert const*, struct SQLRecordSchema*);

enum sql_e sql_parsegen_record_from_insert(
	struct SQLParsedInsert const*, struct SQLRecordSchema*, struct SQLRecord*);

#endif