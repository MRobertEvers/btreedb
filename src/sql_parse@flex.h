#ifndef SQL_PARSE @FLEX_H_
#define SQL_PARSE @FLEX_H_

#include "sql_lexer_types.h"

// https://westes.github.io/flex/manual/Reentrant-Detail.html#Reentrant-Detail
typedef void* yyscan_t;
extern int yylex_init(yyscan_t* ptr_yy_globals);
extern char* yyget_text(yyscan_t yyscanner);
unsigned int yyget_leng(yyscan_t yyscanner);
typedef struct yy_buffer_state* YY_BUFFER_STATE;
extern YY_BUFFER_STATE yy_scan_string(const char* yystr, yyscan_t yyscanner);
extern YY_BUFFER_STATE
yy_scan_bytes(const char* yystr, unsigned int size, yyscan_t yyscanner);
extern void yy_flush_buffer(YY_BUFFER_STATE b, yyscan_t yyscanner);
extern void yy_delete_buffer(YY_BUFFER_STATE b, yyscan_t yyscanner);
extern int yylex(yyscan_t yyscanner);
extern int yylex_destroy(yyscan_t yyscanner);

#endif