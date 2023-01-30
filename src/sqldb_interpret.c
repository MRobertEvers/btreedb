#include "sqldb_interpret.h"

#include "sql_parsegen.h"
#include "sqldb.h"

static enum sql_e
create_table(struct SQLDB* db, struct SQLParsedCreateTable* create_table)
{
	enum sql_e result = SQL_OK;
	struct SQLTable* table = sql_table_create();

	// Create table struct
	sql_parsegen_table_from_create_table(create_table, table);

	result = sqldb_create_table(db, table);
	if( result != SQL_OK )
		goto end;

end:
	sql_table_destroy(table);
	return result;
}

// static enum sql_e
// create_table(struct SQLDB* db, struct SQLParsedInsert* insert)
// {
// 	enum sql_e result = SQL_OK;
// 	return result;
// }

enum sql_e
sqldb_interpret(struct SQLDB* db, struct SQLParse* parsed)
{
	enum sql_e result = SQL_OK;
	switch( parsed->type )
	{
	case SQL_PARSE_INVALID:
		break;
	case SQL_PARSE_CREATE_TABLE:
		result = create_table(db, &parsed->parse.create_table);
		break;
	case SQL_PARSE_INSERT:
		break;
	}

	return result;
}