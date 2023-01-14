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
	struct Page* holding_page = NULL;
	struct PageSelector selector = {0};
	struct BTreeNode node = {0};
	struct BTreeNode holding_node = {0};
	struct CursorBreadcrumb crumb = {0};
	enum cell_type_e cell_type = CELL_TYPE_UNKNOWN;
	struct Cursor* cursor = cursor_create(tree);
	// Holding page and node is required to move cells up the tree.
	result = btpage_err(page_create(tree->pager, &holding_page));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_init_from_page(&holding_node, holding_page);
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
			payload,
			payload_size);
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
					&node, tree->pager, &index, 0, payload, payload_size);
				if( result != BTREE_OK )
					goto end;

				goto end;
			}
			else
			{
				u32 first_half = (node.header->num_keys + 1) / 2;
				int child_insertion = left_or_right_insertion(&index, &node);

				struct SplitPage split_result;
				result = ibta_split_node(
					&node, tree->pager, &holding_node, &split_result);
				if( result != BTREE_OK )
					goto end;

				// TODO: Key compare function.
				if( child_insertion == -1 )
				{
					pager_reselect(&selector, split_result.left_page_id);
					pager_read_page(tree->pager, &selector, page);
					btree_node_init_from_page(&node, page);
				}

				// Write the input payload to the correct child.
				result = btree_node_write_ex(
					&node, tree->pager, &index, 0, payload, payload_size);
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

				page_index_as_key = split_result.left_page_id;
				payload = (void*)&split_result.left_page_id;
				payload_size = sizeof(split_result.left_page_id);
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
	if( holding_page )
		page_destroy(tree->pager, holding_page);
	return BTREE_OK;
}
