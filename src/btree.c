#include "btree.h"

#include "binary_search.h"
#include "btree_alg.h"
#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_node_writer.h"
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

u32
btree_min_page_size(void)
{
	u32 min_heap_required_for_largest_cell_type =
		btree_node_get_heap_required_for_insertion(
			btree_cell_overflow_get_min_inline_heap_size());
	// Minimum size to fit 4 overflow cells on the root page.
	return (
		(BTREE_HEADER_SIZE) + (4 * min_heap_required_for_largest_cell_type) +
		+sizeof(struct BTreePageHeader));
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

	// Arbitrary 4*12 is approximately 4 entries that overflow.
	// 1 int for cell size, 2 more for overflow page meta.
	if( pager->page_size < btree_min_page_size() )
		return BTREE_ERR_INVALID_PAGE_SIZE_TOO_SMALL;

	tree->root_page_id = 1;
	tree->pager = pager;

	struct Page* page = NULL;
	page_create(tree->pager, &page);
	btree_result = btree_init_root_page(tree, page);
	if( btree_result != BTREE_OK )
		goto end;

	tree->header = *(struct BTreeHeader*)page->page_buffer;
end:
	if( page )
		page_destroy(tree->pager, page);
	return btree_result;
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

enum btree_e
swap_root_page(struct BTree* tree, u32 other_page_id)
{
	enum btree_e result = BTREE_OK;
	struct Page* root_page = NULL;
	struct Page* other_page = NULL;
	struct BTreeNode* root_ptr = {0};
	struct BTreeNode other = {0};
	struct PageSelector selector = {0};

	page_create(tree->pager, &root_page);
	page_create(tree->pager, &other_page);

	btree_node_create_as_page_number(&root_ptr, tree->root_page_id, root_page);

	pager_reselect(&selector, other_page_id);
	pager_read_page(tree->pager, &selector, other_page);
	btree_node_init_from_page(&other, other_page);

	root_ptr->header->is_leaf = other.header->is_leaf;

	struct MergedPage merge_result = {0};
	bta_merge_nodes(root_ptr, &other, tree->pager, &merge_result);

	if( root_ptr )
		btree_node_destroy(root_ptr);

	page_destroy(tree->pager, root_page);
	page_destroy(tree->pager, other_page);

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

		result = btree_node_remove(&node, &crumb.key_index, NULL, NULL, 0);
		if( result != BTREE_OK )
			break;

		pager_write_page(tree->pager, node.page);

		// TODO: Small size threshold.
		if( node.header->num_keys == 0 )
		{
			// Keep deleting.
			// TODO: Add empty page to free list if it's not the highwater page.

			// TODO: Deleting the last key from the root_node should just result
			// in a swap with the last remaining page. page.
			if( page->page_id == tree->root_page_id )
			{
				if( !node.header->is_leaf )
					result = swap_root_page(tree, node.header->right_child);

				break;
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
	return result;
}
