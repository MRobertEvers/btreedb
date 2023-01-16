#include "ibtree_alg.h"

#include "btree_cell.h"
#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_node_debug.h"
#include "btree_node_writer.h"
#include "btree_overflow.h"
#include "btree_utils.h"
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
	struct Pager* pager,
	struct SplitPageAsParent* split_page)
{
	enum btree_e result = BTREE_OK;
	struct InsertionIndex insert_end = {.mode = KLIM_END};
	struct BTreeCellInline write_cell = {0};

	struct BTreeNode* parent = NULL;
	struct BTreeNode* left = NULL;
	struct BTreeNode* right = NULL;

	struct Page* parent_page = NULL;
	struct Page* left_page = NULL;
	struct Page* right_page = NULL;

	result = btpage_err(page_create(pager, &parent_page));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_create_as_page_number(
		&parent, node->page_number, parent_page);
	if( result != BTREE_OK )
		goto end;

	result = btpage_err(page_create(pager, &left_page));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_create_from_page(&left, left_page);
	if( result != BTREE_OK )
		goto end;

	result = btpage_err(page_create(pager, &right_page));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_create_from_page(&right, right_page);
	if( result != BTREE_OK )
		goto end;

	struct split_node_t split_result = {0};
	result = split_node(node, pager, left, right, NULL, &split_result);
	if( result != BTREE_OK )
		goto end;

	// We need to write the pages out to get the page ids.
	left->header->is_leaf = node->header->is_leaf;
	result = btpage_err(pager_write_page(pager, left_page));
	if( result != BTREE_OK )
		goto end;

	right->header->is_leaf = node->header->is_leaf;
	result = btpage_err(pager_write_page(pager, right_page));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_move_cell_ex(
		node, parent, split_result.left_child_index, left_page->page_id, pager);
	if( result != BTREE_OK )
		goto end;

	// When splitting a leaf-node,
	// the right_child pointer becomes the right_page id
	// When splitting a non-leaf node
	// The right_child pointer becomes the right_page id
	// and the old right_child pointer becomes the right_child pointer
	// of the right page.
	parent->header->is_leaf = 0;
	parent->header->right_child = right_page->page_id;

	result = btree_node_copy(node, parent);
	if( result != BTREE_OK )
		goto end;

	result = btpage_err(pager_write_page(pager, node->page));
	if( result != BTREE_OK )
		goto end;

	if( split_page != NULL )
	{
		split_page->left_child_page_id = left_page->page_id;
		split_page->right_child_page_id = right_page->page_id;
		split_page->left_child_high_key = split_result.left_child_index;
	}

end:
	if( left )
		btree_node_destroy(left);
	if( left_page )
		page_destroy(pager, left_page);

	if( right )
		btree_node_destroy(right);
	if( right_page )
		page_destroy(pager, right_page);

	if( parent )
		btree_node_destroy(parent);
	if( parent_page )
		page_destroy(pager, parent_page);

	return BTREE_OK;
}

/**
 * See header for details.
 */
enum btree_e
ibta_split_node(
	struct BTreeNode* node,
	struct Pager* pager,
	struct BTreeNode* holding_node,
	struct SplitPage* split_page)
{
	enum btree_e result = BTREE_OK;
	enum pager_e page_result = PAGER_OK;

	struct BTreeNode* left = NULL;
	struct BTreeNode* right = NULL;

	struct Page* left_page = NULL;
	struct Page* right_page = NULL;

	// We want right page to remain stable since pointers
	// in parent nodes are already pointing to the high-key of the input
	// node which becomes the high key of the right child.
	result = btpage_err(page_create(pager, &right_page));
	if( result != BTREE_OK )
		goto end;

	result =
		btree_node_create_as_page_number(&right, node->page_number, right_page);
	if( result != BTREE_OK )
		goto end;

	result = btpage_err(page_create(pager, &left_page));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_create_from_page(&left, left_page);
	if( result != BTREE_OK )
		goto end;

	struct split_node_t split_result = {0};
	result = split_node(node, pager, left, right, holding_node, &split_result);
	if( result != BTREE_OK )
		goto end;

	right->header->is_leaf = node->header->is_leaf;
	left->header->is_leaf = node->header->is_leaf;
	// We need to write the pages out to get the page ids.
	result = btpage_err(pager_write_page(pager, right_page));
	if( result != BTREE_OK )
		goto end;
	// Write out the input page.
	result = btpage_err(pager_write_page(pager, left_page));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_copy(node, right);
	if( result != BTREE_OK )
		goto end;

	if( split_page != NULL )
	{
		split_page->left_page_id = left_page->page_id;
		split_page->left_page_high_key = split_result.left_child_index;
	}

end:
	if( left )
		btree_node_destroy(left);
	if( left_page )
		page_destroy(pager, left_page);

	if( right )
		btree_node_destroy(right);
	if( right_page )
		page_destroy(pager, right_page);

	return result;
}

static u32
calc_heap_used(struct BTreeNode* node)
{
	return btree_node_calc_heap_capacity(node) - node->header->free_heap;
}

// /**
//  * See header for details.
//  */
// enum btree_e
// ibta_merge_nodes(
// 	struct BTreeNode* stable_node,
// 	struct BTreeNode* other_node,
// 	struct Pager* pager,
// 	struct MergedPage* merged_page)
// {
// 	enum btree_e result = BTREE_OK;
// 	// TODO: Assert here?
// 	if( stable_node->header->is_leaf != other_node->header->is_leaf )
// 		return BTREE_ERR_CANNOT_MERGE;

// 	// Calculate the smallest amount of space that all the cells
// 	// from other_node could take up if they were overflow nodes.
// 	u32 min_reasonable_size_per_cell = btree_node_heap_required_for_insertion(
// 		btree_cell_overflow_min_disk_size());
// 	u32 min_size_required =
// 		min_reasonable_size_per_cell * other_node->header->num_keys;

// 	if( stable_node->header->free_heap < min_size_required )
// 		return BTREE_ERR_NODE_NOT_ENOUGH_SPACE;

// 	struct InsertionIndex index = {0};
// 	for( int i = 0; i < other_node->header->num_keys; i++ )
// 	{
// 		result = copy_cell_with_overflow(other_node, stable_node, i, pager);
// 		if( result != BTREE_OK )
// 			break;
// 	}

// 	if( result == BTREE_OK )
// 		result = btpage_err(pager_write_page(pager, stable_node->page));

// 	return result;
// }

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
	struct BTree* tree = cursor->tree;
	assert(tree->compare != NULL);

	enum btree_e result = BTREE_OK;
	char found = 0;
	int page_index_as_key =
		insert_at->key; // This is only not zero when there is a page split.
	u32 flags = insert_at->flags; // Only used when writing to parent.
	enum writer_ex_mode_e writer_mode = insert_at->mode;
	byte* payload = insert_at->data;
	u32 payload_size = insert_at->data_size;
	struct Page* page = NULL;
	struct Page* holding_page_one = NULL;
	struct Page* holding_page_two = NULL;
	struct PageSelector selector = {0};
	struct BTreeNode node = {0};
	struct BTreeNode holding_node_one = {0};
	struct BTreeNode holding_node_two = {0};

	struct BTreeNode* next_holding_node = &holding_node_one;

	struct CursorBreadcrumb crumb = {0};

	// Holding page and node is required to move cells up the tree.
	result = btpage_err(page_create(tree->pager, &holding_page_one));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_init_from_page(&holding_node_one, holding_page_one);
	if( result != BTREE_OK )
		goto end;

	result = btpage_err(page_create(tree->pager, &holding_page_two));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_init_from_page(&holding_node_two, holding_page_two);
	if( result != BTREE_OK )
		goto end;

	result = btpage_err(page_create(tree->pager, &page));
	if( result != BTREE_OK )
		goto end;

	while( 1 )
	{
		result = cursor_pop(cursor, &crumb);
		if( result != BTREE_OK )
			goto end;

		result =
			btree_node_init_from_read(&node, page, tree->pager, crumb.page_id);
		if( result != BTREE_OK )
			goto end;

		struct InsertionIndex index = from_cli(&crumb.key_index);

		result = btree_node_write_ex(
			&node,
			tree->pager,
			&index,
			page_index_as_key,
			flags, // Nonzero only on split.
			payload,
			payload_size,
			writer_mode);
		if( result == BTREE_ERR_NODE_NOT_ENOUGH_SPACE )
		{
			if( node.page->page_id == 1 )
			{
				// TODO: This first half thing is shaky.
				u32 first_half = (node.header->num_keys + 1) / 2;
				int child_insertion = left_or_right_insertion(&index, &node);

				struct SplitPageAsParent split_result;
				result = ibta_split_node_as_parent(
					&node, tree->pager, &split_result);
				if( result != BTREE_OK )
					goto end;

				pager_reselect(
					&selector,
					child_insertion == -1 ? split_result.left_child_page_id
										  : split_result.right_child_page_id);

				result =
					btpage_err(pager_read_page(tree->pager, &selector, page));
				if( result != BTREE_OK )
					goto end;

				result = btree_node_init_from_page(&node, page);
				if( result != BTREE_OK )
					goto end;

				if( child_insertion == 1 )
					index.index -= first_half;

				result = btree_node_write_ex(
					&node,
					tree->pager,
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
				u32 first_half = (node.header->num_keys + 1) / 2;
				int child_insertion = left_or_right_insertion(&index, &node);

				memset(
					next_holding_node->page->page_buffer,
					0x00,
					next_holding_node->page->page_size);
				btree_node_init_from_page(
					next_holding_node, next_holding_node->page);

				struct SplitPage split_result;
				result = ibta_split_node(
					&node, tree->pager, next_holding_node, &split_result);
				if( result != BTREE_OK )
					goto end;

				// TODO: Key compare function.
				if( child_insertion == -1 )
				{
					pager_reselect(&selector, split_result.left_page_id);
					pager_read_page(tree->pager, &selector, page);
					btree_node_init_from_page(&node, page);
				}

				if( child_insertion == 1 )
					index.index -= first_half;

				// Write the input payload to the correct child.
				result = btree_node_write_ex(
					&node,
					tree->pager,
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
				flags = next_holding_node->keys[0].flags;
				page_index_as_key = split_result.left_page_id;

				payload = btu_get_cell_buffer(next_holding_node, 0);
				payload_size = btu_get_cell_buffer_size(next_holding_node, 0);

				next_holding_node = next_holding_node == &holding_node_one
										? &holding_node_two
										: &holding_node_one;
			}
		}
		else
		{
			break;
		}
	}

end:
	if( page )
		page_destroy(tree->pager, page);
	if( holding_page_one )
		page_destroy(tree->pager, holding_page_one);
	if( holding_page_two )
		page_destroy(tree->pager, holding_page_two);
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
static enum btree_e
check_sibling(struct Cursor* cursor, enum cursor_sibling_e sibling)
{
	enum btree_e result = BTREE_OK;
	enum btree_e check_result = BTREE_OK;
	struct Page* page = NULL;
	struct BTreeNode node = {0};

	result = btpage_err(page_create(cursor->tree->pager, &page));
	if( result != BTREE_OK )
		goto end;

	result = cursor_sibling(cursor, sibling);
	if( result != BTREE_OK )
		goto end;

	result = btree_node_init_from_read(
		&node, page, cursor->tree->pager, cursor->current_page_id);
	if( result != BTREE_OK )
		goto end;

	if( node.header->num_keys <= 1 )
		check_result = BTREE_ERR_NODE_NOT_ENOUGH_SPACE;

	result = cursor_sibling(
		cursor,
		sibling == CURSOR_SIBLING_LEFT ? CURSOR_SIBLING_RIGHT
									   : CURSOR_SIBLING_LEFT);
	if( result != BTREE_OK )
		goto end;

end:
	if( page )
		page_destroy(cursor->tree->pager, page);
	if( result == BTREE_OK )
		result = check_result;
	return result;
}

enum btree_e
decide_rebalance_mode(struct Cursor* cursor, enum rebalance_mode_e* out_mode)
{
	enum btree_e result = BTREE_OK;
	enum btree_e right_result = BTREE_OK;

	right_result = check_sibling(cursor, CURSOR_SIBLING_RIGHT);
	result = right_result;
	if( result == BTREE_OK )
	{
		*out_mode = REBALANCE_MODE_ROTATE_LEFT;
		goto end;
	}

	if( result != BTREE_ERR_CURSOR_NO_SIBLING &&
		result != BTREE_ERR_NODE_NOT_ENOUGH_SPACE )
		goto end;

	result = check_sibling(cursor, CURSOR_SIBLING_LEFT);
	if( result == BTREE_OK )
	{
		*out_mode = REBALANCE_MODE_ROTATE_RIGHT;
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
		*out_mode = REBALANCE_MODE_MERGE_LEFT;
	else
		*out_mode = REBALANCE_MODE_MERGE_RIGHT;

end:
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
ibta_rotate(struct Cursor* cursor, enum rebalance_mode_e mode)
{
	assert(
		mode == REBALANCE_MODE_ROTATE_LEFT ||
		mode == REBALANCE_MODE_ROTATE_RIGHT);
	enum btree_e result = BTREE_OK;
	struct Page* page = NULL;
	struct Page* parent_page = NULL;
	struct BTreeNode node = {0};
	struct BTreeNode parent_node = {0};

	result = btpage_err(page_create(cursor->tree->pager, &page));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_init_from_read(
		&node, page, cursor->tree->pager, cursor->current_page_id);
	if( result != BTREE_OK )
		goto end;

	result = btpage_err(page_create(cursor->tree->pager, &parent_page));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_init_from_page(&parent_node, parent_page);
	if( result != BTREE_OK )
		goto end;

	result = cursor_read_parent(cursor, &parent_node);
	if( result != BTREE_OK )
		goto end;

	struct ChildListIndex parent_index = {0};
	result = cursor_parent_index(cursor, &parent_index);
	if( result != BTREE_OK )
		goto end;

	if( mode == REBALANCE_MODE_ROTATE_RIGHT )
	{
		if( parent_index.mode == KLIM_RIGHT_CHILD )
			parent_index.index = parent_node.header->num_keys - 1;
		else
			parent_index.index -= 1;
	}
	parent_index.mode = KLIM_INDEX;

	struct InsertionIndex insert_lowered_index = {0};
	if( mode == REBALANCE_MODE_ROTATE_RIGHT )
	{
		insert_lowered_index.mode = KLIM_INDEX;
		insert_lowered_index.index = 0;
	}
	else
	{
		insert_lowered_index.mode = KLIM_END;
		insert_lowered_index.index = node.header->num_keys;
	}
	result = btree_node_move_cell_ex_to(
		&parent_node,
		&node,
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

	result = btpage_err(pager_write_page(cursor->tree->pager, node.page));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_remove(&parent_node, &parent_index, NULL, NULL, 0);
	if( result != BTREE_OK )
		goto end;

	u32 source_node_page_id = node.page_number;

	// Save on a write here because the parent stays in memory.
	// result =
	// 	btpage_err(pager_write_page(cursor->tree->pager, parent_node.page));
	// if( result != BTREE_OK )
	// 	goto end;

	// Now move the highest element from the left child to the parent.
	result = cursor_sibling(
		cursor,
		mode == REBALANCE_MODE_ROTATE_RIGHT ? CURSOR_SIBLING_LEFT
											: CURSOR_SIBLING_RIGHT);
	if( result != BTREE_OK )
		goto end;

	result = btree_node_init_from_read(
		&node, page, cursor->tree->pager, cursor->current_page_id);
	if( result != BTREE_OK )
		goto end;

	u32 source_index_number = 0;
	if( mode == REBALANCE_MODE_ROTATE_RIGHT )
	{
		source_index_number = node.header->num_keys - 1;
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
	u32 orphaned_page_id = mode == REBALANCE_MODE_ROTATE_RIGHT
							   ? cursor->current_page_id
							   : source_node_page_id;
	result = btree_node_move_cell_ex_to(
		&node,
		&parent_node,
		source_index_number,
		&insert_elevated_index,
		orphaned_page_id,
		cursor->tree->pager);
	if( result != BTREE_OK )
		goto end;

	result =
		btpage_err(pager_write_page(cursor->tree->pager, parent_node.page));
	if( result != BTREE_OK )
		goto end;

	// Keep track of the child that was previously pointed to by the elevated
	// cell.
	// On rotate right, node points to left sibling. (Elevated cell side.)
	// On rotate left, node points to right sibling.
	u32 orphaned_child_page = node.keys[source_index_number].key;

	if( mode == REBALANCE_MODE_ROTATE_RIGHT )
	{
		u32 prev_rightmost_of_left_node = node.header->right_child;

		node.header->right_child = orphaned_child_page;

		// Need to update the lowered cell to point to this.
		orphaned_child_page = prev_rightmost_of_left_node;
	}
	else
	{}

	struct ChildListIndex source_index;
	source_index.index = source_index_number;
	source_index.mode = KLIM_INDEX;
	result = btree_node_remove(&node, &source_index, NULL, NULL, 0);
	if( result != BTREE_OK )
		goto end;

	result = btpage_err(pager_write_page(cursor->tree->pager, node.page));
	if( result != BTREE_OK )
		goto end;

	// Now we go back to the previously deficient page and update it to point to
	// the correct children.
	result = cursor_sibling(
		cursor,
		mode == REBALANCE_MODE_ROTATE_RIGHT ? CURSOR_SIBLING_RIGHT
											: CURSOR_SIBLING_LEFT);
	if( result != BTREE_OK )
		goto end;

	result = btree_node_init_from_read(
		&node, page, cursor->tree->pager, cursor->current_page_id);
	if( result != BTREE_OK )
		goto end;

	if( mode == REBALANCE_MODE_ROTATE_RIGHT )
	{
		// Need to update the lowered cell to point to
		node.keys[insert_lowered_index.index].key = orphaned_child_page;
	}
	else
	{
		u32 prev_rightmost_of_left_node = node.header->right_child;

		// Update the lowered cell in the left node to point to
		// child of elevated cell.
		node.header->right_child = orphaned_child_page;

		// Update the
		node.keys[insert_lowered_index.index].key = prev_rightmost_of_left_node;
	}

	result = btpage_err(pager_write_page(cursor->tree->pager, node.page));
	if( result != BTREE_OK )
		goto end;

end:
	if( page )
		page_destroy(cursor->tree->pager, page);
	if( parent_page )
		page_destroy(cursor->tree->pager, parent_page);
	return result;
}

enum btree_e
ibta_merge(struct Cursor* cursor, enum rebalance_mode_e mode)
{
	assert(
		mode == REBALANCE_MODE_MERGE_LEFT ||
		mode == REBALANCE_MODE_MERGE_RIGHT);

	enum btree_e result = BTREE_OK;

	// Merge left
	// sandwich left parent.
	// Parent cell points to the right child of the left node.
	struct BTreeNode left_node = {0};
	struct BTreeNode right_node = {0};
	struct BTreeNode parent_node = {0};

	struct Page* left_page = NULL;
	struct Page* right_page = NULL;
	struct Page* parent_page = NULL;

	result = btpage_err(page_create(cursor->tree->pager, &parent_page));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_init_from_page(&parent_node, parent_page);
	if( result != BTREE_OK )
		goto end;

	result = cursor_read_parent(cursor, &parent_node);
	if( result != BTREE_OK )
		goto end;

	result = btpage_err(page_create(cursor->tree->pager, &left_page));
	if( result != BTREE_OK )
		goto end;
	result = btpage_err(page_create(cursor->tree->pager, &right_page));
	if( result != BTREE_OK )
		goto end;

	struct ChildListIndex parent_index = {0};
	if( mode == REBALANCE_MODE_MERGE_RIGHT )
	{
		// The cursor points to the left child.
		result = btree_node_init_from_read(
			&left_node,
			left_page,
			cursor->tree->pager,
			cursor->current_page_id);
		if( result != BTREE_OK )
			goto end;

		result = cursor_parent_index(cursor, &parent_index);
		if( result != BTREE_OK )
			goto end;

		result = cursor_sibling(cursor, CURSOR_SIBLING_RIGHT);
		if( result != BTREE_OK )
			goto end;

		result = btree_node_init_from_read(
			&right_node,
			right_page,
			cursor->tree->pager,
			cursor->current_page_id);
		if( result != BTREE_OK )
			goto end;
	}
	else
	{
		result = btree_node_init_from_read(
			&right_node,
			right_page,
			cursor->tree->pager,
			cursor->current_page_id);
		if( result != BTREE_OK )
			goto end;

		result = cursor_sibling(cursor, CURSOR_SIBLING_LEFT);
		if( result != BTREE_OK )
			goto end;

		result = btree_node_init_from_read(
			&left_node,
			left_page,
			cursor->tree->pager,
			cursor->current_page_id);
		if( result != BTREE_OK )
			goto end;

		result = cursor_parent_index(cursor, &parent_index);
		if( result != BTREE_OK )
			goto end;
	}

	result = btree_node_move_cell_ex(
		&parent_node,
		&left_node,
		parent_index.index,
		left_node.header->right_child,
		cursor->tree->pager);
	if( result != BTREE_OK )
		goto end;

	result = btpage_err(pager_write_page(cursor->tree->pager, left_node.page));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_remove(&parent_node, &parent_index, NULL, NULL, 0);
	if( result != BTREE_OK )
		goto end;

	result =
		btpage_err(pager_write_page(cursor->tree->pager, parent_node.page));
	if( result != BTREE_OK )
		goto end;

	// TODO: Underflow condition.
	char parent_deficient = parent_node.header->num_keys < 1;

	for( int i = 0; i < right_node.header->num_keys; i++ )
	{
		result = btree_node_move_cell(
			&right_node, &left_node, i, cursor->tree->pager);
		if( result != BTREE_OK )
			goto end;
	}

	left_node.header->right_child = right_node.header->right_child;

	result = btree_node_copy(&right_node, &left_node);
	if( result != BTREE_OK )
		goto end;

	result = btpage_err(pager_write_page(cursor->tree->pager, right_node.page));
	if( result != BTREE_OK )
		goto end;

	// TODO: Free list left page.

	// If deficient.
	if( parent_deficient &&
		parent_node.page_number == cursor->tree->root_page_id )
	{
		// TODO: Assumes that root page is the same size as all other pages.
		// TODO: Free list right page.

		result = btree_node_copy(&parent_node, &right_node);
		if( result != BTREE_OK )
			goto end;

		result =
			btpage_err(pager_write_page(cursor->tree->pager, parent_node.page));
		if( result != BTREE_OK )
			goto end;
	}

end:
	if( parent_page )
		page_destroy(cursor->tree->pager, parent_page);
	if( left_page )
		page_destroy(cursor->tree->pager, left_page);
	if( right_page )
		page_destroy(cursor->tree->pager, right_page);

	if( result == BTREE_OK && parent_deficient )
		result = BTREE_ERR_PARENT_DEFICIENT;
	return result;
}

enum btree_e
ibta_rebalance(struct Cursor* cursor)
{
	enum btree_e result = BTREE_OK;
	enum rebalance_mode_e mode = REBALANCE_MODE_UNK;

	do
	{
		result = decide_rebalance_mode(cursor, &mode);
		if( result == BTREE_ERR_CURSOR_NO_PARENT )
		{
			// Can't rebalance root.
			result = BTREE_OK;
			goto end;
		}

		switch( mode )
		{
		case REBALANCE_MODE_ROTATE_LEFT:
		case REBALANCE_MODE_ROTATE_RIGHT:
			result = ibta_rotate(cursor, mode);
			break;
		case REBALANCE_MODE_MERGE_LEFT:
		case REBALANCE_MODE_MERGE_RIGHT:
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