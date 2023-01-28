#include "sqldb_seq_tbl.h"

#include "btree_factory.h"
#include "btree_node_debug.h"
#include "btree_op_update.h"
#include "ibtree.h"
#include "ibtree_layout_schema.h"
#include "ibtree_layout_schema_cmp.h"
#include "sql_ibtree.h"

#include <stdlib.h>

struct BTree*
sqldb_seq_tbl_create(struct Pager* pager)
{
	return btree_factory_create_ex(pager, BTREE_INDEX, 2);
}

// Sequence table
// seq_name (string)
// val (int)
// static byte const* seq_tbl_name[] = {0x3, 0x00, 0x00, 0x00, 's', 'e', 'q'};

// TODO: Const
static struct IBTreeLayoutSchema
seq_tbl_layout(void)
{
	struct IBTreeLayoutSchema schema = {0};
	schema.nkey_definitions = 1;
	schema.key_definitions[0].size = 0;
	schema.key_definitions[0].type = IBTLSK_TYPE_VARSIZE;
	return schema;
}

// TODO: Const
static struct SQLTable
seq_tbl(void)
{
	struct SQLTable tbl = {0};

	tbl.table_name = sql_string_create_from_cstring("seq");

	sql_table_emplace_column_c(&tbl, "sequence_name", SQL_DT_STRING, true);
	sql_table_emplace_column_c(&tbl, "value", SQL_DT_INT, false);

	return tbl;
}

enum sql_e
sqldb_seq_tbl_next(
	struct SQLDB* db, struct SQLString const* sequence_name, int* out_next)
{
	enum sql_e result = SQL_OK;
	enum btree_e btresult = BTREE_OK;

	// TODO: This leaks!!!
	struct SQLTable tbl = seq_tbl();

	struct IBTreeLayoutSchema schema = seq_tbl_layout();
	struct IBTLSCompareContext ctx = {0};
	ibtls_init_compare_context_from_schema(
		&ctx, &schema, PAYLOAD_COMPARE_TYPE_KEY);

	byte* table_key = (byte*)malloc(sequence_name->size + 4);
	sql_value_serialize_string(
		sequence_name, table_key, sequence_name->size + 4);
	struct OpUpdate update = {0};
	btresult = btree_op_update_acquire_index(
		db->meta.tb_sequences,
		&update,
		table_key,
		sequence_name->size + 4,
		&ctx);

	btresult = btree_op_update_prepare(&update);
	if( btresult != BTREE_ERR_KEY_NOT_FOUND )
	{
		byte* buf = (byte*)malloc(update.data_size);
		btresult = btree_op_update_preview(&update, buf, update.data_size);

		// TODO: This leaks!
		struct SQLRecordSchema record_schema = {0};

		// TODO: This leaks!!
		struct SQLRecord record = {0};
		sql_ibtree_deserialize_record(
			&tbl, &record_schema, &record, buf, update.data_size);

		*out_next = record.values[1].value.num.num;
		record.values[1].value.num.num += 1;

		free(buf);

		// TODO: This leaks!
		struct SQLSerializedRecord serred = {0};
		sql_ibtree_serialize_record(&tbl, &record, &serred);

		btree_op_update_commit(&update, serred.buf, serred.size);
	}
	else
	{
		*out_next = 1;

		// TODO: This leaks!

		struct SQLRecordSchema record_schema = {0};
		record_schema.columns[0] =
			sql_string_create_from_cstring("sequence_name");
		record_schema.columns[1] = sql_string_create_from_cstring("value");
		record_schema.ncolumns = 2;

		// TODO: This leaks
		struct SQLRecord record = {0};
		record.schema = &record_schema;
		sql_record_emplace_literal(
			&record, sequence_name, SQL_LITERALSTR_TYPE_STRING);
		record.values[1].type = SQL_VALUE_TYPE_INT;
		record.values[1].value.num.num = *out_next;
		record.nvalues = 2;

		// TODO: This leaks
		struct SQLSerializedRecord serred = {0};
		sql_ibtree_serialize_record(&tbl, &record, &serred);

		dbg_print_buffer(serred.buf, serred.size);

		ibtree_insert_ex(db->meta.tb_sequences, serred.buf, serred.size, &ctx);
	}

	btree_op_update_release(&update);
	free(table_key);

	return SQL_OK;
}