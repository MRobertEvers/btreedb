#include "sql_lexer_utils.h"

// https://westes.github.io/flex/manual/Accessor-Methods.html#Accessor-Methods

typedef unsigned int yy_size_t;

// https://westes.github.io/flex/manual/Reentrant-Functions.html#Reentrant-Functions
extern yy_size_t yyget_leng(yyscan_t yyscanner);
extern char* yyget_text(yyscan_t yyscanner);

void
sqlex_chop(yyscan_t scanner)
{
	int len = yyget_leng(scanner);
	yyget_text(scanner)[len - 1] = '\0';
}