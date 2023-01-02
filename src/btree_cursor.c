#include "btree_cursor.h"

#include "btree_node.h"
#include "btree_utils.h"
#include "pager.h"

#include <stdlib.h>
#include <string.h>

struct Cursor*
cursor_create(struct BTree* tree)
{
	struct Cursor* cursor = (struct Cursor*)malloc(sizeof(struct Cursor));

	memset(cursor, 0x00, sizeof(*cursor));

	cursor->tree = tree;
	cursor->current_key = 0;
	cursor->current_page_id = tree->root_page_id;
	return cursor;
}

void
cursor_destroy(struct Cursor* cursor)
{
	free(cursor);
}

enum btree_e
cursor_select_parent(struct Cursor* cursor)
{
	if( cursor->breadcrumbs_size == 0 )
		return BTREE_ERR_CURSOR_NO_PARENT;

	struct CursorBreadcrumb* crumb =
		&cursor->breadcrumbs[cursor->breadcrumbs_size - 1];
	cursor->current_page_id = crumb->page_id;
	cursor->current_key = crumb->key;
	cursor->breadcrumbs_size--;

	return BTREE_OK;
}

enum btree_e
cursor_traverse_to(struct Cursor* cursor, int key, char* found)
{
	int child_key_index = 0;
	enum btree_e result = BTREE_OK;
	enum pager_e page_result = PAGER_OK;
	struct Page* page = NULL;
	struct PageSelector selector = {0};
	struct BTreeNode node = {0};
	struct CellData cell = {0};

	page_result = page_create(cursor->tree->pager, &page);
	if( page_result != PAGER_OK )
		return BTREE_ERR_UNK;

	do
	{
		struct CursorBreadcrumb* crumb =
			&cursor->breadcrumbs[cursor->breadcrumbs_size];
		crumb->key = cursor->current_key;
		crumb->page_id = cursor->current_page_id;
		cursor->breadcrumbs_size++;

		page_result = pager_reselect(&selector, cursor->current_page_id);
		if( page_result != PAGER_OK )
		{
			result = BTREE_ERR_UNK;
			goto end;
		}

		page_result = pager_read_page(cursor->tree->pager, &selector, page);
		if( page_result != PAGER_OK )
		{
			result = BTREE_ERR_UNK;
			goto end;
		}

		result = btree_node_init_from_page(&node, page);
		if( result != BTREE_OK )
			goto end;

		child_key_index = btu_binary_search_keys(
			node.keys, node.header->num_keys, key, found);

		cursor->current_key = child_key_index;

		if( !node.header->is_leaf )
		{
			if( child_key_index == node.header->num_keys )
			{
				cursor->current_page_id = node.header->right_child;
			}
			else
			{
				btu_read_cell(&node, child_key_index, &cell);
				if( *cell.size > sizeof(cursor->current_page_id) )
				{
					result = BTREE_ERR_CORRUPT_CELL;
					goto end;
				}

				memcpy(&cursor->current_page_id, cell.pointer, *cell.size);
			}
		}
	} while( !node.header->is_leaf );

end:
	page_destroy(cursor->tree->pager, page);

	return result;
}