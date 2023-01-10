#include "ibtree.h"

#include "btree.h"
#include "btree_alg.h"
#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_node_writer.h"
#include "btree_utils.h"

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
	right += bytes_compared;

	if( bytes_compared == 0 )
	{
		left_buffer += sizeof(u32);
		left_size -= sizeof(u32);
	}

	*out_bytes_compared = min(left_size, right_size - bytes_compared);
	int cmp = memcmp(left, right, *out_bytes_compared);
	if( cmp == 0 )
	{
		if( left_size == right_size )
			return 0;
		else
			return left_size < right_size ? -1 : 1;
	}
	else
	{
		return cmp;
	}
}

enum btree_e
ibtree_insert(struct BTree* tree, void* key, int key_size)
{
	assert(tree->compare != NULL);

	enum btree_e result = BTREE_OK;
	char found = 0;
	struct Page* page = NULL;
	struct PageSelector selector = {0};
	struct BTreeNode node = {0};
	struct CursorBreadcrumb crumb = {0};
	struct Cursor* cursor = cursor_create(tree);
	page_create(tree->pager, &page);

	result = cursor_traverse_to_ex(cursor, key, key_size, &found);
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

		btree_node_init_from_page(&node, page);

		result = btree_node_write(&node, tree->pager, key, data, data_size);
		if( result == BTREE_ERR_NODE_NOT_ENOUGH_SPACE )
		{
			// We want to keep the root node as the first page.
			// So if the first page is to be split, then split
			// the data in this page between two children nodes.
			// This node becomes the new parent of those nodes.
			if( node.page->page_id == 1 )
			{
				struct SplitPageAsParent split_result;
				bta_split_node_as_parent(&node, tree->pager, &split_result);

				// TODO: Key compare function.
				if( key <= split_result.left_child_high_key )
				{
					pager_reselect(&selector, split_result.left_child_page_id);
				}
				else
				{
					pager_reselect(&selector, split_result.right_child_page_id);
				}

				pager_read_page(tree->pager, &selector, page);
				btree_node_init_from_page(&node, page);

				result =
					btree_node_write(&node, tree->pager, key, data, data_size);
				if( result != BTREE_OK )
					goto end;

				goto end;
			}
			else
			{
				struct SplitPage split_result;
				bta_split_node(&node, tree->pager, &split_result);

				// TODO: Key compare function.
				if( key <= split_result.left_page_high_key )
				{
					pager_reselect(&selector, split_result.left_page_id);
					pager_read_page(tree->pager, &selector, page);
					btree_node_init_from_page(&node, page);
				}

				result =
					btree_node_write(&node, tree->pager, key, data, data_size);
				if( result != BTREE_OK )
					goto end;

				// Move the cursor one to the left so we can insert there.
				result = cursor_pop(cursor, &crumb);
				if( result != BTREE_OK )
					goto end;

				// TODO: Better way to do this?
				// Basically we have to insert the left child to the left of the
				// node that was split.
				//
				// It is expected that the index of a key with KLIM_RIGHT_CHILD
				// is the num keys on that page.
				// cursor->current_key_index.index;

				cursor->current_key_index.mode = KLIM_INDEX;
				result = cursor_push(cursor);
				if( result != BTREE_OK )
					goto end;

				// Args for next iteration
				// Insert the new child's page number into the parent.
				// The parent pointer is already the key of the highest
				// key in the right page, so that can stay the same.
				// Insert the highest key of the left page.
				//    K5              K<5   K5
				//     |   ---->      |     |
				//    K5              K<5   K5
				//
				key = split_result.left_page_high_key;
				data = (void*)&split_result.left_page_id;
				data_size = sizeof(split_result.left_page_id);
			}
		}
		else
		{
			break;
		}
	}

end:
	cursor_destroy(cursor);
	page_destroy(tree->pager, page);
	return BTREE_OK;
}
