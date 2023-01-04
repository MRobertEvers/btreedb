#include "btree.h"

#include "binary_search.h"
#include "btree_alg.h"
#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_node_debug.h"
#include "btree_utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Initializes the first page of a file as the root page.
 *
 * page->page_id must be 1.
 *
 * @param tree
 * @param page
 * @return enum btree_e
 */
static enum btree_e
init_new_root_page(struct BTree* tree, struct Page* page)
{
	assert(page->page_id == 1);

	struct BTreeNode temp_node = {0};

	btree_node_init_from_page(&temp_node, page);

	struct BTreeHeader* temp_btree_header =
		(struct BTreeHeader*)page->page_buffer;

	temp_node.header->is_leaf = 1;

	temp_btree_header->page_high_water = 2;

	return BTREE_OK;
}

static enum btree_e
btree_init_root_page(struct BTree* tree, struct Page* page)
{
	enum pager_e pager_status = PAGER_OK;
	struct PageSelector selector;
	pager_reselect(&selector, 1);

	pager_status = pager_read_page(tree->pager, &selector, page);
	if( pager_status == PAGER_ERR_NIF )
	{
		init_new_root_page(tree, page);

		pager_status = pager_write_page(tree->pager, page);
		if( pager_status == PAGER_OK )
		{
			return BTREE_OK;
		}
		else
		{
			return BTREE_ERR_UNK;
		}
	}
	else if( pager_status != PAGER_OK )
	{
		return BTREE_ERR_UNK;
	}
	else
	{
		return BTREE_OK;
	}
}

enum btree_e
btree_alloc(struct BTree** r_tree)
{
	*r_tree = (struct BTree*)malloc(sizeof(struct BTree));
	return BTREE_OK;
}

enum btree_e
btree_dealloc(struct BTree* tree)
{
	free(tree);
	return BTREE_OK;
}

/**
 * @brief Load the root page into memory and read out information from the btree
 * header.
 *
 * @param tree
 * @return enum btree_e
 */

enum btree_e
btree_init(struct BTree* tree, struct Pager* pager)
{
	enum btree_e btree_result = BTREE_OK;

	// Arbitrary 4*16 is approximately 4 keys.
	if( pager->page_size < ((BTREE_HEADER_SIZE) + (4 * 16)) )
		return BTREE_ERR_INVALID_PAGE_SIZE_TOO_SMALL;

	tree->root_page_id = 1;
	tree->pager = pager;

	struct Page* page = NULL;
	page_create(tree->pager, &page);
	btree_result = btree_init_root_page(tree, page);
	if( btree_result != BTREE_OK )
		return BTREE_ERR_UNK;

	tree->header = *(struct BTreeHeader*)page->page_buffer;

	return BTREE_OK;
}

enum btree_e
btree_deinit(struct BTree* tree)
{
	return BTREE_OK;
}

enum btree_e
btree_insert(struct BTree* tree, int key, void* data, int data_size)
{
	enum btree_e result = BTREE_OK;
	int index = 0;
	char found = 0;
	enum pager_e page_result = PAGER_OK;
	struct Page* page = NULL;
	struct PageSelector selector = {0};
	struct BTreeNode node = {0};
	struct CursorBreadcrumb crumb = {0};
	struct Cursor* cursor = cursor_create(tree);
	page_create(tree->pager, &page);

	result = cursor_traverse_to(cursor, key, &found);
	if( result != BTREE_OK )
		goto end;

	while( 1 )
	{
		result = cursor_pop(cursor, &crumb);
		if( result != BTREE_OK )
			goto end;

		pager_reselect(&selector, crumb.page_id);
		page_result = pager_read_page(tree->pager, &selector, page);
		if( page_result != PAGER_OK )
		{
			result = BTREE_ERR_UNK;
			goto end;
		}

		btree_node_init_from_page(&node, page);

		result =
			btree_node_insert(&node, &crumb.key_index, key, data, data_size);
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

				int new_insert_index = btu_binary_search_keys(
					node.keys, node.header->num_keys, key, &found);

				struct KeyListIndex index = {0};
				btu_init_keylistindex_from_index(
					&index, &node, new_insert_index);
				index.mode = KLIM_END;
				btree_node_insert(&node, &index, key, data, data_size);
				pager_write_page(tree->pager, page);

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

				int new_insert_index = btu_binary_search_keys(
					node.keys, node.header->num_keys, key, &found);

				struct KeyListIndex index = {0};
				btu_init_keylistindex_from_index(
					&index, &node, new_insert_index);

				btree_node_insert(&node, &index, key, data, data_size);
				pager_write_page(tree->pager, node.page);

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
			// Write the page out and we're done.
			pager_write_page(tree->pager, node.page);
			break;
		}
	}

end:
	cursor_destroy(cursor);
	page_destroy(tree->pager, page);
	return BTREE_OK;
}

enum btree_e
btree_delete(struct BTree* tree, int key)
{
	enum btree_e result = BTREE_OK;
	int index = 0;
	char found = 0;
	enum pager_e page_result = PAGER_OK;
	struct Page* page = NULL;
	struct PageSelector selector = {0};
	struct BTreeNode node = {0};
	struct Cursor* cursor = cursor_create(tree);
	page_create(tree->pager, &page);

	result = cursor_traverse_to(cursor, key, &found);
	if( result != BTREE_OK )
		return result;

	while( 1 )
	{
		pager_reselect(&selector, cursor->current_page_id);
		page_result = pager_read_page(tree->pager, &selector, page);
		if( page_result != PAGER_OK )
		{
			result = BTREE_ERR_UNK;
			goto end;
		}

		btree_node_init_from_page(&node, page);

		result = btree_node_delete(&node, &cursor->current_key_index);
		pager_write_page(tree->pager, node.page);

		if( btree_node_arity(&node) == 0 )
		{
			// Args for next iteration
			// Delete the empty page from the parent..
			result = cursor_select_parent(cursor);
			if( result == BTREE_ERR_CURSOR_NO_PARENT )
				goto end;

			// TODO: Add empty page to free list if it's not the highwater page.
		}
		else
		{
			break;
		}
	}

end:
	return result;
}
