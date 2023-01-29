#include "btree_op_scan.h"

#include "btree_cursor.h"

#include <assert.h>

enum btree_e
btree_op_scan_acquire(struct BTree* tree, struct OpScan* op)
{
	memset(op, 0x00, sizeof(struct OpScan));

	op->cursor = cursor_create(tree);

	op->last_status = BTREE_OK;
	op->data_size = 0;
	op->step = OP_SCAN_STEP_INIT;
	op->initialized = true;

	return BTREE_OK;
}

enum btree_e
btree_op_scan_prepare(struct OpScan* op)
{
	assert(op->step == OP_SCAN_STEP_INIT);
	enum btree_e result = BTREE_OK;
	char found;
	struct NodeView nv = {0};
	struct Cursor* cursor = op->cursor;

	result = noderc_acquire(cursor_rcer(cursor), &nv);
	if( result != BTREE_OK )
		goto end;

	result = cursor_iter_begin(cursor);
	if( result != BTREE_OK )
		goto end;

	result = cursor_read_current(cursor, &nv);
	if( result != BTREE_OK )
		goto end;

	u32 ind = cursor->current_key_index.index;
	result = btree_node_payload_size_at(
		cursor_tree(cursor), nv_node(&nv), ind, &op->data_size);
	if( result != BTREE_OK )
		goto end;

end:
	noderc_release(cursor_rcer(cursor), &nv);

	if( result == BTREE_ERR_ITER_DONE )
	{
		op->step = OP_SCAN_STEP_DONE;
		result = BTREE_OK;
	}
	else if( result == BTREE_OK )
	{
		op->step = OP_SCAN_STEP_ITERATING;
	}

	op->last_status = result;
	return result;
}

enum btree_e
btree_op_scan_current(struct OpScan* op, void* buffer, u32 buffer_size)
{
	enum btree_e result = BTREE_OK;
	struct NodeView nv = {0};
	struct Cursor* cursor = op->cursor;

	if( op->step != OP_SCAN_STEP_ITERATING )
		goto end;

	result = noderc_acquire(cursor_rcer(cursor), &nv);
	if( result != BTREE_OK )
		goto end;

	result = cursor_read_current(cursor, &nv);
	if( result != BTREE_OK )
		goto end;

	u32 ind = cursor->current_key_index.index;
	result = btree_node_read_at(
		cursor_tree(cursor), nv_node(&nv), ind, buffer, buffer_size);
	if( result != BTREE_OK )
		goto end;

end:
	noderc_release(cursor_rcer(cursor), &nv);

	op->last_status = result;
	return result;
}

enum btree_e
btree_op_scan_next(struct OpScan* op)
{
	enum btree_e result = BTREE_OK;
	if( op->step != OP_SCAN_STEP_ITERATING )
		goto end;

	result = cursor_iter_next(op->cursor);
	if( result != BTREE_OK )
		goto end;

end:
	if( result == BTREE_ERR_ITER_DONE )
	{
		op->step = OP_SCAN_STEP_DONE;
		result = BTREE_OK;
	}
	op->last_status = result;
	return result;
}

enum btree_e
btree_op_scan_done(struct OpScan* op)
{}

enum btree_e
btree_op_scan_release(struct OpScan* op)
{}