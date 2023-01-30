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
	default:
		return SQL_ERR_UNKNOWN;
	}
}