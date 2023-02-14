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
		sql_parsed_update_cleanup(&parse->parse.update);
		break;
	case SQL_PARSE_SELECT:
		sql_parsed_select_cleanup(&parse->parse.select);
		break;
	case SQL_PARSE_DELETE:
		sql_parsed_delete_cleanup(&parse->parse.delete);
		break;
	case SQL_PARSE_INVALID:
		break;
	}

	free(parse);
}