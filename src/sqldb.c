#include "sqldb.h"

#include "btree.h"
#include "btree_defs.h"
#include "btree_node_debug.h"
#include "btree_op_update.h"
#include "ibtree_layout_schema.h"
#include "ibtree_layout_schema_ctx.h"
#include "sqldb_meta_tbls.h"

#include <stdlib.h>
#include <string.h>

enum sql_e
sqldb_create(struct SQLDB** out_sqldb, char const* filename)
{
	enum sql_e result = SQL_OK;
	struct SQLDB* db = (struct SQLDB*)malloc(sizeof(struct SQLDB));

	result = sqldb_meta_tables_create(&db->meta, filename);

	*out_sqldb = db;

	return result;
}

enum sql_e
sqldb_create_table(struct SQLDB* sqldb, struct SQLTable* table)
{
	u32 ser_size = sqldb_meta_serialize_table_def_size(table);

	byte* buffer = (byte*)malloc(ser_size);
	sqldb_meta_serialize_table_def(table, buffer, ser_size);

	dbg_print_buffer(buffer, ser_size);

	struct OpUpdate update = {0};
	enum btree_e res;
	// res =
	// 	btree_op_update_acquire_tbl(sqldb->meta.tb_sequences, &update, 1, NULL);

	// TODO: Lookup sequence number
	res = btree_insert(sqldb->meta.tb_table_definitions, 1, buffer, ser_size);
	// // Look up next sequence
	// struct OpUpdate update = {0};
	// struct SQLString* tables_table_name = "tables";
	// byte tables_table_name_key[] = {
	// 	0x6, 0x00, 0x00, 0x00, 't', 'a', 'b', 'l', 'e', 's'};
	// struct IBTreeLayoutSchema schema = {
	// 	.key_offset = 0,
	// };
	// schema.nkey_definitions = 1;
	// struct IBTLSKeyDef key_one_def = {.type = IBTLSK_TYPE_VARSIZE, .size =
	// 0}; schema.key_definitions[0] = key_one_def;

	// struct IBTLSCompareContext ctx = {
	// 	.schema = schema,
	// 	.curr_key = 0,
	// 	.initted = false,
	// 	.type = PAYLOAD_COMPARE_TYPE_KEY};

	// enum btree_e res;
	// res = btree_op_select_acquire_index(
	// 	sqldb->meta.tb_sequences,
	// 	&select,
	// 	tables_table_name_key,
	// 	sizeof(tables_table_name_key),
	// 	&ctx);

	// res = btree_op_select_prepare(&select);

	// res = btree_op_select_commit(&select);
}