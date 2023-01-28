#ifndef SQL_IBTREE_H_
#define SQL_IBTREE_H_

#include "ibtree_layout_schema.h"
#include "sql_defs.h"
#include "sql_parsed.h"
#include "sql_record.h"
#include "sql_table.h"

// TODO: This function must create the schema in the same way it serializes.
enum sql_e sql_ibtree_create_ibt_layout_schema(
	struct SQLTable* tbl, struct IBTreeLayoutSchema* out_schema);

enum sql_e sql_ibtree_create_ibt_layout_schema(
	struct SQLTable* tbl, struct IBTreeLayoutSchema* out_schema);

struct SQLSerializedRecord
{
	void* buf;
	u32 size;
};

enum sql_e sql_ibtree_serialize_record(
	struct SQLTable*,
	struct SQLRecordSchema*,
	struct SQLRecord*,
	struct SQLSerializedRecord*);

#endif