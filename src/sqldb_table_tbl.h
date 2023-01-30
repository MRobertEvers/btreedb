#ifndef SQLDB_TABLE_TBL_H_
#define SQLDB_TABLE_TBL_H_

#include "pager.h"
#include "sql_defs.h"
#include "sql_string.h"
#include "sqldb_defs.h"

struct SQLDBMetaTable sqldb_table_tbl_create(struct Pager* pager);

enum sql_e sqldb_table_tbl_find(
	struct SQLDB* sqldb, struct SQLString* name, struct SQLTable* out_table);

enum sql_e sqldb_table_tbl_serialize_table_def_size(struct SQLTable* table);

/**
 * @brief Serialize a table definition
 *
 * TODO: This sidesteps the default serialization so this may be prone to bugs.
 *
 * Normally would should define a "TABLE" table and then convert the SQLTable
 * struct to a record for that table and then use the normal record
 * serialization code.
 *
 * @param table
 * @param table_id
 * @param buf
 * @param buf_size
 * @return enum sql_e
 */
enum sql_e sqldb_table_tbl_serialize_table_def(
	struct SQLTable* table, void* buf, u32 buf_size);

enum sql_e sqldb_table_tbl_deserialize_table_def(
	void* buf, u32 buf_size, struct SQLTable* out_table);

#endif