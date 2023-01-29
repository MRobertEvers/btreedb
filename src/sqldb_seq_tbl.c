#include "sqldb_seq_tbl.h"

#include "btree_factory.h"
#include "btree_node_debug.h"
#include "btree_op_update.h"
#include "ibtree.h"
#include "ibtree_layout_schema.h"
#include "ibtree_layout_schema_cmp.h"
#include "sql_ibtree.h"
#include "sql_utils.h"

#include <stdlib.h>

static struct SQLTable*
seq_tbl(void)
{
	struct SQLTable* tbl = sql_table_create();

	tbl->table_name = sql_string_create_from_cstring("seq");

	sql_table_emplace_column_c(tbl, "sequence_name", SQL_DT_STRING, true);
	sql_table_emplace_column_c(tbl, "value", SQL_DT_INT, false);

	return tbl;
}

struct SQLDBMetaTable
sqldb_seq_tbl_create(struct Pager* pager)
{
	struct SQLDBMetaTable tb = {
		.tree = btree_factory_create_ex(pager, BTREE_INDEX, 2),
		.table = seq_tbl()};
	return tb;
}

// Sequence table
// seq_name (string)
// val (int)
// static byte const* seq_tbl_name[] = {0x3, 0x00, 0x00, 0x00, 's', 'e', 'q'};

static struct IBTreeLayoutSchema
seq_tbl_layout(void)
{
	struct IBTreeLayoutSchema schema = {0};
	schema.nkey_definitions = 1;
	schema.key_definitions[0].size = 0;
	schema.key_definitions[0].type = IBTLSK_TYPE_VARSIZE;
	return schema;
}

static struct IBTLSCompareContext
tb_seq_ctx(void)
{
	struct IBTreeLayoutSchema schema = seq_tbl_layout();

	struct IBTLSCompareContext ctx = {0};
	ibtls_init_compare_context_from_schema(
		&ctx, &schema, PAYLOAD_COMPARE_TYPE_KEY);

	return ctx;
}

static enum sql_e
read_and_update_sequence(
	struct SQLDB* db, struct SQLString const* sequence_name, int* out_next)
{
	enum sql_e result = SQL_OK;
	struct SQLRecordSchema* record_schema = NULL;
	struct SQLRecord* record = NULL;
	byte* buf = NULL;
	byte* table_key = NULL;
	struct SQLSerializedRecord serred = {0};

	struct SQLTable* tbl = db->tb_sequences.table;

	struct IBTLSCompareContext ctx = tb_seq_ctx();

	table_key = (byte*)malloc(sequence_name->size + 4);
	sql_value_serialize_string(
		sequence_name, table_key, sequence_name->size + 4);

	struct OpUpdate update = {0};
	result = sqlbt_err(btree_op_update_acquire_index(
		db->tb_sequences.tree,
		&update,
		table_key,
		sequence_name->size + 4,
		&ctx));
	if( result != SQL_OK )
		goto end;

	enum btree_e btr = btree_op_update_prepare(&update);
	if( btr == BTREE_ERR_KEY_NOT_FOUND )
	{
		result = SQL_ERR_NOT_FOUND;
		goto end;
	}
	else if( btr == BTREE_OK )
	{
		buf = (byte*)malloc(update.data_size);
		result =
			sqlbt_err(btree_op_update_preview(&update, buf, update.data_size));
		if( result != SQL_OK )
			goto end;

		record_schema = sql_record_schema_create();
		record = sql_record_create();

		result = sql_ibtree_deserialize_record(
			tbl, record_schema, record, buf, update.data_size);
		if( result != SQL_OK )
			goto end;

		record->values[1].value.num.num += 1;
		*out_next = record->values[1].value.num.num;

		result = sql_ibtree_serialize_record_acquire(&serred, tbl, record);
		if( result != SQL_OK )
			goto end;

		result =
			sqlbt_err(btree_op_update_commit(&update, serred.buf, serred.size));
		if( result != SQL_OK )
			goto end;
	}
	else
	{
		result = sqlbt_err(btr);
		goto end;
	}

end:
	sql_ibtree_serialize_record_release(&serred);
	btree_op_update_release(&update);
	if( record_schema )
		sql_record_schema_destroy(record_schema);
	if( record )
		sql_record_destroy(record);

	if( buf )
		free(buf);
	if( table_key )
		free(table_key);

	return result;
}

static enum sql_e
create_sequence(
	struct SQLDB* db, struct SQLString const* sequence_name, int* out_next)
{
	enum sql_e result = SQL_OK;
	struct SQLRecordSchema* record_schema = sql_record_schema_create();
	struct SQLRecord* record = sql_record_create();
	struct SQLSerializedRecord serred = {0};
	struct SQLTable* tbl = db->tb_sequences.table;
	struct IBTLSCompareContext ctx = tb_seq_ctx();

	sql_record_schema_emplace_colname_c(record_schema, "sequence_name");
	sql_record_schema_emplace_colname_c(record_schema, "value");

	record->schema = record_schema;
	sql_record_emplace_literal(
		record, sequence_name, SQL_LITERALSTR_TYPE_STRING);

	sql_record_emplace_number(record, *out_next);

	sql_ibtree_serialize_record_acquire(&serred, tbl, record);

	dbg_print_buffer(serred.buf, serred.size);

	result = sqlbt_err(
		ibtree_insert_ex(db->tb_sequences.tree, serred.buf, serred.size, &ctx));
	if( result != SQL_OK )
		goto end;

end:
	sql_ibtree_serialize_record_release(&serred);
	if( record_schema )
		sql_record_schema_destroy(record_schema);
	if( record )
		sql_record_destroy(record);

	return result;
}

enum sql_e
sqldb_seq_tbl_next(
	struct SQLDB* db, struct SQLString const* sequence_name, int* out_next)
{
	enum sql_e result = SQL_OK;

	result = read_and_update_sequence(db, sequence_name, out_next);
	if( result == SQL_ERR_NOT_FOUND )
	{
		*out_next = 1;
		result = create_sequence(db, sequence_name, out_next);
	}

	return result;
}