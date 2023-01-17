#include "btree_cursor.h"

#include "btree_cell.h"
#include "btree_node.h"
#include "btree_utils.h"
#include "pager.h"

#include <assert.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

struct Cursor*
cursor_create(struct BTree* tree)
{
	struct Cursor* cursor = (struct Cursor*)malloc(sizeof(struct Cursor));

	memset(cursor, 0x00, sizeof(*cursor));

	cursor->tree = tree;
	cursor->current_page_id = tree->root_page_id;
	return cursor;
}

void
cursor_destroy(struct Cursor* cursor)
{
	free(cursor);
}

enum btree_e
cursor_push(struct Cursor* cursor)
{
	if( cursor->breadcrumbs_size ==
		sizeof(cursor->breadcrumbs) / sizeof(cursor->breadcrumbs[0]) )
		return BTREE_ERR_CURSOR_DEPTH_EXCEEDED;

	struct CursorBreadcrumb* crumb =
		&cursor->breadcrumbs[cursor->breadcrumbs_size];
	crumb->key_index = cursor->current_key_index;
	crumb->page_id = cursor->current_page_id;
	cursor->breadcrumbs_size++;

	return BTREE_OK;
}

enum btree_e
cursor_push_crumb(struct Cursor* cursor, struct CursorBreadcrumb* crumb)
{
	if( cursor->breadcrumbs_size ==
		sizeof(cursor->breadcrumbs) / sizeof(cursor->breadcrumbs[0]) )
		return BTREE_ERR_CURSOR_DEPTH_EXCEEDED;

	cursor->breadcrumbs[cursor->breadcrumbs_size] = *crumb;
	cursor->breadcrumbs_size++;

	cursor->current_page_id =
		cursor->breadcrumbs[cursor->breadcrumbs_size - 1].page_id;
	cursor->current_key_index =
		cursor->breadcrumbs[cursor->breadcrumbs_size - 1].key_index;

	return BTREE_OK;
}

enum btree_e
cursor_pop(struct Cursor* cursor, struct CursorBreadcrumb* crumb)
{
	if( cursor->breadcrumbs_size == 0 )
		return BTREE_ERR_CURSOR_NO_PARENT;

	if( crumb )
		*crumb = cursor->breadcrumbs[cursor->breadcrumbs_size - 1];
	cursor->breadcrumbs_size--;

	if( cursor->breadcrumbs_size == 0 )
	{
		cursor->current_page_id = 0;
		memset(
			&cursor->current_key_index,
			0x00,
			sizeof(cursor->current_key_index));
	}
	else
	{
		cursor->current_page_id =
			cursor->breadcrumbs[cursor->breadcrumbs_size - 1].page_id;
		cursor->current_key_index =
			cursor->breadcrumbs[cursor->breadcrumbs_size - 1].key_index;
	}

	return BTREE_OK;
}

enum btree_e
cursor_peek(struct Cursor* cursor, struct CursorBreadcrumb* crumb)
{
	if( cursor->breadcrumbs_size == 0 )
		return BTREE_ERR_CURSOR_NO_PARENT;

	if( crumb )
		*crumb = cursor->breadcrumbs[cursor->breadcrumbs_size - 1];
	return BTREE_OK;
}

/**
 * @brief Gets the child page id from the cell.
 *
 * @param cursor
 * @param node
 * @param child_key_index
 * @return enum btree_e
 */
static enum btree_e
read_cell_page(
	struct Cursor* cursor, struct BTreeNode* node, u32 child_key_index)
{
	enum btree_e result = BTREE_OK;
	if( child_key_index >= node->header->num_keys )
	{
		cursor->current_page_id = node->header->right_child;
	}
	else
	{
		if( cursor->tree->type == BTREE_TBL )
		{
			struct CellData cell = {0};
			btu_read_cell(node, child_key_index, &cell);
			u32 cell_size = btree_cell_get_size(&cell);
			if( cell_size != sizeof(cursor->current_page_id) )
			{
				result = BTREE_ERR_CORRUPT_CELL;
				goto end;
			}

			memcpy(&cursor->current_page_id, cell.pointer, cell_size);
		}
		else
		{
			cursor->current_page_id = node->keys[child_key_index].key;
		}
	}

end:
	return result;
}

enum btree_e
cursor_traverse_to(struct Cursor* cursor, int key, char* found)
{
	int child_key_index = 0;
	enum btree_e result = BTREE_OK;
	struct Page* page = NULL;
	struct BTreeNode node = {0};

	result = btpage_err(page_create(cursor->tree->pager, &page));
	if( result != BTREE_OK )
		goto end; // TODO: No-mem

	do
	{
		result = btree_node_init_from_read(
			&node, page, cursor->tree->pager, cursor->current_page_id);
		if( result != BTREE_OK )
			goto end;

		child_key_index = btu_binary_search_keys(
			node.keys, node.header->num_keys, key, found);

		btu_init_keylistindex_from_index(
			&cursor->current_key_index, &node, child_key_index);

		result = cursor_push(cursor);
		if( result != BTREE_OK )
			goto end;

		if( !node.header->is_leaf )
		{
			result = read_cell_page(cursor, &node, child_key_index);
			if( result != BTREE_OK )
				goto end;
		}
	} while( !node.header->is_leaf );

end:
	if( page )
		page_destroy(cursor->tree->pager, page);

	return result;
}

enum btree_e
cursor_traverse_to_ex(
	struct Cursor* cursor, void* key, u32 key_size, char* found)
{
	u32 child_key_index = 0;
	enum btree_e result = BTREE_OK;
	struct Page* page = NULL;
	struct BTreeNode node = {0};
	*found = 0;

	result = btpage_err(page_create(cursor->tree->pager, &page));
	if( result != BTREE_OK )
		goto end; // TODO: No-mem

	do
	{
		result = btree_node_init_from_read(
			&node, page, cursor->tree->pager, cursor->current_page_id);
		if( result != BTREE_OK )
			goto end;

		result = btree_node_search_keys(
			cursor->tree, &node, key, key_size, &child_key_index);
		if( result == BTREE_OK )
			*found = 1;

		if( result == BTREE_ERR_KEY_NOT_FOUND )
			result = BTREE_OK;

		if( result != BTREE_OK )
			goto end;

		btu_init_keylistindex_from_index(
			&cursor->current_key_index, &node, child_key_index);

		result = cursor_push(cursor);
		if( result != BTREE_OK )
			goto end;

		if( !node.header->is_leaf && !(*found) )
		{
			result = read_cell_page(cursor, &node, child_key_index);
			if( result != BTREE_OK )
				goto end;
		}
		// TODO: Only break if ibtree
	} while( !node.header->is_leaf && !(*found) );

end:
	if( page )
		page_destroy(cursor->tree->pager, page);

	return result;
}

enum btree_e
cursor_traverse_largest(struct Cursor* cursor)
{
	enum btree_e result = BTREE_OK;
	struct Page* page = NULL;
	struct BTreeNode node = {0};
	struct CursorBreadcrumb crumb = {0};

	result = btpage_err(page_create(cursor->tree->pager, &page));
	if( result != BTREE_OK )
		goto end; // TODO: No-mem

	do
	{
		result = cursor_pop(cursor, &crumb);
		if( result != BTREE_OK )
			goto end;

		result = btree_node_init_from_read(
			&node, page, cursor->tree->pager, crumb.page_id);
		if( result != BTREE_OK )
			goto end;

		if( node.header->is_leaf )
		{
			cursor->current_key_index.index = node.header->num_keys - 1;
			cursor->current_key_index.mode = KLIM_INDEX;
			cursor->current_page_id = crumb.page_id;
		}
		else
		{
			cursor->current_key_index.index = node.header->num_keys;
			cursor->current_key_index.mode = KLIM_RIGHT_CHILD;
			cursor->current_page_id = crumb.page_id;
		}

		result = cursor_push(cursor);
		if( result != BTREE_OK )
			goto end;

		if( !node.header->is_leaf )
		{
			result =
				read_cell_page(cursor, &node, cursor->current_key_index.index);
			if( result != BTREE_OK )
				goto end;
		}
	} while( !node.header->is_leaf );

end:
	if( page )
		page_destroy(cursor->tree->pager, page);

	return result;
}

enum btree_e
cursor_sibling(struct Cursor* cursor, enum cursor_sibling_e sibling)
{
	enum btree_e result = BTREE_OK;
	enum btree_e restore_result = BTREE_OK;
	struct Page* page = NULL;
	struct BTreeNode node = {0};
	struct CursorBreadcrumb crumb = {0};
	struct CursorBreadcrumb restore_crumb = {0};

	if( cursor->breadcrumbs_size < 1 )
		return BTREE_ERR_CURSOR_NO_PARENT;

	result = btpage_err(page_create(cursor->tree->pager, &page));
	if( result != BTREE_OK )
		goto end;

	// Pop the child.
	result = cursor_pop(cursor, &restore_crumb);
	if( result != BTREE_OK )
		goto end;

	// Pop the parent.
	result = cursor_pop(cursor, &crumb);
	if( result != BTREE_OK )
		goto end;

	result = btree_node_init_from_read(
		&node, page, cursor->tree->pager, crumb.page_id);
	if( result != BTREE_OK )
		goto end;

	u32 sibling_id = 0;
	if( sibling == CURSOR_SIBLING_LEFT )
	{
		if( crumb.key_index.index == 0 )
		{
			result = BTREE_ERR_CURSOR_NO_SIBLING;
			goto undo;
		}

		cursor->current_key_index.mode = KLIM_INDEX;
		cursor->current_key_index.index = crumb.key_index.index - 1;
		cursor->current_page_id = crumb.page_id;
	}
	else
	{
		if( (crumb.key_index.index == node.header->num_keys ||
			 crumb.key_index.mode != KLIM_INDEX) &&
			node.header->right_child == 0 )
		{
			result = BTREE_ERR_CURSOR_NO_SIBLING;
			goto undo;
		}

		if( crumb.key_index.index == node.header->num_keys )
		{
			cursor->current_key_index.mode = KLIM_RIGHT_CHILD;
			cursor->current_key_index.index = node.header->num_keys;
		}
		else
		{
			cursor->current_key_index.mode = KLIM_INDEX;
			cursor->current_key_index.index = crumb.key_index.index + 1;
		}

		cursor->current_page_id = crumb.page_id;
	}

	result = cursor_push(cursor);
	if( result != BTREE_OK )
		goto end;

	// Read the page id.
	if( sibling == CURSOR_SIBLING_LEFT )
	{
		result = read_cell_page(cursor, &node, crumb.key_index.index - 1);
		if( result != BTREE_OK )
			goto end;
	}
	else
	{
		result = read_cell_page(cursor, &node, crumb.key_index.index + 1);
		if( result != BTREE_OK )
			goto end;
	}

	// Cursor is now point to the child; point to first element of sibling.
	cursor->current_key_index.index = 0;
	cursor->current_key_index.mode = KLIM_INDEX;

	result = cursor_push(cursor);
	if( result != BTREE_OK )
		goto end;

end:
	if( page )
		page_destroy(cursor->tree->pager, page);

	return result;

undo:
	restore_result = result;
	result = cursor_push_crumb(cursor, &crumb);
	if( result != BTREE_OK )
		goto end;
	result = cursor_push_crumb(cursor, &restore_crumb);
	if( result != BTREE_OK )
		goto end;
	result = restore_result;
	goto end;
}

// TODO: Should be nodeview
enum btree_e
cursor_read_parent(struct Cursor* cursor, struct BTreeNode* out_node)
{
	enum btree_e result = BTREE_OK;
	struct CursorBreadcrumb crumb = {0};

	assert(out_node->page != NULL);

	result = cursor_pop(cursor, &crumb);
	if( result != BTREE_OK )
		return result;

	result = btree_node_init_from_read(
		out_node, out_node->page, cursor->tree->pager, cursor->current_page_id);
	if( result != BTREE_OK )
		return result;

	result = cursor_push_crumb(cursor, &crumb);
	if( result != BTREE_OK )
		return result;

	return result;
}

enum btree_e
cursor_parent_index(struct Cursor* cursor, struct ChildListIndex* out_index)
{
	enum btree_e result = BTREE_OK;
	struct CursorBreadcrumb crumb = {0};

	result = cursor_pop(cursor, &crumb);
	if( result != BTREE_OK )
		return result;

	*out_index = cursor->current_key_index;

	result = cursor_push_crumb(cursor, &crumb);
	if( result != BTREE_OK )
		return result;

	return result;
}