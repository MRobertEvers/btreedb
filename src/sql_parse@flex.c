#include "sql_parse@flex.h"

#include "sql_parse.h"

#include <string.h>

struct Lexer
{
	yyscan_t* scanner;
	int tok;
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

static void
cleanup_insert(struct SQLParsedInsert* insert)
{
	if( insert->table_name )
		sql_string_destroy(insert->table_name);

	for( int i = 0; i < insert->ncolumns; i++ )
	{
		if( insert->columns[i] )
			sql_string_destroy(insert->columns[i]);
	}

	for( int i = 0; i < insert->nvalues; i++ )
	{
		if( insert->values[i].value )
			sql_string_destroy(insert->values[i].value);
	}

	memset(insert, 0x00, sizeof(*insert));
}

static struct SQLParsedInsert
parse_insert(struct Lexer* lex, bool* success)
{
	struct SQLParsedInsert insert = {0};

	if( current(lex) != SQL_INSERT_KW )
		goto end;

	if( next(lex) != SQL_QUOTED_IDENTIFIER )
		goto end;

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
	success = false;
	cleanup_insert(&insert);
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

static void
cleanup_create_table(struct SQLParsedCreateTable* tbl)
{
	if( tbl->table_name )
		sql_string_destroy(tbl->table_name);

	for( int i = 0; i < tbl->ncolumns; i++ )
	{
		if( tbl->columns[i].name )
			sql_string_destroy(tbl->columns[i].name);
	}

	memset(tbl, 0x00, sizeof(*tbl));
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
			goto end;

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
	success = false;
	cleanup_create_table(&create_table);
	goto end;
}

struct SQLParse
sql_parse(struct SQLString* str)
{
	struct SQLParse parse = {0};
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
		parse.parse.insert = parse_insert(&lexer, &parse_success);
		parse.type = SQL_PARSE_INSERT;
		break;
	case SQL_CREATE_TABLE_KW:
		parse.parse.create_table = parse_create_table(&lexer, &parse_success);
		parse.type = SQL_PARSE_CREATE_TABLE;
		break;
	default:
		parse.type = SQL_PARSE_INVALID;
		goto cleanup;
	}

	if( !parse_success )
	{
		parse.type = SQL_PARSE_INVALID;
	}

cleanup:
	yy_flush_buffer(buffer, scanner);
	yy_delete_buffer(buffer, scanner);

	yylex_destroy(scanner);

	return parse;
}