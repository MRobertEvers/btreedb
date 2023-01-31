

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
cli()
{
	enum sql_e sqle = SQL_OK;
	struct SQLDB* db = NULL;
	struct SQLString* input = NULL;
	struct SQLParse* parse = NULL;
	char buf[0x2000] = {0};
	u32 buf_len = 0;
	char c = 0;

	sqle = sqldb_create(&db, "sql_db.db");
	if( sqle != SQL_OK )
	{
		printf("Could not open database file.\n");
		return -1;
	}

	while( sqle == SQL_OK )
	{
		while( true )
		{
			c = getchar();
			buf[buf_len++] = c;

			if( c == '\n' )
				break;
		}

		input = sql_string_create_from(buf, buf_len);
		parse = sql_parse_create(input);

		sqle = sqldb_interpret(db, parse);
		printf("Command result: %i\n", sqle);

		sql_parse_destroy(parse);
		parse = NULL;
		sql_string_destroy(input);
		input = NULL;

		memset(buf, 0x00, buf_len);
		buf_len = 0;
	}

	return 0;
}

int
main()
{
	// 	remove("sql_db.db");
	// 	char create_tab_string[] =
	// 		"CREATE TABLE \"my_table\" ( \"name\" STRING, \"age\" INT )";
	// 	char insert_into_string[] = "INSERT INTO \"my_table\" (\"name\",
	// \"age\") " 								"VALUES (\'herby_werby\', 9)";
	// char select[] = "SELECT id,
	// \"name\" FROM \"my_table\""; 	char update[] = "UPDATE \"my_table\" SET
	// \"age\" = 11 WHERE \"name\" = "
	// 					"\'herby_werby\'";

	// 	struct SQLDB* db = NULL;
	// 	struct SQLTable* table = sql_table_create();

	// 	// Parse
	// 	struct SQLString* input =
	// sql_string_create_from_cstring(create_tab_string); 	struct SQLParse*
	// tabparse = sql_parse_create(input);

	// 	sqldb_create(&db, "sql_db.db");

	// 	sqldb_interpret(db, tabparse);

	// 	struct SQLString* insertinput =
	// 		sql_string_create_from_cstring(insert_into_string);
	// 	struct SQLParse* iparse = sql_parse_create(insertinput);
	// 	sqldb_interpret(db, iparse);

	// 	struct SQLString* selectinput = sql_string_create_from_cstring(select);
	// 	struct SQLParse* selectparse = sql_parse_create(selectinput);
	// 	sqldb_interpret(db, selectparse);

	// 	struct SQLString* updateinput = sql_string_create_from_cstring(update);
	// 	struct SQLParse* updateparse = sql_parse_create(updateinput);
	// 	sqldb_interpret(db, updateparse);

	// 	sqldb_interpret(db, selectparse);

	// end:
	// 	sql_table_destroy(table);
	// 	sql_parse_destroy(iparse);
	// 	sql_parse_destroy(tabparse);
	// 	sql_parse_destroy(selectparse);
	// 	sql_string_destroy(input);
	// 	sql_string_destroy(insertinput);

	return cli();
}
