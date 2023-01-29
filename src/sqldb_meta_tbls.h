#ifndef SQLDB_META_TBLS_H_
#define SQLDB_META_TBLS_H_

#include "btint.h"
#include "sql_defs.h"
#include "sql_table.h"
#include "sqldb_defs.h"

enum sql_e sqldb_meta_tables_create(struct SQLDB* sqldb, char const* filename);

enum sql_e sqldb_meta_deserialize_table_def(
	void* buf, u32 buf_size, struct SQLTable* out_table);
enum sql_e sqldb_meta_serialize_table_def_size(struct SQLTable* table);

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
enum sql_e sqldb_meta_serialize_table_def(
	struct SQLTable* table, int table_id, void* buf, u32 buf_size);

#endif