#include "btree_op_select.h"

#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_node_reader.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

enum btree_e
btree_op_select_acquire_tbl(
	struct BTree* tree, struct OpSelection* op, u32 key, void* cmp_ctx)
{
	assert(tree->type == BTREE_TBL);

	memset(op, 0x00, sizeof(struct OpSelection));

	op->cursor = cursor_create_ex(tree, cmp_ctx);

	op->last_status = BTREE_OK;
	op->sm_key_buf = key;
	op->key = (byte*)&op->sm_key_buf;
	op->key_size = sizeof(key);
	op->data_size = 0;
	op->step = OP_SELECTION_STEP_INIT;

	return BTREE_OK;
}

enum btree_e
btree_op_select_acquire_index(
	struct BTree* tree,
	struct OpSelection* op,
	byte* key,
	u32 key_size,
	void* cmp_ctx)
{
	assert(tree->type == BTREE_INDEX);

	memset(op, 0x00, sizeof(struct OpSelection));

	op->cursor = cursor_create_ex(tree, cmp_ctx);

	op->last_status = BTREE_OK;
	op->sm_key_buf = 0;
	op->key = key;
	op->key_size = key_size;
	op->data_size = 0;
	op->step = OP_SELECTION_STEP_INIT;
	return BTREE_OK;
}

enum btree_e
btree_op_select_prepare(struct OpSelection* op)
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
		op->step = OP_SELECTION_STEP_NOT_FOUND;
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

	op->step = OP_SELECTION_STEP_PREPARED;
	op->last_status = result;
	return result;
}

enum btree_e
btree_op_select_commit(struct OpSelection* op, void* buffer, u32 buffer_size)
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

	op->step = OP_SELECTION_STEP_DONE;
	op->last_status = result;
	return result;
}

enum btree_e
btree_op_select_release(struct OpSelection* op)
{
	assert(op != NULL);
	memset(op, 0x00, sizeof(struct OpSelection));

	if( op->cursor )
		cursor_destroy(op->cursor);

	op->cursor = NULL;
	op->step = OP_SELECTION_STEP_DONE;
	return BTREE_OK;
}

u32
op_select_size(struct OpSelection* op)
{
	assert(
		op->step != OP_SELECTION_STEP_INIT &&
		op->step != OP_SELECTION_STEP_UNINIT &&
		op->step != OP_SELECTION_STEP_NOT_FOUND);
	return op->data_size;
}