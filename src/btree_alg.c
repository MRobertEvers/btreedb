#include "btree_alg.h"

#include "btree_cell.h"
#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_node_debug.h"
#include "btree_node_writer.h"
#include "btree_overflow.h"
#include "btree_utils.h"
#include "noderc.h"
#include "pager.h"
#include "serialization.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static void
read_key_cell(
	struct BTreeNode* source_node, unsigned int* buffer, unsigned int index)
{
	// TODO: Buffer size check
	struct CellData read_cell = {0};
	unsigned int key = 0;

	key = source_node->keys[index].key;
	btu_read_cell(source_node, index, &read_cell);

	ser_read_32bit_le(buffer, read_cell.pointer);
}

struct split_node_t
{
	unsigned int left_child_high_key;
};

static enum btree_e
split_node(
	struct BTreeNode* source_node,
	struct Pager* pager,
	struct BTreeNode* left,
	struct BTreeNode* right,
	struct split_node_t* split_result)
{
	split_result->left_child_high_key = 0;
	if( source_node->header->num_keys == 0 )
		return BTREE_OK;

	int first_half = ((source_node->header->num_keys + 1) / 2);
	// We need to keep track of this. If this is a nonleaf node,
	// then the left child high key will be lost.
	unsigned int left_child_high_key = source_node->keys[first_half - 1].key;

	for( int i = 0; i < first_half - 1; i++ )
	{
		btree_node_move_cell(source_node, left, i, pager);
	}

	if( !source_node->header->is_leaf )
		read_key_cell(source_node, &left->header->right_child, first_half - 1);
	else
		btree_node_move_cell(source_node, left, first_half - 1, pager);

	for( int i = first_half; i < source_node->header->num_keys; i++ )
	{
		btree_node_move_cell(source_node, right, i, pager);
	}

	// Non-leaf nodes also have to move right child too.
	if( !source_node->header->is_leaf )
		right->header->right_child = source_node->header->right_child;

	split_result->left_child_high_key = left_child_high_key;

	return BTREE_OK;
}

/**
 * See header for details.
 */
enum btree_e
bta_split_node_as_parent(
	struct BTreeNode* node,
	struct BTreeNodeRC* rcer,
	struct SplitPageAsParent* split_page)
{
	enum btree_e result = BTREE_OK;
	struct InsertionIndex insert_end = {.mode = KLIM_END};
	struct BTreeCellInline write_cell = {0};

	struct NodeView parent_nv = {0};
	struct NodeView left_nv = {0};
	struct NodeView right_nv = {0};

	result = noderc_acquire_load_n(
		rcer, 3, &parent_nv, 0, &left_nv, 0, &right_nv, 0);
	if( result != BTREE_OK )
		goto end;

	result = noderc_reinit_as(rcer, &parent_nv, node->page_number);
	if( result != BTREE_OK )
		goto end;

	struct split_node_t split_result = {0};
	result = split_node(
		node,
		nv_pager(&parent_nv),
		nv_node(&left_nv),
		nv_node(&right_nv),
		&split_result);
	if( result != BTREE_OK )
		goto end;

	// We need to write the pages out to get the page ids.
	bool is_leaf = node_is_leaf(node);
	node_is_leaf_set(nv_node(&left_nv), is_leaf);
	node_is_leaf_set(nv_node(&right_nv), is_leaf);
	result = noderc_persist_n(rcer, 2, &left_nv, &right_nv);
	if( result != BTREE_OK )
		goto end;

	// When splitting a leaf-node,
	// the right_child pointer becomes the right_page id
	// When splitting a non-leaf node
	// The right_child pointer becomes the right_page id
	// and the old right_child pointer becomes the right_child pointer
	// of the right page.
	node_is_leaf_set(nv_node(&parent_nv), false);
	node_right_child_set(nv_node(&parent_nv), nv_page(&right_nv)->page_id);

	// Add the middle point between the left and right pages as a key to the
	// parent.
	write_cell.inline_size = sizeof(nv_page(&left_nv)->page_id);
	write_cell.payload = &nv_page(&left_nv)->page_id;
	result = btree_node_insert_inline(
		nv_node(&parent_nv),
		&insert_end,
		split_result.left_child_high_key,
		&write_cell);
	if( result != BTREE_OK )
		goto end;

	result = btree_node_copy(node, nv_node(&parent_nv));
	if( result != BTREE_OK )
		goto end;

	result = btpage_err(pager_write_page(nv_pager(&parent_nv), node->page));
	if( result != BTREE_OK )
		goto end;

	if( split_page != NULL )
	{
		split_page->left_child_page_id = nv_page(&left_nv)->page_id;
		split_page->right_child_page_id = nv_page(&right_nv)->page_id;
		split_page->left_child_high_key = split_result.left_child_high_key;
	}

end:
	noderc_release_n(rcer, 3, &parent_nv, &left_nv, &right_nv);

	return BTREE_OK;
}

/**
 * See header for details.
 */
enum btree_e
bta_split_node(
	struct BTreeNode* node,
	struct BTreeNodeRC* rcer,
	struct SplitPage* split_page)
{
	enum btree_e result = BTREE_OK;
	struct NodeView left_nv = {0};
	struct NodeView right_nv = {0};

	result = noderc_acquire_load_n(rcer, 2, &left_nv, 0, &right_nv, 0);
	if( result != BTREE_OK )
		goto end;

	// We want right page to remain stable since pointers
	// in parent nodes are already pointing to the high-key of the input
	// node which becomes the high key of the right child.
	result = noderc_reinit_as(rcer, &right_nv, node->page_number);
	if( result != BTREE_OK )
		goto end;

	struct split_node_t split_result = {0};
	result = split_node(
		node,
		nv_pager(&left_nv),
		nv_node(&left_nv),
		nv_node(&right_nv),
		&split_result);
	if( result != BTREE_OK )
		goto end;

	bool is_leaf = node_is_leaf(node);
	node_is_leaf_set(nv_node(&left_nv), is_leaf);
	node_is_leaf_set(nv_node(&right_nv), is_leaf);
	result = noderc_persist_n(rcer, 2, &right_nv, &left_nv);
	if( result != BTREE_OK )
		goto end;

	result = btree_node_copy(node, nv_node(&right_nv));
	if( result != BTREE_OK )
		goto end;

	if( split_page != NULL )
	{
		split_page->left_page_id = nv_page(&left_nv)->page_id;
		split_page->left_page_high_key = split_result.left_child_high_key;
	}

end:
	noderc_release_n(rcer, 2, &left_nv, &right_nv);

	return result;
}

static u32
calc_heap_used(struct BTreeNode* node)
{
	return btree_node_calc_heap_capacity(node) - node->header->free_heap;
}

/**
 * @brief With the cursor pointing at the underflown node, this will rotate
 * right.
 *
 * Put the separator from the parent node into the right node, move a node from
 * the left node into the parent as the new separator.
 *
 * @param cursor
 * @return enum btree_e
 */
enum btree_e
bta_rotate(struct Cursor* cursor, enum bta_rebalance_mode_e mode)
{
	assert(
		mode == BTA_REBALANCE_MODE_ROTATE_LEFT ||
		mode == BTA_REBALANCE_MODE_ROTATE_RIGHT);
	enum btree_e result = BTREE_OK;
	struct NodeView source_nv = {0};
	struct NodeView dest_nv = {0};
	struct NodeView parent_nv = {0};
	result = noderc_acquire_load_n(
		cursor_rcer(cursor), 3, &source_nv, 0, &dest_nv, 0, &parent_nv, 0);
	if( result != BTREE_OK )
		goto end;

	result = cursor_read_parent(cursor, &parent_nv);
	if( result != BTREE_OK )
		goto end;

	result = noderc_reinit_read(
		cursor_rcer(cursor), &dest_nv, cursor->current_page_id);
	if( result != BTREE_OK )
		goto end;

	struct ChildListIndex parent_index = {0};
	if( mode == BTA_REBALANCE_MODE_ROTATE_LEFT )
	{
		// The cursor points to the left child.
		result = cursor_parent_index(cursor, &parent_index);
		if( result != BTREE_OK )
			goto end;

		result = cursor_sibling(cursor, CURSOR_SIBLING_RIGHT);
		if( result != BTREE_OK )
			goto end;
	}
	else
	{
		result = cursor_sibling(cursor, CURSOR_SIBLING_LEFT);
		if( result != BTREE_OK )
			goto end;

		result = cursor_parent_index(cursor, &parent_index);
		if( result != BTREE_OK )
			goto end;
	}

	result = noderc_reinit_read(
		cursor_rcer(cursor), &source_nv, cursor->current_page_id);
	if( result != BTREE_OK )
		goto end;

	u32 parent_key = node_key_at(nv_node(&parent_nv), parent_index.index);

	struct InsertionIndex dest_index;
	dest_index.index = mode == BTA_REBALANCE_MODE_ROTATE_RIGHT
						   ? 0
						   : node_num_keys(nv_node(&dest_nv));
	dest_index.mode = KLIM_INDEX;

	struct ChildListIndex source_index;
	source_index.index = mode == BTA_REBALANCE_MODE_ROTATE_RIGHT
							 ? node_num_keys(nv_node(&source_nv)) - 1
							 : 0;
	source_index.mode = KLIM_INDEX;

	struct BTreeNode* left_node = NULL;
	left_node = mode == BTA_REBALANCE_MODE_ROTATE_RIGHT ? nv_node(&source_nv)
														: nv_node(&dest_nv);

	if( node_is_leaf(nv_node(&dest_nv)) )
	{
		result = btree_node_move_cell_ex_to(
			nv_node(&source_nv),
			nv_node(&dest_nv),
			source_index.index,
			&dest_index, // Dest index
			// For leaf nodes, key remains the same.
			node_key_at(nv_node(&source_nv), source_index.index),
			cursor_pager(cursor));
		if( result != BTREE_OK )
			goto end;

		result = btree_node_remove(
			nv_node(&source_nv), &source_index, NULL, NULL, 0);
		if( result != BTREE_OK )
			goto end;

		// For leaf nodes, the parent key is always the last key of the left
		// child.
		u32 new_last_child_index = node_num_keys(left_node) - 1;
		u32 new_last_child_key = node_key_at(left_node, new_last_child_index);
		node_key_at_set(
			nv_node(&parent_nv), parent_index.index, new_last_child_key);
	}
	else
	{
		// If the dest node is not a leaf node, then the cell is guaranteed to
		// be inline containing the page id.
		u32 page_id_of_source_cell_child = 0;
		result = btree_node_read_inline_as_page(
			nv_node(&source_nv), &source_index, &page_id_of_source_cell_child);
		if( result != BTREE_OK )
			goto end;

		u32 prev_right_child_of_left_node = 0;

		prev_right_child_of_left_node = node_right_child(left_node);
		// Write the right child of the left node.
		node_right_child_set(left_node, page_id_of_source_cell_child);

		// Write the new cell pointing to the previous right child of the left
		// node.
		result = btree_node_write_ex(
			nv_node(&dest_nv),
			nv_pager(&dest_nv),
			&dest_index,
			parent_key,
			0, // ignored
			&prev_right_child_of_left_node,
			sizeof(prev_right_child_of_left_node),
			WRITER_EX_MODE_RAW);
		if( result != BTREE_OK )
			goto end;

		u32 source_key = node_key_at(nv_node(&source_nv), source_index.index);

		// The parent key becomes the key of the donated cell.
		node_key_at_set(nv_node(&parent_nv), parent_index.index, source_key);

		result = btree_node_remove(
			nv_node(&source_nv), &source_index, NULL, NULL, 0);
		if( result != BTREE_OK )
			goto end;
	}

	result = noderc_persist_n(
		cursor_rcer(cursor), 3, &source_nv, &dest_nv, &parent_nv);
	if( result != BTREE_OK )
		goto end;

end:
	noderc_release_n(cursor_rcer(cursor), 3, &source_nv, &dest_nv, &parent_nv);
	return result;
}

enum btree_e
bta_merge(struct Cursor* cursor, enum bta_rebalance_mode_e mode)
{
	assert(
		mode == BTA_REBALANCE_MODE_MERGE_LEFT ||
		mode == BTA_REBALANCE_MODE_MERGE_RIGHT);

	enum btree_e result = BTREE_OK;
	bool parent_deficient = false;
	struct NodeView left_nv = {0};
	struct NodeView right_nv = {0};
	struct NodeView parent_nv = {0};

	u32 left_page_id;
	u32 right_page_id;
	struct CursorBreadcrumb lparent_crumb = {0};
	// struct BTreeNode right_node = mode == BTA_REBALANCE_MODE_MERGE_RIGHT ?
	if( mode == BTA_REBALANCE_MODE_MERGE_RIGHT )
	{
		// The cursor points to the left child.
		left_page_id = cursor->current_page_id;

		result = cursor_parent_crumb(cursor, &lparent_crumb);
		if( result != BTREE_OK )
			goto end;

		result = cursor_sibling(cursor, CURSOR_SIBLING_RIGHT);
		if( result != BTREE_OK )
			goto end;

		right_page_id = cursor->current_page_id;
	}
	else
	{
		right_page_id = cursor->current_page_id;

		result = cursor_sibling(cursor, CURSOR_SIBLING_LEFT);
		if( result != BTREE_OK )
			goto end;

		result = cursor_parent_crumb(cursor, &lparent_crumb);
		if( result != BTREE_OK )
			goto end;

		left_page_id = cursor->current_page_id;
	}

	result = noderc_acquire_load_n(
		cursor_rcer(cursor),
		3,
		&left_nv,
		left_page_id,
		&right_nv,
		right_page_id,
		&parent_nv,
		lparent_crumb.page_id);
	if( result != BTREE_OK )
		goto end;

	u32 parent_key =
		node_key_at(nv_node(&parent_nv), lparent_crumb.key_index.index);

	if( node_is_leaf(nv_node(&left_nv)) )
	{
		// Just copy
		for( int i = 0; i < node_num_keys(nv_node(&right_nv)); i++ )
		{
			result = btree_node_move_cell(
				nv_node(&right_nv), nv_node(&left_nv), i, cursor_pager(cursor));
			if( result != BTREE_OK )
				goto end;
		}
	}
	else
	{
		// Slide parent
		u32 rchild_of_lnode = node_right_child(nv_node(&left_nv));

		struct InsertionIndex insert_end = {.mode = KLIM_END};
		result = btree_node_write_ex(
			nv_node(&left_nv),
			nv_pager(&left_nv),
			&insert_end,
			parent_key,
			0, // ignored
			&rchild_of_lnode,
			sizeof(rchild_of_lnode),
			WRITER_EX_MODE_RAW);
		if( result != BTREE_OK )
			goto end;

		for( int i = 0; i < node_num_keys(nv_node(&right_nv)); i++ )
		{
			result = btree_node_move_cell(
				nv_node(&right_nv), nv_node(&left_nv), i, cursor_pager(cursor));
			if( result != BTREE_OK )
				goto end;
		}

		node_right_child_set(
			nv_node(&left_nv), node_right_child(nv_node(&right_nv)));
	}

	// Remove left parent.
	result = btree_node_remove(
		nv_node(&parent_nv), &lparent_crumb.key_index, NULL, NULL, 0);
	if( result != BTREE_OK )
		goto end;

	// Result always ends up on the right-side page.
	result = btree_node_copy(nv_node(&right_nv), nv_node(&left_nv));
	if( result != BTREE_OK )
		goto end;

	parent_deficient =
		node_num_keys(nv_node(&parent_nv)) < cursor->tree->header.underflow;

	// TODO: Free list.
	result = noderc_persist_n(
		cursor_rcer(cursor), 3, &left_nv, &right_nv, &parent_nv);
	if( result != BTREE_OK )
		goto end;

end:
	noderc_release_n(cursor_rcer(cursor), 3, &left_nv, &right_nv, &parent_nv);

	if( result == BTREE_OK && parent_deficient )
		result = BTREE_ERR_PARENT_DEFICIENT;
	return result;
}

/**
 * @brief Checks if right sibling can
 *
 * Clobber current cursor index.
 *
 * @param cursor
 * @return enum btree_e
 */
enum btree_e
bta_check_sibling(struct Cursor* cursor, enum cursor_sibling_e sibling)
{
	enum btree_e result = BTREE_OK;
	enum btree_e check_result = BTREE_OK;
	struct NodeView nv = {0};
	struct CursorBreadcrumb crumbs[2] = {0};

	result = cursor_pop_n(cursor, crumbs, 2);
	if( result != BTREE_OK )
		goto end;

	result = cursor_sibling(cursor, sibling);
	if( result != BTREE_OK )
		goto restore;

	result =
		noderc_acquire_load(cursor_rcer(cursor), &nv, cursor->current_page_id);
	if( result != BTREE_OK )
		goto restore;

	if( node_num_keys(nv_node(&nv)) <= cursor->tree->header.underflow )
		result = BTREE_ERR_NODE_NOT_ENOUGH_SPACE;

restore:
	check_result = result;
	result = cursor_restore(cursor, crumbs, 2);
	if( result != BTREE_OK )
		goto end;

	result = check_result;
end:
	noderc_release(cursor_rcer(cursor), &nv);
	if( result == BTREE_OK || result == BTREE_ERR_CURSOR_NO_SIBLING )
		result = check_result;
	return result;
}

enum btree_e
bta_decide_rebalance_mode(
	struct Cursor* cursor, enum bta_rebalance_mode_e* out_mode)
{
	enum btree_e result = BTREE_OK;
	enum btree_e right_result = BTREE_OK;

	right_result = bta_check_sibling(cursor, CURSOR_SIBLING_RIGHT);
	result = right_result;
	if( result == BTREE_OK )
	{
		*out_mode = BTA_REBALANCE_MODE_ROTATE_LEFT;
		goto end;
	}

	if( result != BTREE_ERR_CURSOR_NO_SIBLING &&
		result != BTREE_ERR_NODE_NOT_ENOUGH_SPACE )
		goto end;

	result = bta_check_sibling(cursor, CURSOR_SIBLING_LEFT);
	if( result == BTREE_OK )
	{
		*out_mode = BTA_REBALANCE_MODE_ROTATE_RIGHT;
		goto end;
	}

	if( result != BTREE_ERR_CURSOR_NO_SIBLING &&
		result != BTREE_ERR_NODE_NOT_ENOUGH_SPACE )
		goto end;

	// TODO: Assert that at least one is present?
	assert(
		result != BTREE_ERR_CURSOR_NO_SIBLING ||
		right_result != BTREE_ERR_CURSOR_NO_SIBLING);

	// There is a left sibling; merge with it.
	if( result != BTREE_ERR_CURSOR_NO_SIBLING )
		*out_mode = BTA_REBALANCE_MODE_MERGE_LEFT;
	else
		*out_mode = BTA_REBALANCE_MODE_MERGE_RIGHT;

end:
	return result;
}

enum btree_e
bta_rebalance(struct Cursor* cursor)
{
	enum btree_e result = BTREE_OK;
	enum bta_rebalance_mode_e mode = BTA_REBALANCE_MODE_UNK;

	do
	{
		result = bta_decide_rebalance_mode(cursor, &mode);
		if( result == BTREE_ERR_CURSOR_NO_PARENT )
		{
			result = bta_rebalance_root(cursor);
			goto end;
		}

		switch( mode )
		{
		case BTA_REBALANCE_MODE_ROTATE_LEFT:
		case BTA_REBALANCE_MODE_ROTATE_RIGHT:
			result = bta_rotate(cursor, mode);
			break;
		case BTA_REBALANCE_MODE_MERGE_LEFT:
		case BTA_REBALANCE_MODE_MERGE_RIGHT:
			result = bta_merge(cursor, mode);
			break;
		default:
			result = BTREE_ERR_UNK;
			break;
		}

		if( result == BTREE_ERR_PARENT_DEFICIENT )
			if( cursor_pop(cursor, NULL) != BTREE_OK )
				goto end;

	} while( result == BTREE_ERR_PARENT_DEFICIENT );

end:
	return result;
}

/**
 * @brief Rebalance root is called when the root is underflown;
 *
 * The root only underflows when there is only one child. I.e.
 * regardless of any other underflow condition.
 *
 * If the root has any children more than 1, then this returns BTREE_OK.
 *
 * @param cursor
 * @return enum btree_e
 */
enum btree_e
bta_rebalance_root(struct Cursor* cursor)
{
	enum btree_e result = BTREE_OK;
	struct NodeView root_nv = {0};
	struct NodeView right_nv = {0};
	struct NodeView empty_nv = {0};

	result = noderc_acquire_load(
		cursor_rcer(cursor), &root_nv, cursor->tree->root_page_id);
	if( result != BTREE_OK )
		goto end;

	assert(!node_is_leaf(nv_node(&root_nv)));

	if( node_num_keys(nv_node(&root_nv)) > 0 )
		goto end;

	result = noderc_acquire_load(
		cursor_rcer(cursor), &right_nv, node_right_child(nv_node(&root_nv)));
	if( result != BTREE_OK )
		goto end;

	assert(node_is_leaf(nv_node(&right_nv)));

	// If there is enough room on the root to fit all the children,
	// then move all the data to the root.
	// Otherwise, we create an empty right child.
	if( calc_heap_used(nv_node(&right_nv)) >
		btree_node_calc_heap_capacity(nv_node(&root_nv)) )
	{
		// Create empty right child.
		result = noderc_acquire(cursor_rcer(cursor), &empty_nv);
		if( result != BTREE_OK )
			goto end;

		node_is_leaf_set(nv_node(&empty_nv), true);

		result = noderc_persist_n(cursor_rcer(cursor), 1, &empty_nv);
		if( result != BTREE_OK )
			goto end;

		assert(nv_page(&empty_nv)->page_id != 0);
		node_right_child_set(nv_node(&root_nv), nv_page(&empty_nv)->page_id);

		// Take the highest key of the child and use that as the new key for the
		// root
		u32 high_key = node_key_at(
			nv_node(&right_nv), node_num_keys(nv_node(&right_nv)) - 1);
		u32 right_page = nv_node(&right_nv)->page_number;
		struct InsertionIndex insert_end = {.mode = KLIM_END};
		result = btree_node_write_ex(
			nv_node(&root_nv),
			nv_pager(&root_nv),
			&insert_end,
			high_key,
			0,
			&right_page,
			sizeof(right_page),
			WRITER_EX_MODE_RAW);
		if( result != BTREE_OK )
			goto end;
	}
	else
	{
		// Shrink the tree.
		node_is_leaf_set(nv_node(&root_nv), true);

		result = btree_node_reset(nv_node(&root_nv));
		if( result != BTREE_OK )
			goto end;

		for( int i = 0; i < node_num_keys(nv_node(&right_nv)); i++ )
		{
			result = btree_node_move_cell(
				nv_node(&right_nv), nv_node(&root_nv), i, cursor_pager(cursor));
			if( result != BTREE_OK )
				goto end;
		}

		// TODO: free list.
	}

end:
	noderc_release_n(cursor_rcer(cursor), 3, &root_nv, &right_nv, &empty_nv);
	return result;
}