%option noyywrap
%option reentrant
%option noline

%{
#include "sql_lexer_types.h"
%}

named_character [0-9a-zA-Z]
digit		[0-9]
int_const	{digit}+
quoted_literal {named_character}+ 
sql_literal {named_character}+ 

%%

"CREATE TABLE"		{ return SQL_CREATE_TABLE; }
"INSERT INTO"		{ return SQL_INSERT_LITERAL; }
\"{quoted_literal}\"		{  return SQL_NAMED_LITERAL; }
{sql_literal}		{  return SQL_KEYWORD_LITERAL; }
","		{  return SQL_COMMA; }
\(|\) /* do nothing */ { yytext += 1;}
[ \n\r\n]  /* do nothing */ { yytext += 1;}
