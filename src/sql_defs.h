#ifndef SQL_DEFS_H_
#define SQL_DEFS_H_

enum sql_e
{
	SQL_INVALID = 0,
	SQL_OK,
	SQL_ERR_INVALID_SQL,
	SQL_ERR_RECORD_MISSING_PKEY,
	SQL_ERR_BAD_PARSE,
	SQL_ERR_BAD_RECORD,
	SQL_ERR_UNKNOWN,
	SQL_ERR_TABLE_DNE,
	SQL_ERR_NOT_FOUND,
	SQL_ERR_SCAN_DONE,
};

#endif