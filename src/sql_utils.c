#include "sql_utils.h"

enum sql_e
sqlbt_err(enum btree_e err)
{
	switch( err )
	{
	case BTREE_OK:
		return SQL_OK;
	case BTREE_ERR_ITER_DONE:
		return SQL_ERR_SCAN_DONE;
	case BTREE_ERR_KEY_NOT_FOUND:
		return SQL_ERR_NOT_FOUND;
	default:
		return SQL_ERR_UNKNOWN;
	}
}

enum sql_e
sqlpager_err(enum pager_e err)
{
	switch( err )
	{
	case PAGER_OK:
		return SQL_OK;
	default:
		return SQL_ERR_UNKNOWN;
	}
}