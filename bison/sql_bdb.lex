/* Mini Calculator */
/* calc.lex */

%{
#include "heading.h"
#include "tok.h"
int yyerror(char *s);
// int yylineno = 1;
%}

named_character [^"\\]
digit		[0-9]
int_const	{digit}+
named_const {named_character}+ 


%%

"INSERT INTO"		{ yylval.ptr = yytext; return SQL_INSERT_LITERAL; }
\"{named_const}\"		{ yylval.ptr = yytext; return SQL_NAMED_LITERAL; }

[ \t]*		{}
[\n]		{ yylineno++;	}

.		{ std::cerr << "SCANNER "; yyerror(""); exit(1);	}
