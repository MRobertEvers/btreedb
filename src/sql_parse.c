#include "sql_parse.h"

#include "sql_parsed.h"

#include <stdlib.h>

void
sql_parse_destroy(struct SQLParse* parse)
{
	switch( parse->type )
	{
	case SQL_PARSE_INSERT:
		sql_parsed_insert_cleanup(&parse->parse.insert);
		break;
	case SQL_PARSE_CREATE_TABLE:
		sql_parsed_create_table_cleanup(&parse->parse.create_table);
		break;
	case SQL_PARSE_UPDATE:
		break;
	case SQL_PARSE_SELECT:
		break;
	case SQL_PARSE_INVALID:
		break;
	}

	free(parse);
}