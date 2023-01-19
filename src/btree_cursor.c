#include "btree_cursor.h"

#include "btree_cell.h"
#include "btree_node.h"
#include "btree_utils.h"
#include "noderc.h"
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
struct Cursor*
cursor_create_ex(struct BTree* tree, void* compare_context)
{
	struct Cursor* cursor = cursor_create(tree);

	cursor->compare_context = compare_context;

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
		sizeof(cursor->breadcrumbs) / sizeof(cursor->breadcrumbs[1]) )
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
		sizeof(cursor->breadcrumbs) / sizeof(cursor->breadcrumbs[1]) )
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
cursor_pop_n(struct Cursor* cursor, struct CursorBreadcrumb* crumb, u32 num)
{
	enum btree_e result = BTREE_OK;
	for( int i = 0; i < num; i++ )
	{
		result = cursor_pop(cursor, crumb + i);
		if( result != BTREE_OK )
			break;
	}

	return result;
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

enum btree_e
cursor_restore(
	struct Cursor* cursor, struct CursorBreadcrumb* crumb, u32 num_restore)
{
	enum btree_e result = BTREE_OK;
	for( int i = 0; i < num_restore; i++ )
	{
		result = cursor_push_crumb(cursor, crumb + (num_restore - 1 - i));
		if( result != BTREE_OK )
			return result;
	}

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
	struct NodeView nv = {0};

	result = noderc_acquire(cursor_rcer(cursor), &nv);
	if( result != BTREE_OK )
		goto end; // TODO: No-mem

	do
	{
		result = noderc_reinit_read(
			cursor_rcer(cursor), &nv, cursor->current_page_id);
		if( result != BTREE_OK )
			goto end;

		child_key_index = btu_binary_search_keys(
			nv_node(&nv)->keys, node_num_keys(nv_node(&nv)), key, found);

		btu_init_keylistindex_from_index(
			&cursor->current_key_index, nv_node(&nv), child_key_index);

		result = cursor_push(cursor);
		if( result != BTREE_OK )
			goto end;

		if( !node_is_leaf(nv_node(&nv)) )
		{
			result = read_cell_page(cursor, nv_node(&nv), child_key_index);
			if( result != BTREE_OK )
				goto end;
		}
	} while( !node_is_leaf(nv_node(&nv)) );

end:
	noderc_release(cursor_rcer(cursor), &nv);
	return result;
}

struct BTreeCompareContext
compare_context_init(struct Cursor* cursor)
{
	struct BTreeCompareContext ctx = {
		.compare = cursor->tree->compare,
		.reset = cursor->tree->reset_compare,
		.keyof = cursor->tree->keyof,
		.compare_context = cursor->compare_context,
		.pager = cursor->tree->pager};
	return ctx;
}

enum btree_e
cursor_traverse_to_ex(
	struct Cursor* cursor, void* key, u32 key_size, char* found)
{
	enum btree_e result = BTREE_OK;
	u32 child_key_index = 0;
	struct NodeView nv = {0};
	struct BTreeCompareContext ctx = compare_context_init(cursor);
	*found = 0;

	result = noderc_acquire(cursor_rcer(cursor), &nv);
	if( result != BTREE_OK )
		goto end; // TODO: No-mem

	do
	{
		result = noderc_reinit_read(
			cursor_rcer(cursor), &nv, cursor->current_page_id);
		if( result != BTREE_OK )
			goto end;

		result = btree_node_search_keys(
			&ctx, nv_node(&nv), key, key_size, &child_key_index);
		if( result == BTREE_OK )
			*found = 1;

		if( result == BTREE_ERR_KEY_NOT_FOUND )
			result = BTREE_OK;

		if( result != BTREE_OK )
			goto end;

		btu_init_keylistindex_from_index(
			&cursor->current_key_index, nv_node(&nv), child_key_index);

		result = cursor_push(cursor);
		if( result != BTREE_OK )
			goto end;

		if( !node_is_leaf(nv_node(&nv)) && !(*found) )
		{
			result = read_cell_page(cursor, nv_node(&nv), child_key_index);
			if( result != BTREE_OK )
				goto end;
		}
		// TODO: Only break if ibtree
	} while( !node_is_leaf(nv_node(&nv)) && !(*found) );

end:
	noderc_release(cursor_rcer(cursor), &nv);

	return result;
}

enum btree_e
cursor_traverse_largest(struct Cursor* cursor)
{
	enum btree_e result = BTREE_OK;
	struct CursorBreadcrumb crumb = {0};
	struct NodeView nv = {0};

	result = noderc_acquire(cursor_rcer(cursor), &nv);
	if( result != BTREE_OK )
		goto end;

	do
	{
		result = cursor_pop(cursor, &crumb);
		if( result != BTREE_OK )
			goto end;

		result = noderc_reinit_read(cursor_rcer(cursor), &nv, crumb.page_id);
		if( result != BTREE_OK )
			goto end;

		if( node_is_leaf(nv_node(&nv)) )
		{
			cursor->current_key_index.index = node_num_keys(nv_node(&nv)) - 1;
			cursor->current_key_index.mode = KLIM_INDEX;
			cursor->current_page_id = crumb.page_id;
		}
		else
		{
			cursor->current_key_index.index = node_num_keys(nv_node(&nv));
			cursor->current_key_index.mode = KLIM_RIGHT_CHILD;
			cursor->current_page_id = crumb.page_id;
		}

		result = cursor_push(cursor);
		if( result != BTREE_OK )
			goto end;

		if( !node_is_leaf(nv_node(&nv)) )
		{
			result = read_cell_page(
				cursor, nv_node(&nv), cursor->current_key_index.index);
			if( result != BTREE_OK )
				goto end;
		}
	} while( !node_is_leaf(nv_node(&nv)) );

end:
	noderc_release_n(cursor_rcer(cursor), 1, &nv);

	return result;
}

enum btree_e
cursor_sibling(struct Cursor* cursor, enum cursor_sibling_e sibling)
{
	enum btree_e result = BTREE_OK;
	enum btree_e restore_result = BTREE_OK;
	struct NodeView nv = {0};
	struct CursorBreadcrumb crumbs[2] = {0};

	if( cursor->breadcrumbs_size < 2 )
		return BTREE_ERR_CURSOR_NO_PARENT;

	// Pop the child.
	result = cursor_pop_n(cursor, crumbs, 2);
	if( result != BTREE_OK )
		goto end;

	result = noderc_acquire_load(cursor_rcer(cursor), &nv, crumbs[1].page_id);
	if( result != BTREE_OK )
		goto end;

	if( sibling == CURSOR_SIBLING_LEFT )
	{
		if( crumbs[1].key_index.index == 0 )
		{
			result = BTREE_ERR_CURSOR_NO_SIBLING;
			goto undo;
		}

		cursor->current_key_index.mode = KLIM_INDEX;
		cursor->current_key_index.index = crumbs[1].key_index.index - 1;
		cursor->current_page_id = crumbs[1].page_id;
	}
	else
	{
		if( (crumbs[1].key_index.index + 1 == node_num_keys(nv_node(&nv)) &&
			 node_right_child(nv_node(&nv)) == 0) ||
			crumbs[1].key_index.mode != KLIM_INDEX )
		{
			result = BTREE_ERR_CURSOR_NO_SIBLING;
			goto undo;
		}

		if( crumbs[1].key_index.index + 1 == node_num_keys(nv_node(&nv)) )
		{
			cursor->current_key_index.mode = KLIM_RIGHT_CHILD;
			cursor->current_key_index.index = node_num_keys(nv_node(&nv));
		}
		else
		{
			cursor->current_key_index.mode = KLIM_INDEX;
			cursor->current_key_index.index = crumbs[1].key_index.index + 1;
		}

		cursor->current_page_id = crumbs[1].page_id;
	}

	result = cursor_push(cursor);
	if( result != BTREE_OK )
		goto end;

	// Read the page id.
	if( sibling == CURSOR_SIBLING_LEFT )
	{
		result =
			read_cell_page(cursor, nv_node(&nv), crumbs[1].key_index.index - 1);
		if( result != BTREE_OK )
			goto end;
	}
	else
	{
		result =
			read_cell_page(cursor, nv_node(&nv), crumbs[1].key_index.index + 1);
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
	noderc_release_n(cursor_rcer(cursor), 1, &nv);

	return result;

undo:
	restore_result = result;
	result = cursor_restore(cursor, crumbs, 2);
	if( result != BTREE_OK )
		goto end;
	result = restore_result;
	goto end;
}

enum btree_e
cursor_read_parent(struct Cursor* cursor, struct NodeView* out_view)
{
	enum btree_e result = BTREE_OK;
	struct CursorBreadcrumb crumb = {0};

	assert(out_view->page != NULL);

	result = cursor_pop(cursor, &crumb);
	if( result != BTREE_OK )
		return result;

	result = noderc_reinit_read(
		cursor_rcer(cursor), out_view, cursor->current_page_id);
	if( result != BTREE_OK )
		return result;

	result = cursor_push_crumb(cursor, &crumb);
	if( result != BTREE_OK )
		return result;

	return result;
}

// TODO: Deprecate this
enum btree_e
cursor_parent_index(struct Cursor* cursor, struct ChildListIndex* out_index)
{
	enum btree_e result = BTREE_OK;
	struct CursorBreadcrumb crumb;

	result = cursor_parent_crumb(cursor, &crumb);

	if( result == BTREE_OK )
		*out_index = crumb.key_index;

	return result;
}

enum btree_e
cursor_parent_crumb(struct Cursor* cursor, struct CursorBreadcrumb* out_crumb)
{
	enum btree_e result = BTREE_OK;
	struct CursorBreadcrumb child;

	result = cursor_pop(cursor, &child);
	if( result != BTREE_OK )
		goto end;

	result = cursor_peek(cursor, out_crumb);
	if( result != BTREE_OK )
		goto end;

end:
	result = cursor_push_crumb(cursor, &child);
	if( result != BTREE_OK )
		return result;

	return result;
}

struct BTreeNodeRC*
cursor_rcer(struct Cursor* cursor)
{
	return cursor->tree->rcer;
}

struct Pager*
cursor_pager(struct Cursor* cursor)
{
	return cursor->tree->pager;
}