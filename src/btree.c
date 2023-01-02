#include "btree.h"

#include "binary_search.h"
#include "btree_alg.h"
#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_utils.h"

#include <assert.h>
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
	struct Page* page = NULL;
	struct PageSelector selector = {0};
	struct BTreeNode* node = NULL;
	struct Cursor* cursor = cursor_create(tree);

	result = cursor_traverse_to(cursor, key, &found);
	if( result != BTREE_OK )
		return result;

	page_create(tree->pager, &page);

	while( 1 )
	{
		pager_reselect(&selector, cursor->current_page_id);
		pager_read_page(tree->pager, &selector, page);

		// TODO: Don't need to alloc in loop.
		btree_node_create_from_page(&node, page);

		result =
			btree_node_insert(node, cursor->current_key, key, data, data_size);
		if( result == BTREE_ERR_NODE_NOT_ENOUGH_SPACE )
		{
			if( node->page->page_id != 1 )
			{
				struct SplitPage split_result;
				bta_split_node(tree, node, &split_result);

				if( key >= split_result.right_page_low_key )
				{
					pager_reselect(&selector, split_result.right_page_id);
					pager_read_page(tree->pager, &selector, page);
					btree_node_init_from_page(node, page);
				}

				int new_insert_index = btu_binary_search_keys(
					node->keys, node->header->num_keys, key, &found);
				btree_node_insert(node, new_insert_index, key, data, data_size);

				// Args for next iteration
				result = cursor_select_parent(cursor);
				if( result == BTREE_ERR_CURSOR_NO_PARENT )
					assert(0);

				key = cursor->current_key;
				data = (void*)split_result.right_page_id;
				data_size = sizeof(split_result.right_page_id);

				btree_node_destroy(node);
			}
			else
			{
				bta_bplus_split_node(tree, node);
				btree_node_destroy(node);
				goto end;
			}
		}
		else
		{
			pager_write_page(tree->pager, page);
			btree_node_destroy(node);
			break;
		}
	}

end:
	cursor_destroy(cursor);
	page_destroy(tree->pager, page);
	return BTREE_OK;
}
