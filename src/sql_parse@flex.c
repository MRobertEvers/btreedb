#include "sql_parse@flex.h"

#include "sql_parse.h"
#include "sql_parsed.h"

#include <stdlib.h>
#include <string.h>

struct Lexer
{
	yyscan_t* scanner;
	enum sql_token_e tok;
};

static int
next(struct Lexer* lexer)
{
	return lexer->tok = yylex(*lexer->scanner);
}

static int
current(struct Lexer* lexer)
{
	return lexer->tok;
}

static char*
text(struct Lexer* lexer)
{
	return yyget_text(*lexer->scanner);
}

static u32
leng(struct Lexer* lexer)
{
	return yyget_leng(*lexer->scanner);
}

static enum sql_literalstr_type_e
get_data_type_from_sql_type(enum sql_token_e tok_type)
{
	switch( tok_type )
	{
	case SQL_INT_LITERAL:
		return SQL_LITERALSTR_TYPE_INT;
	case SQL_STRING_LITERAL:
		return SQL_LITERALSTR_TYPE_STRING;
	default:
		return SQL_LITERALSTR_TYPE_INVAL;
	}
}

static struct SQLParsedInsert
parse_insert(struct Lexer* lex, bool* success)
{
	struct SQLParsedInsert insert = {0};

	if( current(lex) != SQL_INSERT_KW )
		goto end;

	if( next(lex) != SQL_QUOTED_IDENTIFIER )
		goto fail;

	insert.table_name = sql_string_create_from(text(lex), leng(lex));

	if( next(lex) != SQL_OPEN_PAREN )
		goto fail;

	do
	{
		if( next(lex) != SQL_QUOTED_IDENTIFIER )
			goto fail;

		insert.columns[insert.ncolumns] =
			sql_string_create_from(text(lex), leng(lex));

		insert.ncolumns += 1;
	} while( next(lex) == SQL_COMMA );

	if( current(lex) != SQL_CLOSE_PAREN )
		goto fail;

	if( next(lex) != SQL_VALUES_KW )
		goto fail;

	if( next(lex) != SQL_OPEN_PAREN )
		goto fail;

	do
	{
		next(lex);
		if( current(lex) != SQL_STRING_LITERAL &&
			current(lex) != SQL_INT_LITERAL )
			goto fail;

		insert.values[insert.nvalues].type =
			get_data_type_from_sql_type(current(lex));

		insert.values[insert.nvalues].value =
			sql_string_create_from(text(lex), leng(lex));

		insert.nvalues += 1;

	} while( next(lex) == SQL_COMMA && insert.nvalues < 4 );

	if( current(lex) != SQL_CLOSE_PAREN )
		goto fail;

end:

	return insert;

fail:
	*success = false;
	sql_parsed_insert_cleanup(&insert);
	goto end;
}

static enum sql_dt_e
get_data_type(char const* s)
{
	if( strcmp(s, "STRING") == 0 )
		return SQL_DT_STRING;
	else if( strcmp(s, "INT") == 0 )
		return SQL_DT_INT;
	else
		return SQL_DT_INVAL;
}

static struct SQLParsedWhereClause
parse_where(struct Lexer* lex, bool* success)
{
	struct SQLParsedWhereClause where = {0};
	if( current(lex) != SQL_WHERE_KW )
		goto fail;

	next(lex);
	if( current(lex) != SQL_QUOTED_IDENTIFIER &&
		current(lex) != SQL_IDENTIFIER )
		goto fail;

	where.field = sql_string_create_from(text(lex), leng(lex));

	if( next(lex) != SQL_EQUAL_KW )
		goto fail;

	next(lex);
	if( current(lex) != SQL_STRING_LITERAL && current(lex) != SQL_INT_LITERAL )
		goto fail;

	where.value.type = get_data_type_from_sql_type(current(lex));
	where.value.value = sql_string_create_from(text(lex), leng(lex));

end:
	return where;
fail:
	*success = false;
	goto end;
}

static struct SQLParsedCreateTable
parse_create_table(struct Lexer* lex, bool* success)
{
	struct SQLParsedCreateTable create_table = {0};

	if( current(lex) != SQL_CREATE_TABLE_KW )
		goto end;

	if( next(lex) != SQL_QUOTED_IDENTIFIER )
		goto end;

	create_table.table_name = sql_string_create_from(text(lex), leng(lex));

	if( next(lex) != SQL_OPEN_PAREN )
		goto fail;

	do
	{
		if( next(lex) != SQL_QUOTED_IDENTIFIER )
			goto fail;

		create_table.columns[create_table.ncolumns].name =
			sql_string_create_from(text(lex), leng(lex));

		if( next(lex) != SQL_IDENTIFIER )
			goto fail;

		enum sql_dt_e dt = get_data_type(text(lex));
		if( dt == SQL_DT_INVAL )
			goto fail;

		create_table.columns[create_table.ncolumns].type = dt;
		create_table.columns[create_table.ncolumns].is_primary_key = false;

		create_table.ncolumns += 1;
	} while( next(lex) == SQL_COMMA && create_table.ncolumns < 4 );

	if( current(lex) != SQL_CLOSE_PAREN )
		goto fail;

end:
	return create_table;
fail:
	*success = false;
	sql_parsed_create_table_cleanup(&create_table);
	goto end;
}

static struct SQLParsedSelect
parse_select(struct Lexer* lex, bool* success)
{
	struct SQLParsedSelect select = {0};

	if( current(lex) != SQL_SELECT_KW )
		goto fail;

	do
	{
		next(lex);

		if( current(lex) != SQL_QUOTED_IDENTIFIER &&
			current(lex) != SQL_IDENTIFIER && current(lex) != SQL_STAR_KW )
			goto fail;

		select.fields[select.nfields] =
			sql_string_create_from(text(lex), leng(lex));
		select.nfields += 1;

		next(lex);
	} while( current(lex) == SQL_COMMA );

	if( current(lex) != SQL_FROM_KW )
		goto fail;

	if( next(lex) != SQL_QUOTED_IDENTIFIER )
		goto fail;

	select.table_name = sql_string_create_from(text(lex), leng(lex));

	if( next(lex) != SQL_WHERE_KW )
		goto end;

	select.where = parse_where(lex, success);
end:
	return select;
fail:
	*success = false;
	sql_parsed_select_cleanup(&select);
	goto end;
}

static struct SQLParsedUpdate
parse_update(struct Lexer* lex, bool* success)
{
	struct SQLParsedUpdate update = {0};

	if( current(lex) != SQL_UPDATE_KW )
		goto fail;

	if( next(lex) != SQL_QUOTED_IDENTIFIER )
		goto fail;

	update.table_name = sql_string_create_from(text(lex), leng(lex));

	if( next(lex) != SQL_SET_KW )
		goto fail;

	do
	{
		next(lex);
		if( current(lex) != SQL_QUOTED_IDENTIFIER &&
			current(lex) != SQL_IDENTIFIER )
			goto fail;

		update.columns[update.ncolumns] =
			sql_string_create_from(text(lex), leng(lex));
		update.ncolumns += 1;

		if( next(lex) != SQL_EQUAL_KW )
			goto fail;

		next(lex);
		if( current(lex) != SQL_STRING_LITERAL &&
			current(lex) != SQL_INT_LITERAL )
			goto fail;

		update.values[update.nvalues].type =
			get_data_type_from_sql_type(current(lex));

		update.values[update.nvalues].value =
			sql_string_create_from(text(lex), leng(lex));

		update.nvalues += 1;

	} while( current(lex) == SQL_COMMA );

	if( next(lex) != SQL_WHERE_KW )
		goto end;

	update.where = parse_where(lex, success);

end:
	return update;
fail:
	*success = false;
	sql_parsed_update_cleanup(&update);
	goto end;
}

static struct SQLParsedDelete
parse_delete(struct Lexer* lex, bool* success)
{
	struct SQLParsedDelete delete = {0};

	if( current(lex) != SQL_DELETE_KW )
		goto fail;

	if( next(lex) != SQL_QUOTED_IDENTIFIER )
		goto fail;

	delete.table_name = sql_string_create_from(text(lex), leng(lex));

	if( next(lex) != SQL_WHERE_KW )
		goto end;

	delete.where = parse_where(lex, success);

end:
	return delete;
fail:
	*success = false;
	sql_parsed_delete_cleanup(&delete);
	goto end;
}

struct SQLParse*
sql_parse_create(struct SQLString const* str)
{
	struct SQLParse* parse = (struct SQLParse*)malloc(sizeof(struct SQLParse));
	struct Lexer lexer = {0};
	bool parse_success = true;

	yyscan_t scanner;
	lexer.scanner = &scanner;

	yylex_init(&scanner);

	YY_BUFFER_STATE buffer =
		yy_scan_bytes(sql_string_raw(str), sql_string_len(str), scanner);

	switch( next(&lexer) )
	{
	case SQL_INSERT_KW:
		parse->parse.insert = parse_insert(&lexer, &parse_success);
		parse->type = SQL_PARSE_INSERT;
		break;
	case SQL_CREATE_TABLE_KW:
		parse->parse.create_table = parse_create_table(&lexer, &parse_success);
		parse->type = SQL_PARSE_CREATE_TABLE;
		break;
	case SQL_SELECT_KW:
		parse->parse.select = parse_select(&lexer, &parse_success);
		parse->type = SQL_PARSE_SELECT;
		break;
	case SQL_UPDATE_KW:
		parse->parse.update = parse_update(&lexer, &parse_success);
		parse->type = SQL_PARSE_UPDATE;
		break;
	case SQL_DELETE_KW:
		parse->parse.delete = parse_delete(&lexer, &parse_success);
		parse->type = SQL_PARSE_DELETE;
		break;
	default:
		parse->type = SQL_PARSE_INVALID;
		goto cleanup;
	}

	if( !parse_success )
	{
		parse->type = SQL_PARSE_INVALID;
	}

cleanup:
	yy_flush_buffer(buffer, scanner);
	yy_delete_buffer(buffer, scanner);

	yylex_destroy(scanner);

	return parse;
}
