

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
#include "sqldb_interpret.h"
#include "sqldb_table.h"
#include "sqldb_table_tbl.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
temp()
{
	// remove("sql_db.db");
	// struct BTree* tree = btree_factory_create("sql_db.db");
	// char create_tab_string[] =
	// 	"CREATE TABLE \"billy\" ( \"name\" STRING, \"age\" INT )";
	// char insert_into_string[] =
	// 	"INSERT INTO \"billy\" (\"name\", \"age\") VALUES (\'herby_werby\', 9)";

	// struct SQLTable table = {0};
	// struct SQLRecord* record = sql_record_create();
	// struct SQLRecordSchema* record_schema = sql_record_schema_create();

	// // Parse
	// struct SQLParse* tabparse =
	// 	sql_parse_create(sql_string_create_from_cstring(create_tab_string));

	// // Create table struct
	// sql_parsegen_table_from_create_table(&tabparse->parse.create_table,
	// &table);

	// // Prepare record
	// struct SQLParse* insert_parse =
	// 	sql_parse_create(sql_string_create_from_cstring(insert_into_string));
	// sql_parsegen_record_schema_from_insert(
	// 	&insert_parse->parse.insert, record_schema);
	// sql_parsegen_record_from_insert(
	// 	&insert_parse->parse.insert, record_schema, record);
	// sqldb_table_prepare_record(&table, record);

	// // Serialize record
	// struct SQLSerializedRecord serred;
	// sql_ibtree_serialize_record_acquire(&serred, &table, record);

	// dbg_print_buffer(serred.buf, serred.size);

	// // Insert
	// ibtree_insert(tree, serred.buf, serred.size);

	return 0;
}

int
main()
{
	remove("sql_db.db");
	char create_tab_string[] =
		"CREATE TABLE \"my_table\" ( \"name\" STRING, \"age\" INT )";
	char insert_into_string[] = "INSERT INTO \"my_table\" (\"name\", \"age\") "
								"VALUES (\'herby_werby\', 9)";

	struct SQLDB* db = NULL;
	struct SQLTable* table = sql_table_create();

	// Parse
	struct SQLString* input = sql_string_create_from_cstring(create_tab_string);
	struct SQLParse* tabparse = sql_parse_create(input);

	sqldb_create(&db, "sql_db.db");

	sqldb_interpret(db, tabparse);

	struct SQLString* insertinput =
		sql_string_create_from_cstring(insert_into_string);
	struct SQLParse* iparse = sql_parse_create(insertinput);
	sqldb_interpret(db, iparse);

end:
	sql_table_destroy(table);
	sql_parse_destroy(iparse);
	sql_parse_destroy(tabparse);
	sql_string_destroy(input);
	sql_string_destroy(insertinput);
	return 0;
}
