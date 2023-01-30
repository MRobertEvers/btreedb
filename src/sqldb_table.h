#ifndef SQLDB_TABLE_H_
#define SQLDB_TABLE_H_

#include "btree_defs.h"
#include "sql_defs.h"
#include "sql_record.h"
#include "sql_table.h"
#include "sqldb_defs.h"

// TODO: Modifies both schema and record
// TODO: Rowid param is iffy
enum sql_e sqldb_table_prepare_record(
	struct SQLDB* db, struct SQLTable*, struct SQLRecord*, u32* out_row_id);

enum sql_e sqldb_table_btree_create(
	struct SQLDB* db, struct SQLTable*, struct BTree** out_tree);
enum sql_e sqldb_table_btree_destroy(
	struct SQLDB* db, struct SQLTable*, struct BTree* out_tree);

#endif