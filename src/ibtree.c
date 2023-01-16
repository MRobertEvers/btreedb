#include "ibtree.h"

#include "btree.h"
#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_node_debug.h"
#include "btree_node_writer.h"
#include "btree_utils.h"
#include "ibtree_alg.h"

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

int
ibtree_payload_writer(void* data, void* cell, struct BufferWriter* writer)
{}

enum btree_e
ibtree_init(
	struct BTree* tree,
	struct Pager* pager,
	u32 root_page_id,
	btree_compare_fn compare)
{
	enum btree_e result = btree_init(tree, pager, root_page_id);
	tree->type = BTREE_INDEX;
	tree->compare = compare;
	return result;
}

int
ibtree_compare(
	void* left,
	u32 left_size,
	void* right,
	u32 right_size,
	u32 bytes_compared,
	u32* out_bytes_compared)
{
	assert(right_size > bytes_compared);

	// The inline payload of a cell on disk for an IBTree always starts with the
	// left child page id.
	byte* left_buffer = (byte*)left;
	byte* right_buffer = (byte*)right;
	right_buffer += bytes_compared;

	*out_bytes_compared = min(left_size, right_size - bytes_compared);
	int cmp = memcmp(left_buffer, right_buffer, *out_bytes_compared);
	if( cmp == 0 )
	{
		return cmp;
	}
	else
	{
		return cmp < 0 ? -1 : 1;
	}
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

/**
 * @brief
 *
 * @param index
 * @param node
 * @return int 1 for right, -1 for left
 */
int
left_or_right_insertion(struct InsertionIndex* index, struct BTreeNode* node)
{
	if( index->mode != KLIM_INDEX )
		return 1;

	u32 first_half = (node->header->num_keys + 1) / 2;
	if( index->index < first_half )
	{
		return -1;
	}
	else
	{
		return 1;
	}
}

enum btree_e
ibtree_insert(struct BTree* tree, void* payload, int payload_size)
{
	assert(tree->compare != NULL);
	enum btree_e result = BTREE_OK;
	char found;
	struct Cursor* cursor = cursor_create(tree);

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

enum btree_e
find_largest_left_child(struct Cursor* cursor)
{}

enum btree_e
ibtree_delete(struct BTree* tree, void* key, int key_size)
{
	enum btree_e result = BTREE_OK;
	char found = 0;
	struct Page* page = NULL;
	struct Page* holding_page = NULL;

	struct BTreeNode node = {0};
	struct BTreeNode holding_node = {0};

	struct CursorBreadcrumb crumb = {0};
	struct Cursor* cursor = cursor_create(tree);

	result = btpage_err(page_create(tree->pager, &holding_page));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_init_from_page(&holding_node, holding_page);
	if( result != BTREE_OK )
		goto end;

	result = btpage_err(page_create(tree->pager, &page));
	if( result != BTREE_OK )
		goto end;

	result = cursor_traverse_to_ex(cursor, key, key_size, &found);
	if( result != BTREE_OK )
		goto end;

	result = cursor_pop(cursor, &crumb);
	if( result != BTREE_OK )
		goto end;

	result = btree_node_init_from_read(&node, page, tree->pager, crumb.page_id);
	if( result != BTREE_OK )
		goto end;

	result = ibtree_node_remove(&node, &crumb.key_index, &holding_node);
	if( result != BTREE_OK )
		goto end;

	result = btpage_err(pager_write_page(tree->pager, node.page));
	if( result != BTREE_OK )
		goto end;

	bool underflow = false;
	// The cell is removed. If it wasn't a leaf node find the largest cell
	// in the left subtree and promote it to the same location. If it was a
	// leaf node, do nothing.
	if( !node.header->is_leaf )
	{
		struct BTreeNode replacement_node = {0};
		struct CursorBreadcrumb replacement_crumb = {0};
		u32 orphaned_left_child = holding_node.keys[0].key;

		// Look
		result = cursor_traverse_left_largest(cursor);
		if( result != BTREE_OK )
			goto end;

		result = cursor_pop(cursor, &replacement_crumb);
		if( result != BTREE_OK )
			goto end;

		result = btree_node_init_from_read(
			&replacement_node, page, tree->pager, replacement_crumb.page_id);
		if( result != BTREE_OK )
			goto end;

		result = btree_node_reset(&holding_node);
		if( result != BTREE_OK )
			goto end;

		result = ibtree_node_remove(
			&replacement_node, &replacement_crumb.key_index, &holding_node);
		if( result != BTREE_OK )
			goto end;

		struct InsertionIndex insert = {0};
		insert.index = crumb.key_index.index;
		insert.mode =
			crumb.key_index.mode == KLIM_RIGHT_CHILD ? KLIM_END : KLIM_INDEX;
		result = btree_node_write_ex(
			&node,
			tree->pager,
			&insert,
			orphaned_left_child,
			holding_node.keys[0].flags,
			btu_get_cell_buffer(&holding_node, 0),
			btu_get_cell_buffer_size(&holding_node, 0),
			WRITER_EX_MODE_CELL_MOVE);
		if( result != BTREE_OK )
			goto end;

		underflow = replacement_node.header->num_keys < 1;
	}
	else
	{
		// Do nothing, detect underflow.
		underflow = node.header->num_keys < 1;
	}

	// If the leaf node underflows, then we need to rebalance.

	// TODO: Assumes min# elements is < 1/2 of the max # elements.
	// TODO: Small size threshold.
	if( underflow )
	{
		result = ibta_rebalance(cursor);
	}

end:
	if( cursor )
		cursor_destroy(cursor);
	if( page )
		page_destroy(tree->pager, page);

	if( holding_page )
		page_destroy(tree->pager, holding_page);
	return result;
}
