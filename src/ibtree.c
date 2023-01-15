#include "ibtree.h"

#include "btree.h"
#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_node_debug.h"
#include "btree_node_writer.h"
#include "btree_utils.h"
#include "ibtree_alg.h"

#include <assert.h>
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
	char found = 0;
	int page_index_as_key =
		0; // This is only not zero when there is a page split.
	struct Page* page = NULL;
	struct Page* holding_page_one = NULL;
	struct Page* holding_page_two = NULL;
	struct PageSelector selector = {0};
	struct BTreeNode node = {0};
	struct BTreeNode holding_node_one = {0};
	struct BTreeNode holding_node_two = {0};

	struct BTreeNode* next_holding_node = &holding_node_one;

	struct CursorBreadcrumb crumb = {0};
	u32 flags = 0; // Only used when writing to parent.
	enum writer_ex_mode_e writer_mode = WRITER_EX_MODE_RAW;

	struct Cursor* cursor = cursor_create(tree);
	// Holding page and node is required to move cells up the tree.
	result = btpage_err(page_create(tree->pager, &holding_page_one));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_init_from_page(&holding_node_one, holding_page_one);
	if( result != BTREE_OK )
		goto end;

	result = btpage_err(page_create(tree->pager, &holding_page_two));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_init_from_page(&holding_node_two, holding_page_two);
	if( result != BTREE_OK )
		goto end;

	result = btpage_err(page_create(tree->pager, &page));
	if( result != BTREE_OK )
		goto end;

	result = cursor_traverse_to_ex(cursor, payload, payload_size, &found);
	if( result != BTREE_OK )
		goto end;

	while( 1 )
	{
		result = cursor_pop(cursor, &crumb);
		if( result != BTREE_OK )
			goto end;

		pager_reselect(&selector, crumb.page_id);
		result = btpage_err(pager_read_page(tree->pager, &selector, page));
		if( result != BTREE_OK )
			goto end;

		result = btree_node_init_from_page(&node, page);
		if( result != BTREE_OK )
			goto end;

		struct InsertionIndex index = from_cli(&cursor->current_key_index);

		result = btree_node_write_ex(
			&node,
			tree->pager,
			&index,
			page_index_as_key,
			flags, // Nonzero only on split.
			payload,
			payload_size,
			writer_mode);
		if( result == BTREE_ERR_NODE_NOT_ENOUGH_SPACE )
		{
			if( node.page->page_id == 1 )
			{
				// TODO: This first half thing is shaky.
				u32 first_half = (node.header->num_keys + 1) / 2;
				int child_insertion = left_or_right_insertion(&index, &node);

				struct SplitPageAsParent split_result;
				result = ibta_split_node_as_parent(
					&node, tree->pager, &split_result);
				if( result != BTREE_OK )
					goto end;

				pager_reselect(
					&selector,
					child_insertion == -1 ? split_result.left_child_page_id
										  : split_result.right_child_page_id);

				result =
					btpage_err(pager_read_page(tree->pager, &selector, page));
				if( result != BTREE_OK )
					goto end;

				result = btree_node_init_from_page(&node, page);
				if( result != BTREE_OK )
					goto end;

				if( child_insertion == 1 )
					index.index -= first_half;

				result = btree_node_write_ex(
					&node,
					tree->pager,
					&index,
					page_index_as_key,
					flags,
					payload,
					payload_size,
					writer_mode);

				if( result != BTREE_OK )
					goto end;

				goto end;
			}
			else
			{
				u32 first_half = (node.header->num_keys + 1) / 2;
				int child_insertion = left_or_right_insertion(&index, &node);

				memset(
					next_holding_node->page->page_buffer,
					0x00,
					next_holding_node->page->page_size);
				btree_node_init_from_page(
					next_holding_node, next_holding_node->page);

				struct SplitPage split_result;
				result = ibta_split_node(
					&node, tree->pager, next_holding_node, &split_result);
				if( result != BTREE_OK )
					goto end;

				// TODO: Key compare function.
				if( child_insertion == -1 )
				{
					pager_reselect(&selector, split_result.left_page_id);
					pager_read_page(tree->pager, &selector, page);
					btree_node_init_from_page(&node, page);
				}

				if( child_insertion == 1 )
					index.index -= first_half;

				// Write the input payload to the correct child.
				result = btree_node_write_ex(
					&node,
					tree->pager,
					&index,
					page_index_as_key,
					flags,
					payload,
					payload_size,
					writer_mode);
				if( result != BTREE_OK )
					goto end;

				// Now we need to write the holding key.
				result = cursor_pop(cursor, &crumb);
				if( result != BTREE_OK )
					goto end;

				cursor->current_key_index.mode = KLIM_INDEX;
				result = cursor_push(cursor);
				if( result != BTREE_OK )
					goto end;

				writer_mode = WRITER_EX_MODE_CELL_MOVE;
				flags = next_holding_node->keys[0].flags;
				page_index_as_key = split_result.left_page_id;

				payload = btu_get_cell_buffer(next_holding_node, 0);
				payload_size = btu_get_cell_buffer_size(next_holding_node, 0);

				next_holding_node = next_holding_node == &holding_node_one
										? &holding_node_two
										: &holding_node_one;
			}
		}
		else
		{
			break;
		}
	}

end:
	if( cursor )
		cursor_destroy(cursor);
	if( page )
		page_destroy(tree->pager, page);
	if( holding_page_one )
		page_destroy(tree->pager, holding_page_one);
	if( holding_page_two )
		page_destroy(tree->pager, holding_page_two);
	return BTREE_OK;
}
