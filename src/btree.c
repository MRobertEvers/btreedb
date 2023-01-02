#include "btree.h"

#include "binary_search.h"
#include "btree_alg.h"
#include "btree_node.h"
#include "btree_utils.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int
binary_search_keys(
	struct BTreePageKey* arr, unsigned char num_keys, int key, char* found)
{
	int left = 0;
	int right = num_keys - 1;
	int mid = 0;
	*found = 0;

	while( left <= right )
	{
		mid = (right - left) / 2 + left;

		if( arr[mid].key == key )
		{
			if( found )
				*found = 1;
			return mid;
		}
		else if( arr[mid].key < key )
		{
			left = mid + 1;
		}
		else
		{
			right = mid - 1;
		}
	}

	return left;
}

static enum btree_e
init_new_root_page(struct BTree* tree, struct Page* page)
{
	struct BTreeNode temp_node = {0};

	// TODO: Hack need function to get page id before writing...
	// That way it wont be "PAGE_NEW" here.
	page->page_id = 1;
	btree_node_init_from_page(&temp_node, page);

	struct BTreeHeader* temp_btree_header =
		(struct BTreeHeader*)page->page_buffer;

	temp_node.header->is_leaf = 1;

	temp_btree_header->page_high_water = 2;

	page->page_id = PAGE_CREATE_NEW_PAGE;

	return BTREE_OK;
}

static enum btree_e
btree_init_root_page(struct BTree* tree, struct Page* page)
{
	enum pager_e pager_status = PAGER_OK;

	pager_status = pager_read_page(tree->pager, page);
	if( pager_status == PAGER_ERR_NIF )
	{
		page_reselect(page, PAGE_CREATE_NEW_PAGE);

		init_new_root_page(tree, page);

		pager_status = pager_write_page(tree->pager, page);
		if( pager_status == PAGER_OK )
		{
			return BTREE_OK;
		}
		else
		{
			return BTREE_UNK_ERR;
		}
	}
	else if( pager_status != PAGER_OK )
	{
		return BTREE_UNK_ERR;
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
	page_create(tree->pager, 1, &page);
	btree_result = btree_init_root_page(tree, page);
	if( btree_result != BTREE_OK )
		return BTREE_UNK_ERR;

	tree->header = *(struct BTreeHeader*)page->page_buffer;

	return BTREE_OK;
}

enum btree_e
btree_deinit(struct BTree* tree)
{
	// TODO: Nodes?
	return BTREE_OK;
}

struct Cursor*
create_cursor(struct BTree* tree)
{
	struct Cursor* cursor = (struct Cursor*)malloc(sizeof(struct Cursor));

	memset(cursor, 0x00, sizeof(*cursor));

	cursor->tree = tree;
	cursor->current_key = 0;
	cursor->current_page_id = tree->root_page_id;
	return cursor;
}

void
destroy_cursor(struct Cursor* cursor)
{
	free(cursor);
}

enum btree_e
cursor_select_parent(struct Cursor* cursor)
{
	if( cursor->breadcrumbs_size == 0 )
		return BTREE_ERR_CURSOR_NO_PARENT;

	struct CursorBreadcrumb* crumb =
		&cursor->breadcrumbs[cursor->breadcrumbs_size--];
	cursor->current_page_id = crumb->page_id;
	cursor->current_key = crumb->key;

	return BTREE_OK;
}

enum btree_e
btree_insert2(struct BTree* tree, int key, void* data, int data_size)
{
	enum btree_e result = BTREE_OK;
	int index = 0;
	char found = 0;
	struct Page* page = NULL;
	struct BTreeNode* node = NULL;
	struct Cursor* cursor = create_cursor(tree);

	index = btree_traverse_to(cursor, key, &found);

	page_create(tree->pager, cursor->current_page_id, &page);
	while( 1 )
	{
		page_reselect(page, cursor->current_page_id);
		pager_read_page(tree->pager, page);
		// TODO: Don't need to alloc in loop.
		btree_node_create_from_page(tree, &node, page);

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
					page_reselect(page, split_result.right_page_id);
					pager_read_page(tree->pager, page);
					btree_node_init_from_page(node, page);
				}

				int new_insert_index = binary_search_keys(
					node->keys, node->header->num_keys, key, &found);
				btree_node_insert(node, new_insert_index, key, data, data_size);

				// Args for next iteration
				result = cursor_select_parent(cursor);
				if( result == BTREE_ERR_CURSOR_NO_PARENT )
					assert(0);

				key = cursor->current_key;
				data = (void*)split_result.right_page_id;
				data_size = sizeof(split_result.right_page_id);

				btree_node_destroy(tree, node);
			}
			else
			{
				// TODO:
				// This split_node uses the input node as the parent.
				// split_node_stable
				bta_bplus_split_node(tree, node);
				btree_node_destroy(tree, node);
				goto end;
			}
		}
		else
		{
			pager_write_page(tree->pager, page);
			btree_node_destroy(tree, node);
			break;
		}
	}

end:
	destroy_cursor(cursor);
	page_destroy(tree->pager, page);
	return BTREE_OK;
}

enum btree_e
btree_insert(struct BTree* tree, int key, void* data, int data_size)
{
	enum btree_e result = BTREE_OK;
	int index = 0;
	char found = 0;

	struct Cursor* cursor = create_cursor(tree);

	index = btree_traverse_to(cursor, key, &found);

	struct Page* page = NULL;
	struct BTreeNode* node = NULL;
	page_create(tree->pager, cursor->current_page_id, &page);
	pager_read_page(tree->pager, page);
	btree_node_create_from_page(tree, &node, page);

	// TODO: Insert w/ split if not enough space

	// start:
	//
	// insert_data_into_node(insert_page, data)
	//
	// if not space
	//
	// 	insert_page = cursor->current_page
	//  parent_page => parent
	//  # INSERT_AND_PROPAGATE:
	// 	(current{left}, right) => split_node(insert_page) // left and right are
	// already on disk.
	//  insert_data_into_node(left_or_right, data)
	//  data = right->key
	//  insert_page = insert_page->parent
	//  goto start;
	//
	//
	//  parent = cursor-> (parent of current page)

	result = btree_node_insert(node, cursor->current_key, key, data, data_size);

	pager_write_page(tree->pager, page);

	btree_node_destroy(tree, node);
	page_destroy(tree->pager, page);

	return BTREE_OK;
}

enum btree_e
btree_traverse_to(struct Cursor* cursor, int key, char* found)
{
	int index = 0;
	struct Page* page = NULL;
	struct BTreeNode node = {0};
	struct CellData cell = {0};

	page_create(cursor->tree->pager, cursor->current_page_id, &page);
	do
	{
		struct CursorBreadcrumb* crumb =
			&cursor->breadcrumbs[cursor->breadcrumbs_size];
		crumb->key = cursor->current_key;
		crumb->page_id = cursor->current_page_id;
		cursor->breadcrumbs_size++;

		page_reselect(page, cursor->current_page_id);
		pager_read_page(cursor->tree->pager, page);
		btree_node_init_from_page(&node, page);

		index =
			binary_search_keys(node.keys, node.header->num_keys, key, found);

		cursor->current_key = index;

		if( !node.header->is_leaf )
		{
			if( index == node.header->num_keys )
			{
				cursor->current_page_id = node.header->right_child;
			}
			else
			{
				btu_read_cell(&node, index, &cell);
				memcpy(&cursor->current_page_id, cell.pointer, *cell.size);
			}
		}
	} while( !node.header->is_leaf );

	page_destroy(cursor->tree->pager, page);

	return BTREE_OK;
}