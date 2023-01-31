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
verify_record(struct SQLDB* db, struct SQLTable* tab, struct SQLRecord* record)
{
	// Only column we don't need is the row id. "id".
	bool missing_column = false;
	for( int i = 0; i < tab->ncolumns && !missing_column; i++ )
	{
		struct SQLTableColumn* col = &tab->columns[i];
		if( col->is_primary_key ) // TODO: This should be row id.
			continue;

		// TODO: Bad runtime
		int i = sql_record_schema_indexof(record->schema, col->name);
		if( i == -1 )
			missing_column = true;
	}

	return missing_column ? SQL_ERR_BAD_RECORD : SQL_OK;
}

enum sql_e
sqldb_table_prepare_record(
	struct SQLDB* db,
	struct SQLTable* tab,
	struct SQLRecord* record,
	u32* out_row_id)
{
	enum sql_e result = SQL_OK;

	result = verify_record(db, tab, record);
	if( result != SQL_OK )
		goto end;

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
sqldb_table_btree_acquire(
	struct SQLDB* db, struct SQLTable* tab, struct BTreeView* out_tree)
{
	enum sql_e result = SQL_OK;
	assert(tab->meta.root_page != 0);

	result = btree_factory_view_acquire(
		out_tree, db->pager, BTREE_TBL, tab->meta.root_page);

	return result;
}

enum sql_e
sqldb_table_btree_release(
	struct SQLDB* db, struct SQLTable* tab, struct BTreeView* out_tree)
{
	enum sql_e result = SQL_OK;
	btree_factory_view_release(out_tree);
	return result;
}