#ifndef SQL_DEFS_H_
#define SQL_DEFS_H_

enum sql_e
{
	SQL_INVALID = 0,
	SQL_OK,
	SQL_ERR_INVALID_SQL,
	SQL_ERR_RECORD_MISSING_PKEY,
	SQL_ERR_BAD_PARSE,
};

#endif