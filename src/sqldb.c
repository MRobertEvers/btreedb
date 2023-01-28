#include "sqldb.h"

#include "btree_factory.h"

#include <string.h>

enum sql_e
sqldb_create(struct SQLDB** out_sqldb, char const* filename)
{
	struct SQLDB* db = (struct SQLDB*)malloc(sizeof(struct SQLDB));

	struct Pager* pager = btree_factory_pager_create(filename);

	db->tb_table_definitions = btree_factory_create_ex(pager, BTREE_TBL, 1);
	db->tb_sequences = btree_factory_create_ex(pager, BTREE_INDEX, 2);

	*out_sqldb = db;
}