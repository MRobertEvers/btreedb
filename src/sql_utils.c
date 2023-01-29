#include "sql_utils.h"

enum sql_e
sqlbt_err(enum btree_e err)
{
	switch( err )
	{
	case BTREE_OK:
		return SQL_OK;

	default:
		return SQL_ERR_UNKNOWN;
	}
}