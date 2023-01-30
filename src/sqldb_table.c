#include "sqldb_table.h"

#include "btree_factory.h"
#include "sql_utils.h"
#include "sqldb_seq_tbl.h"

#include <assert.h>
#include <stdlib.h>

static void
emplace_schema_column(
	struct SQLRecordSchema* schema, struct SQLTableColumn* col)
{
	schema->columns[schema->ncolumns] = sql_string_copy(col->name);
	schema->ncolumns += 1;
}

enum sql_e
sqldb_table_prepare_record(
	struct SQLDB* db,
	struct SQLTable* tab,
	struct SQLRecord* record,
	u32* out_row_id)
{
	enum sql_e result = SQL_OK;
	// TODO: This should be rowid key not pkey...
	int pkey_ind = sql_table_find_primary_key(tab);
	assert(pkey_ind != -1);

	int schema_pkey_ind =
		sql_record_schema_indexof(record->schema, tab->columns[pkey_ind].name);

	int seq = 0;
	if( schema_pkey_ind == -1 )
	{
		// If pkey is missing check if it's an INT and autoincrement.
		// Auto increment.
		result = sqldb_seq_tbl_next(db, tab->table_name, &seq);
		if( result != SQL_OK )
			goto end;

		sql_record_emplace_number(record, seq);
		emplace_schema_column(record->schema, &tab->columns[pkey_ind]);
	}
	else
	{
		seq = record->values[schema_pkey_ind].value.num.num;
		result = sqldb_seq_tbl_set(db, tab->table_name, seq);
		if( result != SQL_OK )
			goto end;
	}

	*out_row_id = seq;

end:
	return result;
}

enum sql_e
sqldb_table_btree_create(
	struct SQLDB* db, struct SQLTable* tab, struct BTree** out_tree)
{
	enum sql_e result = SQL_OK;
	assert(tab->meta.root_page != 0);

	*out_tree =
		btree_factory_create_ex(db->pager, BTREE_TBL, tab->meta.root_page);

end:
	return result;
}

enum sql_e
sqldb_table_btree_destroy(
	struct SQLDB* db, struct SQLTable* tab, struct BTree* out_tree)
{
	enum sql_e result = SQL_OK;
	free(out_tree);
	return result;
}