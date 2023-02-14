%option noyywrap
%option reentrant
%option noline

%{
#include "sql_lexer_types.h"
%}

name_character [0-9a-zA-Z_]
digit		[0-9]
int_const	{digit}+
quoted_literal {name_character}+ 
sql_literal {name_character}+ 

%%

\'[^']+\'		{  return SQL_STRING_LITERAL; }
\"{quoted_literal}\"		{  return SQL_QUOTED_IDENTIFIER; }
{int_const} {  return SQL_INT_LITERAL; }
"CREATE TABLE"		{ return SQL_CREATE_TABLE_KW; }
"INSERT INTO"		{ return SQL_INSERT_KW; }
"VALUES"		{ return SQL_VALUES_KW; }
"SELECT"        { return SQL_SELECT_KW;}
"DELETE FROM"        { return SQL_DELETE_KW;}
"UPDATE"         { return SQL_UPDATE_KW;}
"WHERE" {return SQL_WHERE_KW;}
"FROM" {return SQL_FROM_KW;}
"SET" {return SQL_SET_KW;}
"*" {return SQL_STAR_KW;}
"=" {return SQL_EQUAL_KW;}
{sql_literal}		{  return SQL_IDENTIFIER; }
","		{  return SQL_COMMA; }
\(   { return SQL_OPEN_PAREN; } 
\)   { return SQL_CLOSE_PAREN; }
[ \n\r\n]  /* do nothing */ { yytext += 1;}
