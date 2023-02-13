#include "ibtree_alg.h"

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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct split_node_t
{
	unsigned int left_child_index;
};

static enum btree_e
split_node(
	struct BTreeNode* source_node,
	struct Pager* pager,
	struct BTreeNode* left,
	struct BTreeNode* right,
	struct BTreeNode* holding_node,
	struct split_node_t* split_result)
{
	split_result->left_child_index = 0;
	if( source_node->header->num_keys == 0 )
		return BTREE_OK;

	int first_half = ((source_node->header->num_keys + 1) / 2);

	split_result->left_child_index = first_half - 1;

	for( int i = 0; i < first_half - 1; i++ )
	{
		btree_node_move_cell(source_node, left, i, pager);
	}

	// The last child on the left node is not included in ibtrees split. That
	// must go to the parent.
	if( first_half > 0 )
	{
		if( holding_node )
		{
			btree_node_move_cell(
				source_node, holding_node, first_half - 1, pager);
		}

		// The left node's right child becomes the page of the key that gets
		// promoted to the parent.
		left->header->right_child = source_node->keys[first_half - 1].key;
	}

	for( int i = first_half; i < source_node->header->num_keys; i++ )
	{
		btree_node_move_cell(source_node, right, i, pager);
	}

	// Non-leaf nodes also have to move right child too.
	if( !source_node->header->is_leaf )
		right->header->right_child = source_node->header->right_child;

	return BTREE_OK;
}

/**
 * See header for details.
 */
enum btree_e
ibta_split_node_as_parent(
	struct BTreeNode* node,
	struct BTreeNodeRC* rcer,
	struct SplitPageAsParent* split_page)
{
	enum btree_e result = BTREE_OK;

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
		NULL,
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

	result = btree_node_move_cell_ex(
		node,
		nv_node(&parent_nv),
		split_result.left_child_index,
		nv_page(&left_nv)->page_id,
		nv_pager(&left_nv));
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
		split_page->left_child_high_key = split_result.left_child_index;
	}

end:
	noderc_release_n(rcer, 3, &parent_nv, &left_nv, &right_nv);

	return BTREE_OK;
}

/**
 * See header for details.
 */
enum btree_e
ibta_split_node(
	struct BTreeNode* node,
	struct BTreeNodeRC* rcer,
	struct BTreeNode* holding_node,
	struct SplitPage* split_page)
{
	enum btree_e result = BTREE_OK;

	struct NodeView left_nv = {0};
	struct NodeView right_nv = {0};

	// We want right page to remain stable since pointers
	// in parent nodes are already pointing to the high-key of the input
	// node which becomes the high key of the right child.
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
		holding_node,
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
		split_page->left_page_high_key = split_result.left_child_index;
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

static struct InsertionIndex
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
static int
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

void
ibta_insert_at_init(struct ibta_insert_at* insert_at, void* data, u32 data_size)
{
	ibta_insert_at_init_ex(
		insert_at, data, data_size, 0, 0, WRITER_EX_MODE_RAW);
}

void
ibta_insert_at_init_ex(
	struct ibta_insert_at* insert_at,
	void* data,
	u32 data_size,
	u32 flags,
	u32 key,
	enum writer_ex_mode_e mode)
{
	insert_at->data = data;
	insert_at->data_size = data_size;
	insert_at->flags = flags;
	insert_at->key = key;
	insert_at->mode = mode;
}

enum btree_e
ibta_insert_at(struct Cursor* cursor, struct ibta_insert_at* insert_at)
{
	assert(
		cursor->tree->compare != NULL && cursor->tree->reset_compare != NULL);

	enum btree_e result = BTREE_OK;

	int page_index_as_key =
		insert_at->key; // This is only not zero when there is a page split.
	u32 flags = insert_at->flags; // Only used when writing to parent.
	enum writer_ex_mode_e writer_mode = insert_at->mode;
	byte* payload = insert_at->data;
	u32 payload_size = insert_at->data_size;

	struct NodeView nv = {0};
	struct NodeView holding_one_nv = {0};
	struct NodeView holding_two_nv = {0};

	struct NodeView* next_holding_nv = &holding_one_nv;

	struct CursorBreadcrumb crumb = {0};

	result = noderc_acquire_load_n(
		cursor_rcer(cursor), 3, &nv, 0, &holding_one_nv, 0, &holding_two_nv, 0);
	if( result != BTREE_OK )
		goto end;

	while( 1 )
	{
		result = cursor_pop(cursor, &crumb);
		if( result != BTREE_OK )
			goto end;

		result = noderc_reinit_read(cursor_rcer(cursor), &nv, crumb.page_id);
		if( result != BTREE_OK )
			goto end;

		struct InsertionIndex index = from_cli(&crumb.key_index);

		result = btree_node_write_ex(
			nv_node(&nv),
			cursor_pager(cursor),
			&index,
			page_index_as_key,
			flags, // Nonzero only on split.
			payload,
			payload_size,
			writer_mode);
		if( result == BTREE_ERR_NODE_NOT_ENOUGH_SPACE )
		{
			// TODO: This first half thing is shaky.
			u32 first_half = (node_num_keys(nv_node(&nv)) + 1) / 2;
			int child_insertion = left_or_right_insertion(&index, nv_node(&nv));
			if( nv_page(&nv)->page_id == cursor->tree->root_page_id )
			{
				struct SplitPageAsParent split_result;
				result = ibta_split_node_as_parent(
					nv_node(&nv), cursor_rcer(cursor), &split_result);
				if( result != BTREE_OK )
					goto end;

				result = noderc_reinit_read(
					cursor_rcer(cursor),
					&nv,
					child_insertion == -1 ? split_result.left_child_page_id
										  : split_result.right_child_page_id);
				if( result != BTREE_OK )
					goto end;

				if( child_insertion == 1 )
					index.index -= first_half;

				result = btree_node_write_ex(
					nv_node(&nv),
					cursor_pager(cursor),
					&index,
					page_index_as_key,
					flags,
					payload,
					payload_size,
					writer_mode);

				if( result != BTREE_OK )
					goto end;

				goto end;
			}
			else
			{
				result = btree_node_reset(nv_node(next_holding_nv));
				if( result != BTREE_OK )
					goto end;

				struct SplitPage split_result;
				result = ibta_split_node(
					nv_node(&nv),
					cursor_rcer(cursor),
					nv_node(next_holding_nv),
					&split_result);
				if( result != BTREE_OK )
					goto end;

				// TODO: Key compare function.
				if( child_insertion == -1 )
				{
					result = noderc_reinit_read(
						cursor_rcer(cursor), &nv, split_result.left_page_id);
					if( result != BTREE_OK )
						goto end;
				}

				if( child_insertion == 1 )
					index.index -= first_half;

				// Write the input payload to the correct child.
				result = btree_node_write_ex(
					nv_node(&nv),
					cursor_pager(cursor),
					&index,
					page_index_as_key,
					flags,
					payload,
					payload_size,
					writer_mode);
				if( result != BTREE_OK )
					goto end;

				// Now we need to write the holding key.
				result = cursor_pop(cursor, &crumb);
				if( result != BTREE_OK )
					goto end;

				crumb.key_index.mode = KLIM_INDEX;
				result = cursor_push_crumb(cursor, &crumb);
				if( result != BTREE_OK )
					goto end;

				writer_mode = WRITER_EX_MODE_CELL_MOVE;
				flags = nv_node(next_holding_nv)->keys[0].flags;
				page_index_as_key = split_result.left_page_id;

				payload = btu_get_cell_buffer(nv_node(next_holding_nv), 0);
				payload_size =
					btu_get_cell_buffer_size(nv_node(next_holding_nv), 0);

				next_holding_nv = next_holding_nv == &holding_one_nv
									  ? &holding_two_nv
									  : &holding_one_nv;
			}
		}
		else
		{
			break;
		}
	}

end:
	noderc_release_n(
		cursor_rcer(cursor), 3, &nv, &holding_one_nv, &holding_two_nv);
	return result;
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
ibta_rotate(struct Cursor* cursor, enum bta_rebalance_mode_e mode)
{
	assert(
		mode == BTA_REBALANCE_MODE_ROTATE_LEFT ||
		mode == BTA_REBALANCE_MODE_ROTATE_RIGHT);
	enum btree_e result = BTREE_OK;
	struct NodeView nv = {0};
	struct NodeView parent_nv = {0};

	result = noderc_acquire_load_n(
		cursor_rcer(cursor), 2, &parent_nv, 0, &nv, cursor->current_page_id);
	if( result != BTREE_OK )
		goto end;

	result = cursor_read_parent(cursor, &parent_nv);
	if( result != BTREE_OK )
		goto end;

	struct ChildListIndex parent_index = {0};
	result = cursor_parent_index(cursor, &parent_index);
	if( result != BTREE_OK )
		goto end;

	if( mode == BTA_REBALANCE_MODE_ROTATE_RIGHT )
	{
		if( parent_index.mode == KLIM_RIGHT_CHILD )
			parent_index.index = node_num_keys(nv_node(&parent_nv)) - 1;
		else
			parent_index.index -= 1;
	}
	parent_index.mode = KLIM_INDEX;

	struct InsertionIndex insert_lowered_index = {0};
	if( mode == BTA_REBALANCE_MODE_ROTATE_RIGHT )
	{
		insert_lowered_index.mode = KLIM_INDEX;
		insert_lowered_index.index = 0;
	}
	else
	{
		insert_lowered_index.mode = KLIM_END;
		insert_lowered_index.index = node_num_keys(nv_node(&nv));
	}
	result = btree_node_move_cell_ex_to(
		nv_node(&parent_nv),
		nv_node(&nv),
		parent_index.index,
		&insert_lowered_index,
		// For rotate right, this will eventually be updated to
		// point to the prev r-most of the left node.
		// For rotate left, this will point to the child of the prev
		// leftmost node.
		0,
		cursor->tree->pager);
	if( result != BTREE_OK )
		goto end;

	result = noderc_persist_n(cursor_rcer(cursor), 1, &nv);
	if( result != BTREE_OK )
		goto end;

	result =
		btree_node_remove(nv_node(&parent_nv), &parent_index, NULL, NULL, 0);
	if( result != BTREE_OK )
		goto end;

	u32 source_node_page_id = nv_node(&nv)->page_number;

	// Save on a write here because the parent stays in memory.
	// result =
	// 	btpage_err(pager_write_page(cursor->tree->pager, parent_node.page));
	// if( result != BTREE_OK )
	// 	goto end;

	// Now move the highest element from the left child to the parent.
	result = cursor_sibling(
		cursor,
		mode == BTA_REBALANCE_MODE_ROTATE_RIGHT ? CURSOR_SIBLING_LEFT
												: CURSOR_SIBLING_RIGHT);
	if( result != BTREE_OK )
		goto end;

	result =
		noderc_reinit_read(cursor_rcer(cursor), &nv, cursor->current_page_id);
	if( result != BTREE_OK )
		goto end;

	u32 source_index_number = 0;
	if( mode == BTA_REBALANCE_MODE_ROTATE_RIGHT )
	{
		source_index_number = node_num_keys(nv_node(&nv)) - 1;
	}
	else
	{
		source_index_number = 0;
	}

	struct InsertionIndex insert_elevated_index = {0};
	insert_elevated_index.index = parent_index.index;
	insert_elevated_index.mode =
		parent_index.mode == KLIM_RIGHT_CHILD ? KLIM_END : KLIM_INDEX;
	// We always want to point to the left child.
	u32 orphaned_page_id = mode == BTA_REBALANCE_MODE_ROTATE_RIGHT
							   ? cursor->current_page_id
							   : source_node_page_id;
	result = btree_node_move_cell_ex_to(
		nv_node(&nv),
		nv_node(&parent_nv),
		source_index_number,
		&insert_elevated_index,
		orphaned_page_id,
		cursor->tree->pager);
	if( result != BTREE_OK )
		goto end;

	result = noderc_persist_n(cursor_rcer(cursor), 1, &parent_nv);
	if( result != BTREE_OK )
		goto end;

	// Keep track of the child that was previously pointed to by the elevated
	// cell.
	// On rotate right, node points to left sibling. (Elevated cell side.)
	// On rotate left, node points to right sibling.
	u32 orphaned_child_page = node_key_at(nv_node(&nv), source_index_number);

	if( mode == BTA_REBALANCE_MODE_ROTATE_RIGHT )
	{
		u32 prev_rightmost_of_left_node = node_right_child(nv_node(&nv));

		node_right_child_set(nv_node(&nv), orphaned_child_page);

		// Need to update the lowered cell to point to this.
		orphaned_child_page = prev_rightmost_of_left_node;
	}

	struct ChildListIndex source_index;
	source_index.index = source_index_number;
	source_index.mode = KLIM_INDEX;
	result = btree_node_remove(nv_node(&nv), &source_index, NULL, NULL, 0);
	if( result != BTREE_OK )
		goto end;

	result = noderc_persist_n(cursor_rcer(cursor), 1, &nv);
	if( result != BTREE_OK )
		goto end;

	// Now we go back to the previously deficient page and update it to point to
	// the correct children.
	result = cursor_sibling(
		cursor,
		mode == BTA_REBALANCE_MODE_ROTATE_RIGHT ? CURSOR_SIBLING_RIGHT
												: CURSOR_SIBLING_LEFT);
	if( result != BTREE_OK )
		goto end;

	result =
		noderc_reinit_read(cursor_rcer(cursor), &nv, cursor->current_page_id);
	if( result != BTREE_OK )
		goto end;

	if( mode == BTA_REBALANCE_MODE_ROTATE_RIGHT )
	{
		// Need to update the lowered cell to point to
		node_key_at_set(
			nv_node(&nv), insert_lowered_index.index, orphaned_child_page);
	}
	else
	{
		u32 prev_rightmost_of_left_node = node_right_child(nv_node(&nv));

		// Update the lowered cell in the left node to point to
		// child of elevated cell.
		node_right_child_set(nv_node(&nv), orphaned_child_page);

		node_key_at_set(
			nv_node(&nv),
			insert_lowered_index.index,
			prev_rightmost_of_left_node);
	}

	result = noderc_persist_n(cursor_rcer(cursor), 1, &nv);
	if( result != BTREE_OK )
		goto end;

end:
	noderc_release_n(cursor_rcer(cursor), 2, &parent_nv, &nv);
	return result;
}

enum btree_e
ibta_merge(struct Cursor* cursor, enum bta_rebalance_mode_e mode)
{
	assert(
		mode == BTA_REBALANCE_MODE_MERGE_LEFT ||
		mode == BTA_REBALANCE_MODE_MERGE_RIGHT);

	enum btree_e result = BTREE_OK;

	// Merge left
	// sandwich left parent.
	// Parent cell points to the right child of the left node.

	struct NodeView parent_nv = {0};
	struct NodeView left_nv = {0};
	struct NodeView right_nv = {0};

	result = noderc_acquire_load_n(
		cursor_rcer(cursor), 3, &parent_nv, 0, &left_nv, 0, &right_nv, 0);
	if( result != BTREE_OK )
		goto end;

	result = cursor_read_parent(cursor, &parent_nv);
	if( result != BTREE_OK )
		goto end;

	struct ChildListIndex parent_index = {0};
	if( mode == BTA_REBALANCE_MODE_MERGE_RIGHT )
	{
		// The cursor points to the left child.
		result = noderc_reinit_read(
			cursor_rcer(cursor), &left_nv, cursor->current_page_id);
		if( result != BTREE_OK )
			goto end;

		result = cursor_parent_index(cursor, &parent_index);
		if( result != BTREE_OK )
			goto end;

		result = cursor_sibling(cursor, CURSOR_SIBLING_RIGHT);
		if( result != BTREE_OK )
			goto end;

		result = noderc_reinit_read(
			cursor_rcer(cursor), &right_nv, cursor->current_page_id);
		if( result != BTREE_OK )
			goto end;
	}
	else
	{
		result = noderc_reinit_read(
			cursor_rcer(cursor), &right_nv, cursor->current_page_id);
		if( result != BTREE_OK )
			goto end;

		result = cursor_sibling(cursor, CURSOR_SIBLING_LEFT);
		if( result != BTREE_OK )
			goto end;

		result = noderc_reinit_read(
			cursor_rcer(cursor), &left_nv, cursor->current_page_id);
		if( result != BTREE_OK )
			goto end;

		result = cursor_parent_index(cursor, &parent_index);
		if( result != BTREE_OK )
			goto end;
	}

	result = btree_node_move_cell_ex(
		nv_node(&parent_nv),
		nv_node(&left_nv),
		parent_index.index,
		node_right_child(nv_node(&left_nv)),
		cursor->tree->pager);
	if( result != BTREE_OK )
		goto end;

	result =
		btree_node_remove(nv_node(&parent_nv), &parent_index, NULL, NULL, 0);
	if( result != BTREE_OK )
		goto end;

	result = noderc_persist_n(cursor_rcer(cursor), 2, &left_nv, &parent_nv);
	if( result != BTREE_OK )
		goto end;

	char parent_deficient =
		node_num_keys(nv_node(&parent_nv)) < btree_underflow_lim(cursor->tree);
	for( int i = 0; i < node_num_keys(nv_node(&right_nv)); i++ )
	{
		result = btree_node_move_cell(
			nv_node(&right_nv), nv_node(&left_nv), i, cursor->tree->pager);
		if( result != BTREE_OK )
			goto end;
	}
	node_right_child_set(
		nv_node(&left_nv), node_right_child(nv_node(&right_nv)));

	result = btree_node_copy(nv_node(&right_nv), nv_node(&left_nv));
	if( result != BTREE_OK )
		goto end;

	result = noderc_persist_n(cursor_rcer(cursor), 1, &right_nv);
	if( result != BTREE_OK )
		goto end;

	// Free List
	result =
		btpage_err(pager_free_page(cursor_pager(cursor), nv_page(&right_nv)));
	if( result != BTREE_OK )
		goto end;

end:
	noderc_release_n(cursor_rcer(cursor), 3, &parent_nv, &left_nv, &right_nv);

	if( result == BTREE_OK && parent_deficient )
		result = BTREE_ERR_PARENT_DEFICIENT;
	return result;
}

enum btree_e
ibta_rebalance(struct Cursor* cursor)
{
	enum btree_e result = BTREE_OK;
	enum bta_rebalance_mode_e mode = BTA_REBALANCE_MODE_UNK;

	do
	{
		result = bta_decide_rebalance_mode(cursor, &mode);
		if( result == BTREE_ERR_CURSOR_NO_PARENT )
		{
			// Can't rebalance root.
			// TODO: Split child and populate root.
			result = ibta_rebalance_root(cursor);
			goto end;
		}

		switch( mode )
		{
		case BTA_REBALANCE_MODE_ROTATE_LEFT:
		case BTA_REBALANCE_MODE_ROTATE_RIGHT:
			result = ibta_rotate(cursor, mode);
			break;
		case BTA_REBALANCE_MODE_MERGE_LEFT:
		case BTA_REBALANCE_MODE_MERGE_RIGHT:
			result = ibta_merge(cursor, mode);
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

enum btree_e
ibta_rebalance_root(struct Cursor* cursor)
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
	u32 heap_used = calc_heap_used(nv_node(&right_nv));
	u32 heap_cap = btree_node_calc_heap_capacity(nv_node(&root_nv));
	if( heap_used > heap_cap )
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

		// Take the highest key of the child and use that as the new key for
		// the root
		u32 right_page = nv_node(&right_nv)->page_number;
		struct ChildListIndex remove_index = {
			.mode = KLIM_INDEX, .index = node_num_keys(nv_node(&right_nv)) - 1};
		result = btree_node_move_cell_ex(
			nv_node(&right_nv),
			nv_node(&root_nv),
			remove_index.index,
			right_page,
			cursor_pager(cursor));
		if( result != BTREE_OK )
			goto end;

		result =
			btree_node_remove(nv_node(&right_nv), &remove_index, NULL, NULL, 0);
		if( result != BTREE_OK )
			goto end;
	}
	else
	{
		// Shrink the tree.

		result = btree_node_reset(nv_node(&root_nv));
		if( result != BTREE_OK )
			goto end;

		node_is_leaf_set(nv_node(&root_nv), true);

		for( int i = 0; i < node_num_keys(nv_node(&right_nv)); i++ )
		{
			result = btree_node_move_cell(
				nv_node(&right_nv), nv_node(&root_nv), i, cursor_pager(cursor));
			if( result != BTREE_OK )
				goto end;
		}

		// Free List
		result = btpage_err(
			pager_free_page(cursor_pager(cursor), nv_page(&right_nv)));
		if( result != BTREE_OK )
			goto end;
	}

	result = noderc_persist_n(cursor_rcer(cursor), 2, &root_nv, &right_nv);
	if( result != BTREE_OK )
		goto end;
end:
	noderc_release_n(cursor_rcer(cursor), 3, &root_nv, &right_nv, &empty_nv);
	return result;
}