#include "ibtree.h"

#include "btree.h"
#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_node_debug.h"
#include "btree_node_reader.h"
#include "btree_node_writer.h"
#include "btree_utils.h"
#include "ibtree_alg.h"
#include "noderc.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
min(int left, int right)
{
	return left < right ? left : right;
}

enum btree_e
ibtree_init(
	struct BTree* tree,
	struct Pager* pager,
	struct BTreeNodeRC* rcer,
	u32 root_page_id,
	btree_compare_fn compare,
	btree_compare_reset_fn reset_compare)
{
	enum btree_e result = btree_init(tree, pager, rcer, root_page_id);
	tree->type = BTREE_INDEX;
	tree->compare = compare;
	tree->reset_compare = reset_compare;
	tree->keyof = &ibtree_keyof;
	return result;
}

int
ibtree_compare(
	void* compare_context,
	void* left,
	u32 left_size,
	u32 cmp_total_size,
	void* right,
	u32 right_size,
	u32 bytes_compared,
	u32* out_bytes_compared,
	u32* out_key_size_remaining)
{
	assert(right_size > bytes_compared);
	if( bytes_compared >= right_size )
	{
		*out_bytes_compared = 0;
		*out_key_size_remaining = 0;
		return 0;
	}

	// The inline payload of a cell on disk for an IBTree always starts with the
	// left child page id.
	byte* left_buffer = (byte*)left;
	byte* right_buffer = (byte*)right;
	right_buffer += bytes_compared;

	*out_bytes_compared = min(left_size, right_size - bytes_compared);
	u32 new_total_bytes_compared = bytes_compared + *out_bytes_compared;
	*out_key_size_remaining = right_size - new_total_bytes_compared;
	int cmp = memcmp(left_buffer, right_buffer, *out_bytes_compared);
	if( cmp == 0 )
	{
		if( *out_key_size_remaining == 0 &&
			new_total_bytes_compared < cmp_total_size )
		{
			// Key is shorter than compare. Therefor Key < Cmp;
			return 1;
		}
		else if(
			*out_key_size_remaining != 0 &&
			new_total_bytes_compared == cmp_total_size )
		{
			// Key is longer than compare. Therefore Cmp < Key
			return -1;
		}

		return cmp;
	}
	else
	{
		return cmp < 0 ? -1 : 1;
	}
}

void
ibtree_compare_reset(void* compare_context)
{}

// TODO: Return error
// TODO: Might have to read onto overflow page...
byte*
ibtree_keyof(
	void* compare_context,
	struct BTreeNode* node,
	u32 index,
	u32* out_size,
	u32* out_total_size,
	u32* out_follow_page)
{
	struct BTreePageKey* key = &node->keys[index];
	bool is_overflow =
		btree_pkey_is_cell_type(key->flags, PKEY_FLAG_CELL_TYPE_OVERFLOW);

	byte* cell_data = btu_get_cell_buffer(node, index);
	u32 cell_data_size = btu_get_cell_buffer_size(node, index);

	byte* payload = NULL;
	*out_size = 0;
	*out_total_size = 0;
	*out_follow_page = 0;
	if( is_overflow )
	{
		// Overflow page -> Overflow page.
		struct BTreeCellOverflow read_cell = {0};
		struct BufferReader reader = {0};
		btree_cell_init_overflow_reader(&reader, cell_data, cell_data_size);

		btree_cell_read_overflow_ex(&reader, &read_cell, NULL, 0);

		payload = (byte*)read_cell.inline_payload;
		*out_size =
			btree_cell_overflow_calc_inline_payload_size(read_cell.inline_size);
		*out_total_size = read_cell.total_size;
		*out_follow_page = read_cell.overflow_page_id;
	}
	else
	{
		struct BTreeCellInline cell = {0};
		btree_cell_read_inline(cell_data, cell_data_size, &cell, NULL, 0, NULL);

		payload = (byte*)cell.payload;
		*out_size = cell.inline_size;
		*out_total_size = cell.inline_size;
	}

	return payload;
}

struct InsertionIndex
from_cli(struct ChildListIndex* cli)
{
	struct InsertionIndex result;
	result.index = cli->index;

	if( cli->mode == KLIM_RIGHT_CHILD )
	{
		result.mode = KLIM_END;
	}
	else
	{
		result.mode = cli->mode;
	}

	return result;
}

enum btree_e
ibtree_insert(struct BTree* tree, void* payload, int payload_size)
{
	return ibtree_insert_ex(tree, payload, payload_size, NULL);
}

enum btree_e
ibtree_insert_ex(
	struct BTree* tree, void* payload, int payload_size, void* cmp_ctx)
{
	assert(tree->compare != NULL);
	enum btree_e result = BTREE_OK;
	char found;
	struct Cursor* cursor = cursor_create_ex(tree, cmp_ctx);

	result = cursor_traverse_to_ex(cursor, payload, payload_size, &found);
	if( result != BTREE_OK )
		goto end;

	struct ibta_insert_at insert_at;
	ibta_insert_at_init(&insert_at, payload, payload_size);
	result = ibta_insert_at(cursor, &insert_at);
	if( result != BTREE_OK )
		goto end;

end:
	cursor_destroy(cursor);
	return result;
}

static enum btree_e
delete_single(struct Cursor* cursor, void* key, int key_size)
{
	enum btree_e result = BTREE_OK;
	char found = 0;
	struct NodeView nv = {0};
	struct NodeView holding_nv = {0};
	bool underflow = false;
	struct CursorBreadcrumb crumb = {0};

	result =
		noderc_acquire_load_n(cursor_rcer(cursor), 2, &nv, 0, &holding_nv, 0);
	if( result != BTREE_OK )
		goto end;

	result = cursor_traverse_to_ex(cursor, key, key_size, &found);
	if( result != BTREE_OK || !found )
		goto end;

	result = cursor_peek(cursor, &crumb);
	if( result != BTREE_OK )
		goto end;

	result = noderc_reinit_read(cursor_rcer(cursor), &nv, crumb.page_id);
	if( result != BTREE_OK )
		goto end;

	result = ibtree_node_remove(
		nv_node(&nv), &crumb.key_index, nv_node(&holding_nv));
	if( result != BTREE_OK )
		goto end;

	result = noderc_persist_n(cursor_rcer(cursor), 1, &nv);
	if( result != BTREE_OK )
		goto end;

	// The cell is removed. If it wasn't a leaf node find the largest cell
	// in the left subtree and promote it to the same location. If it was a
	// leaf node, do nothing.
	if( !node_is_leaf(nv_node(&nv)) )
	{
		u32 orphaned_left_child = node_key_at(nv_node(&holding_nv), 0);
		struct CursorBreadcrumb replacement_crumb = {
			.key_index =
				{
					.mode = KLIM_INDEX,
					.index = 0,
				},
			.page_id = orphaned_left_child};

		// Push left child to cursor then find largest in tree.
		result = cursor_push_crumb(cursor, &replacement_crumb);
		if( result != BTREE_OK )
			goto end;

		result = cursor_traverse_largest(cursor);
		if( result != BTREE_OK )
			goto end;

		result = cursor_peek(cursor, &replacement_crumb);
		if( result != BTREE_OK )
			goto end;

		result = noderc_reinit_read(
			cursor_rcer(cursor), &holding_nv, replacement_crumb.page_id);
		if( result != BTREE_OK )
			goto end;

		struct InsertionIndex insert = {
			.index = crumb.key_index.index,
			.mode = crumb.key_index.mode == KLIM_RIGHT_CHILD ? KLIM_END
															 : KLIM_INDEX};
		struct ChildListIndex remove_index = {
			.mode = KLIM_INDEX,
			.index = node_num_keys(nv_node(&holding_nv)) - 1};

		result = btree_node_move_cell_ex_to(
			nv_node(&holding_nv),
			nv_node(&nv),
			remove_index.index,
			&insert,
			orphaned_left_child,
			cursor_pager(cursor));
		if( result != BTREE_OK )
			goto end;

		result = ibtree_node_remove(nv_node(&holding_nv), &remove_index, NULL);
		if( result != BTREE_OK )
			goto end;

		result = noderc_persist_n(cursor_rcer(cursor), 2, &nv, &holding_nv);
		if( result != BTREE_OK )
			goto end;

		underflow = node_num_keys(nv_node(&holding_nv)) <
					btree_underflow_lim(cursor->tree);
	}
	else
	{
		// Do nothing, detect underflow.
		underflow =
			node_num_keys(nv_node(&nv)) < btree_underflow_lim(cursor->tree);
	}

end:
	noderc_release_n(cursor_rcer(cursor), 2, &nv, &holding_nv);

	if( underflow && result == BTREE_OK )
		result = BTREE_ERR_UNDERFLOW;
	return result;
}

enum btree_e
ibtree_delete(struct BTree* tree, void* key, int key_size)
{
	return ibtree_delete_ex(tree, key, key_size, NULL);
}

enum btree_e
ibtree_delete_ex(struct BTree* tree, void* key, int key_size, void* cmp_ctx)
{
	enum btree_e result = BTREE_OK;
	struct Cursor* cursor = cursor_create_ex(tree, cmp_ctx);

	result = delete_single(cursor, key, key_size);
	// If the leaf node underflows, then we need to rebalance.

	// TODO: Assumes min# elements is < 1/2 of the max # elements.
	if( result == BTREE_ERR_UNDERFLOW )
	{
		result = ibta_rebalance(cursor);
		if( result != BTREE_OK )
			goto end;
	}

end:
	if( cursor )
		cursor_destroy(cursor);

	return result;
}

enum btree_e
ibtree_select_ex(
	struct BTree* tree,
	void* cmp_ctx,
	void* key,
	int key_size,
	void* buffer,
	u32 buffer_size)
{
	enum btree_e result = BTREE_OK;
	char found;
	struct NodeView nv = {0};
	struct Cursor* cursor = cursor_create_ex(tree, cmp_ctx);

	result = noderc_acquire(cursor_rcer(cursor), &nv);
	if( result != BTREE_OK )
		goto end;

	result = cursor_traverse_to_ex(cursor, key, key_size, &found);
	if( result != BTREE_OK )
		goto end;

	if( !found )
	{
		result = BTREE_ERR_KEY_NOT_FOUND;
		goto end;
	}

	result = cursor_read_current(cursor, &nv);
	if( result != BTREE_OK )
		goto end;

	assert(cursor->current_key_index.index != node_num_keys(nv_node(&nv)));

	u32 ind = cursor->current_key_index.index;
	result = btree_node_read_at(tree, nv_node(&nv), ind, buffer, buffer_size);
	if( result != BTREE_OK )
		goto end;

end:
	noderc_release(cursor_rcer(cursor), &nv);

	if( cursor )
		cursor_destroy(cursor);

	return result;
}