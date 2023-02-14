#ifndef TOKEN_H_
#define TOKEN_H_

enum sql_token_e
{
	SQL_CREATE_INVAL = 0,
	SQL_CREATE_TABLE_KW,   // 1
	SQL_INSERT_KW,		   // 2
	SQL_VALUES_KW,		   // 3
	SQL_UPDATE_KW,		   // 4
	SQL_DELETE_KW,		   // 4
	SQL_WHERE_KW,		   // 5
	SQL_FROM_KW,		   // 6
	SQL_SET_KW,			   // 7
	SQL_STAR_KW,		   // 8
	SQL_EQUAL_KW,		   // 9
	SQL_SELECT_KW,		   // 20
	SQL_QUOTED_IDENTIFIER, // 6
	SQL_STRING_LITERAL,	   // 7
	SQL_INT_LITERAL,	   // 8
	SQL_IDENTIFIER,		   // 9
	SQL_COMMA,			   // 10
	SQL_OPEN_PAREN,
	SQL_CLOSE_PAREN,
};

#endif