#include "sqldb_meta_tbls.h"

#include "btree_factory.h"
#include "serialization.h"
#include "sql_value.h"
#include "sqldb_seq_tbl.h"

#include <stdlib.h>

enum sql_e
sqldb_meta_tables_create(struct SQLDB* sqldb, struct Pager* pager)
{
	sqldb->tb_tables = (struct SQLDBMetaTable){
		.table = NULL,
		.tree = btree_factory_create_with_pager(pager, BTREE_TBL, 1)};
	sqldb->tb_sequences = sqldb_seq_tbl_create(pager);

	return SQL_OK;
}
