

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
	// CREATE TABLE "a" ("age" INT, "name" STRING)
	// INSERT INTO "a" ("name", "age") VALUES ('hello', 88)
	// SELECT * FROM "a"
	remove("sql_db.db");
	// char create_tab_string[] = "CREATE TABLE \"my_table\" (\"age\" INT )";

	// struct SQLDB* db = NULL;
	// struct SQLTable* table = sql_table_create();

	// // Parse
	// struct SQLString* input =
	// sql_string_create_from_cstring(create_tab_string); struct SQLParse*
	// tabparse = sql_parse_create(input);

	// sqldb_create(&db, "sql_db.db");

	// sqldb_interpret(db, tabparse);

	// // end:
	// sql_table_destroy(table);
	// sql_parse_destroy(tabparse);
	// sql_string_destroy(input);

	return cli();
}
