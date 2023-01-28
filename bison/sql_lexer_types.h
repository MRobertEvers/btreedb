#ifndef TOKEN_H_
#define TOKEN_H_

enum sql_token_e
{
	SQL_CREATE_INVAL = 0,
	SQL_CREATE_TABLE_KW,   // 1
	SQL_INSERT_KW,		   // 2
	SQL_VALUES_KW,		   // 3
	SQL_QUOTED_IDENTIFIER, // 4
	SQL_STRING_LITERAL,	   // 5
	SQL_INT_LITERAL,	   // 6
	SQL_IDENTIFIER,		   // 7
	SQL_COMMA,
	SQL_OPEN_PAREN,
	SQL_CLOSE_PAREN,
};

#endif