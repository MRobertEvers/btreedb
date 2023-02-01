#include "sqldb_scan.h"

#include "btree_defs.h"
#include "btree_factory.h"
#include "btree_op_scan.h"
#include "sql_ibtree.h"
#include "sql_utils.h"
#include "sqldb_scanbuffer.h"
#include "sqldb_table.h"
#include "sqldb_table_tbl.h"

#include <stdlib.h>
#include <string.h>

enum scan_e
{
	SE_INIT,
	SE_BEGIN,
	SE_SCAN,
	SE_END,
	SE_FINALIZED,
};

struct ScanState
{
	enum scan_e step;

	struct OpScan op;
	struct BTreeView tv;
	struct SQLDBScanBuffer buffer;
	struct SQLTable* table;
	struct SQLRecordSchema* record_schema;
	struct SQLRecord* record;
};

enum sql_e
sqldb_scan_acquire(
	struct SQLDB* db, struct SQLString* name, struct SQLDBScan* scan)
{
	scan->db = db;
	scan->table_name = sql_string_copy(name);

	struct ScanState* fsm = (struct ScanState*)malloc(sizeof(struct ScanState));
	memset(fsm, 0x00, sizeof(*fsm));
	fsm->step = SE_INIT;

	scan->internal = fsm;

	return SQL_OK;
}

enum sql_e
sqldb_scan_next(struct SQLDBScan* scan)
{
	enum sql_e result = SQL_OK;
	struct ScanState* fsm = (struct ScanState*)scan->internal;
	switch( fsm->step )
	{
	case SE_INIT:
		goto init;
	case SE_BEGIN:
		goto begin;
	case SE_SCAN:
		goto scan;
	case SE_END:
		goto end;
	case SE_FINALIZED:
		goto await;
	}

init:
	fsm->table = sql_table_create();

	result = sqldb_table_tbl_find(scan->db, scan->table_name, fsm->table);
	if( result != SQL_OK )
		goto end;

	result = sqldb_table_btree_acquire(scan->db, fsm->table, &fsm->tv);
	if( result != SQL_OK )
		goto end;

	result = sqlbt_err(btree_op_scan_acquire(fsm->tv.tree, &fsm->op));
	if( result != SQL_OK )
		goto end;

	result = sqlbt_err(btree_op_scan_prepare(&fsm->op));
	if( result != SQL_OK )
		goto end;

	fsm->step = SE_BEGIN;
	// TODO: Should we await before first iteration?
	// goto await;
begin:

	while( !btree_op_scan_done(&fsm->op) )
	{
		sqldb_scanbuffer_resize(&fsm->buffer, fsm->op.data_size);

		result = sqlbt_err(btree_op_scan_current(
			&fsm->op, fsm->buffer.buffer, fsm->buffer.size));
		if( result != SQL_OK )
			goto end;

		fsm->record_schema = sql_record_schema_create();
		fsm->record = sql_record_create();

		result = sql_ibtree_deserialize_record(
			fsm->table,
			fsm->record_schema,
			fsm->record,
			fsm->buffer.buffer,
			fsm->buffer.size);
		if( result != SQL_OK )
			goto end;

		scan->current_record = fsm->record;
		fsm->step = SE_SCAN;
		goto await;
	scan:

		sql_record_schema_destroy(fsm->record_schema);
		fsm->record_schema = NULL;
		sql_record_destroy(fsm->record);
		fsm->record = NULL;
		scan->current_record = NULL;

		result = sqlbt_err(btree_op_scan_next(&fsm->op));
		if( result != SQL_OK )
			goto end;
	}

	fsm->step = SE_END;
end:

	sqldb_scanbuffer_free(&fsm->buffer);
	btree_op_scan_release(&fsm->op);
	sqldb_table_btree_release(scan->db, fsm->table, &fsm->tv);
	sql_record_schema_destroy(fsm->record_schema);
	sql_record_destroy(fsm->record);
	sql_table_destroy(fsm->table);

	fsm->step = SE_FINALIZED;

await:
	return result;
}

enum sql_e
sqldb_scan_update(struct SQLDBScan* scan, struct SQLRecord* record)
{
	enum sql_e result = SQL_OK;
	struct ScanState* fsm = (struct ScanState*)scan->internal;
	struct SQLSerializedRecord serred = {0};

	u32 newsize = sql_ibtree_serialize_record_size(record);
	sqldb_scanbuffer_resize(&fsm->buffer, newsize);

	result = sql_ibtree_serialize_record_acquire(&serred, fsm->table, record);
	if( result != SQL_OK )
		goto end;

	result = sqlbt_err(btree_op_scan_update(&fsm->op, serred.buf, serred.size));
	if( result != SQL_OK )
		goto end;

end:
	sql_ibtree_serialize_record_release(&serred);
	return result;
}

bool
sqldb_scan_done(struct SQLDBScan* scan)
{
	struct ScanState* fsm = (struct ScanState*)scan->internal;

	return fsm->step == SE_FINALIZED;
}

void
sqldb_scan_release(struct SQLDBScan* scan)
{
	struct ScanState* fsm = (struct ScanState*)scan->internal;

	if( fsm->step != SE_FINALIZED )
	{
		fsm->step = SE_END;
		sqldb_scan_next(scan);
	}

	sql_string_destroy(scan->table_name);
	free(scan->internal);
}

struct SQLRecord*
sqldb_scan_record(struct SQLDBScan* scan)
{
	struct ScanState* fsm = (struct ScanState*)scan->internal;

	if( fsm->step == SE_SCAN )
		return fsm->record;
	else
		return NULL;
}