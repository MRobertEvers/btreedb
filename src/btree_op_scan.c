#include "btree_op_scan.h"

#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_node_reader.h"
#include "btree_node_writer.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

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
	struct NodeView nv = {0};

	if( op->step != OP_SCAN_STEP_ITERATING )
		goto end;

	result = noderc_acquire(cursor_rcer(op->cursor), &nv);
	if( result != BTREE_OK )
		goto end;

	result = cursor_iter_next(op->cursor);
	if( result != BTREE_OK )
		goto end;

	result = cursor_read_current(op->cursor, &nv);
	if( result != BTREE_OK )
		goto end;

	u32 ind = op->cursor->current_key_index.index;
	result = btree_node_payload_size_at(
		cursor_tree(op->cursor), nv_node(&nv), ind, &op->data_size);
	if( result != BTREE_OK )
		goto end;

end:
	noderc_release(cursor_rcer(op->cursor), &nv);
	if( result == BTREE_ERR_ITER_DONE )
	{
		op->step = OP_SCAN_STEP_DONE;
		result = BTREE_OK;
	}
	op->last_status = result;
	return result;
}

enum btree_e
btree_op_scan_update(struct OpScan* op, void* payload, u32 payload_size)
{
	enum btree_e result = BTREE_OK;
	struct NodeView nv = {0};
	struct Cursor* cursor = op->cursor;

	result = noderc_acquire(cursor_rcer(cursor), &nv);
	if( result != BTREE_OK )
		goto end;

	result = cursor_read_current(cursor, &nv);
	if( result != BTREE_OK )
		goto end;

	// TODO: Check that we're looking at a record.
	result =
		btree_node_remove(nv_node(&nv), cursor_curr_ind(cursor), NULL, NULL, 0);
	if( result != BTREE_OK )
		goto end;

	u32 key = node_key_at(nv_node(&nv), cursor_curr_ind(cursor)->index);
	struct InsertionIndex insert_index = {
		.mode = KLIM_INDEX, .index = cursor_curr_ind(cursor)->index};
	result = btree_node_write_at(
		nv_node(&nv),
		cursor_pager(cursor),
		&insert_index,
		key,
		payload,
		payload_size);
	if( result != BTREE_OK )
		goto end;

end:
	noderc_release(cursor_rcer(cursor), &nv);

	op->last_status = result;
	return result;
}

bool
btree_op_scan_done(struct OpScan* op)
{
	return op->step == OP_SCAN_STEP_DONE;
}

enum btree_e
btree_op_scan_release(struct OpScan* op)
{
	assert(op != NULL);

	if( op->cursor )
		cursor_destroy(op->cursor);

	memset(op, 0x00, sizeof(struct OpScan));
	op->cursor = NULL;
	op->step = OP_SCAN_STEP_DONE;
	return BTREE_OK;
}