#include "btree_op_update.h"

#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_node_reader.h"
#include "btree_node_writer.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

enum btree_e
btree_op_update_acquire_tbl(
	struct BTree* tree, struct OpUpdate* op, u32 key, void* cmp_ctx)
{
	assert(tree->type == BTREE_TBL);

	memset(op, 0x00, sizeof(struct OpUpdate));

	op->cursor = cursor_create_ex(tree, cmp_ctx);

	op->last_status = BTREE_OK;
	op->sm_key_buf = key;
	op->key = (byte*)&op->sm_key_buf;
	op->key_size = sizeof(key);
	op->data_size = 0;
	op->step = OP_UPDATE_STEP_INIT;

	return BTREE_OK;
}

enum btree_e
btree_op_update_acquire_index(
	struct BTree* tree,
	struct OpUpdate* op,
	byte* key,
	u32 key_size,
	void* cmp_ctx)
{
	assert(tree->type == BTREE_INDEX);

	memset(op, 0x00, sizeof(struct OpUpdate));

	op->cursor = cursor_create_ex(tree, cmp_ctx);

	op->last_status = BTREE_OK;
	op->sm_key_buf = 0;
	op->key = key;
	op->key_size = key_size;
	op->data_size = 0;
	op->step = OP_UPDATE_STEP_INIT;

	return BTREE_OK;
}

enum btree_e
btree_op_update_prepare(struct OpUpdate* op)
{
	enum btree_e result = BTREE_OK;
	char found;
	struct NodeView nv = {0};
	struct Cursor* cursor = op->cursor;

	result = noderc_acquire(cursor_rcer(cursor), &nv);
	if( result != BTREE_OK )
		goto end;

	result = cursor_traverse_to_ex(cursor, op->key, op->key_size, &found);
	if( result != BTREE_OK )
		goto end;

	if( !found )
	{
		result = BTREE_ERR_KEY_NOT_FOUND;
		op->step = OP_UPDATE_STEP_NOT_FOUND;
		goto end;
	}

	result = cursor_read_current(cursor, &nv);
	if( result != BTREE_OK )
		goto end;

	assert(cursor->current_key_index.index != node_num_keys(nv_node(&nv)));

	u32 ind = cursor->current_key_index.index;
	result = btree_node_payload_size_at(
		cursor_tree(cursor), nv_node(&nv), ind, &op->data_size);
	if( result != BTREE_OK )
		goto end;

end:
	noderc_release(cursor_rcer(cursor), &nv);

	op->step = OP_UPDATE_STEP_PREPARED;
	op->last_status = result;
	return result;
}

enum btree_e
btree_op_update_preview(struct OpUpdate* op, void* buffer, u32 buffer_size)
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

	assert(cursor->current_key_index.index != node_num_keys(nv_node(&nv)));

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
btree_op_update_commit(struct OpUpdate* op, byte* payload, u32 payload_size)
{
	// TODO: Validate the the payload key maintains key value
	assert(op->step == OP_UPDATE_STEP_PREPARED);

	enum btree_e result = BTREE_OK;
	struct NodeView nv = {0};
	struct Cursor* cursor = op->cursor;

	result = noderc_acquire(cursor_rcer(cursor), &nv);
	if( result != BTREE_OK )
		goto end;

	result = cursor_read_current(cursor, &nv);
	if( result != BTREE_OK )
		goto end;

	assert(cursor->current_key_index.index != node_num_keys(nv_node(&nv)));
	assert(cursor->current_key_index.mode == KLIM_INDEX);

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

	op->step = OP_UPDATE_STEP_DONE;
	op->last_status = result;
	return result;
}

enum btree_e
btree_op_update_release(struct OpUpdate* op)
{
	assert(op != NULL);
	memset(op, 0x00, sizeof(struct OpUpdate));

	if( op->cursor )
		cursor_destroy(op->cursor);

	op->cursor = NULL;
	op->step = OP_UPDATE_STEP_DONE;
	return BTREE_OK;
}

u32
op_update_size(struct OpUpdate* op)
{
	assert(
		op->step != OP_UPDATE_STEP_INIT && op->step != OP_UPDATE_STEP_UNINIT &&
		op->step != OP_UPDATE_STEP_NOT_FOUND);
	return op->data_size;
}