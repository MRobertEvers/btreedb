/* Mini Calculator */
/* calc.y */

%{
#include "heading.h"
#include "ast_node.h"
int yyerror(char *s);
int yylex(void);
%}

/* 3.4.5 Providing a Structured Semantic Value Type */
%union{
    long long		val;
    char*	ptr;
}

%start	input 

%token <ptr>	SQL_INSERT_LITERAL
%type <ptr> insert_stmt

%token <ptr> SQL_NAMED_LITERAL

%%

input:		/* empty */
		| insert_stmt 	{ cout << "Result: " << $1 << endl; }
		;

insert_stmt:		SQL_INSERT_LITERAL SQL_NAMED_LITERAL	{ $$ = $2; }
		;

%%

int yyerror(string s)
{
  extern int yylineno;	// defined and maintained in lex.c
  extern char *yytext;	// defined and maintained in lex.c
  
  cerr << "ERROR: " << s << " at symbol \"" << yytext;
  cerr << "\" on line " << yylineno << endl;
  exit(1);
}

int yyerror(char *s)
{
  return yyerror(string(s));
}

