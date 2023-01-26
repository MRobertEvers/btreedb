

#include "sql_main.h"

#include <stdio.h>

// https://westes.github.io/flex/manual/Reentrant-Detail.html#Reentrant-Detail

typedef void* yyscan_t;
extern int yylex_init(yyscan_t* ptr_yy_globals);
extern char* yyget_text(yyscan_t yyscanner);
typedef struct yy_buffer_state* YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char* yystr, yyscan_t yyscanner);
extern void yy_delete_buffer(YY_BUFFER_STATE b, yyscan_t yyscanner);
// extern int
// yylex_init_extra(YY_EXTRA_TYPE user_defined, yyscan_t* ptr_yy_globals);
extern int yylex(yyscan_t yyscanner);
extern int yylex_destroy(yyscan_t yyscanner);

int
main()
{
	char string[] = "CREATE TABLE \"billy\" ( \"name\" STRING, \"age\" INT )";

	yyscan_t scanner;
	int tok;

	yylex_init(&scanner);
	YY_BUFFER_STATE buffer = yy_scan_string(string, scanner);

	while( (tok = yylex(scanner)) > 0 )
		printf("tok=%d  yytext=%s\n", tok, yyget_text(scanner));

	yy_delete_buffer(buffer, scanner);

	yylex_destroy(scanner);
	return 0;
	return 0;
}

// int
// main()
// {
// 	// yyscan_t scanner;
// 	// yylex_init(&scanner);
// 	// yyset_in(fopen(argv[1], "rb"), scanner);
// 	// yylex(scanner);
// 	// yylex_destroy(scanner);

// }