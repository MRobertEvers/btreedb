

#include "sql_main.h"

#include "btree_factory.h"
#include "btree_node_debug.h"
#include "ibtree.h"
#include "serialization.h"
#include "sql_ibtree.h"
#include "sql_lexer_types.h"
#include "sql_parse.h"
#include "sql_parsegen.h"
#include "sqldb.h"
#include "sqldb_table.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
temp()
{
	remove("sql_db.db");
	struct BTree* tree = btree_factory_create("sql_db.db");
	char create_tab_string[] =
		"CREATE TABLE \"billy\" ( \"name\" STRING, \"age\" INT )";
	char insert_into_string[] =
		"INSERT INTO \"billy\" (\"name\", \"age\") VALUES (\'herby_werby\', 9)";

	struct SQLTable table = {0};
	struct SQLRecord record = {0};
	struct SQLRecordSchema record_schema = {0};
	// Parse
	struct SQLParse tabparse =
		sql_parse(sql_string_create_from_cstring(create_tab_string));

	// Create table struct
	sql_parsegen_table_from_create_table(&tabparse.parse.create_table, &table);

	// Prepare record
	struct SQLParse insert_parse =
		sql_parse(sql_string_create_from_cstring(insert_into_string));
	sql_parsegen_record_schema_from_insert(
		&insert_parse.parse.insert, &record_schema);
	sql_parsegen_record_from_insert(
		&insert_parse.parse.insert, &record_schema, &record);
	sqldb_table_prepare_record(&table, &record);

	// Serialize record
	struct SQLSerializedRecord serred;
	sql_ibtree_serialize_record(&table, &record, &serred);

	dbg_print_buffer(serred.buf, serred.size);

	// Insert
	ibtree_insert(tree, serred.buf, serred.size);
}

int
main()
{
	remove("sql_db.db");
	char create_tab_string[] =
		"CREATE TABLE \"billy\" ( \"name\" STRING, \"age\" INT )";
	char insert_into_string[] =
		"INSERT INTO \"billy\" (\"name\", \"age\") VALUES (\'herby_werby\', 9)";

	struct SQLTable table = {0};
	struct SQLRecord record = {0};
	struct SQLRecordSchema record_schema = {0};
	// Parse
	struct SQLParse tabparse =
		sql_parse(sql_string_create_from_cstring(create_tab_string));

	// Create table struct
	sql_parsegen_table_from_create_table(&tabparse.parse.create_table, &table);

	struct SQLDB* db = NULL;
	sqldb_create(&db, "sql_db.db");

	sqldb_create_table(db, &table);
	sqldb_create_table(db, &table);

	// Prepare record
	// struct SQLParse insert_parse =
	// 	sql_parse(sql_string_create_from_cstring(insert_into_string));
	// sql_parsegen_record_schema_from_insert(
	// 	&insert_parse.parse.insert, &record_schema);
	// sql_parsegen_record_from_insert(
	// 	&insert_parse.parse.insert, &record_schema, &record);
	// sqldb_table_prepare_record(&table, &record_schema, &record);

	// // Serialize record
	// struct SQLSerializedRecord serred;
	// sql_ibtree_serialize_record(&table, &record_schema, &record,
	// &serred);

	// dbg_print_buffer(serred.buf, serred.size);

	// // Insert
	// ibtree_insert(tree, serred.buf, serred.size);

	return 0;
}
