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
	unsigned int cell_size = 0;
	byte* cell_data = NULL;

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

	page_create(pager, &parent_page);
	result = btree_node_create_as_page_number(
		&parent, node->page_number, parent_page);
	if( result != BTREE_OK )
		goto end;

	page_create(pager, &left_page);
	result = btree_node_create_from_page(&left, left_page);
	if( result != BTREE_OK )
		goto end;

	page_create(pager, &right_page);
	result = btree_node_create_from_page(&right, right_page);
	if( result != BTREE_OK )
		goto end;

	struct split_node_t split_result = {0};
	result = split_node(node, pager, left, right, &split_result);
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

	// When splitting a leaf-node,
	// the right_child pointer becomes the right_page id
	// When splitting a non-leaf node
	// The right_child pointer becomes the right_page id
	// and the old right_child pointer becomes the right_child pointer
	// of the right page.
	parent->header->is_leaf = 0;

	parent->header->right_child = right_page->page_id;

	// Add the middle point between the left and right pages as a key to the
	// parent.
	write_cell.inline_size = sizeof(unsigned int);
	write_cell.payload = &left_page->page_id;
	result = btree_node_insert_inline(
		parent, &insert_end, split_result.left_child_high_key, &write_cell);
	if( result != BTREE_OK )
		goto end;

	memcpy(
		btu_get_node_buffer(node),
		btu_get_node_buffer(parent),
		btu_get_node_size(parent));

	result = btpage_err(pager_write_page(pager, node->page));
	if( result != BTREE_OK )
		goto end;

	if( split_page != NULL )
	{
		split_page->left_child_page_id = left_page->page_id;
		split_page->right_child_page_id = right_page->page_id;
		split_page->left_child_high_key = split_result.left_child_high_key;
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
bta_split_node(
	struct BTreeNode* node, struct Pager* pager, struct SplitPage* split_page)
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
	page_create(pager, &right_page);

	result =
		btree_node_create_as_page_number(&right, node->page_number, right_page);
	if( result != BTREE_OK )
		goto end;

	page_create(pager, &left_page);
	result = btree_node_create_from_page(&left, left_page);
	if( result != BTREE_OK )
		goto end;

	struct split_node_t split_result = {0};
	result = split_node(node, pager, left, right, &split_result);
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

	memcpy(
		btu_get_node_buffer(node),
		btu_get_node_buffer(right),
		btu_get_node_size(right));
	node->page_number = right_page->page_id;

	if( split_page != NULL )
	{
		split_page->left_page_id = left_page->page_id;
		split_page->left_page_high_key = split_result.left_child_high_key;
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

/**
 * See header for details.
 */
enum btree_e
bta_merge_nodes(
	struct BTreeNode* stable_node,
	struct BTreeNode* other_node,
	struct Pager* pager,
	struct MergedPage* merged_page)
{
	enum btree_e result = BTREE_OK;
	// TODO: Assert here?
	if( stable_node->header->is_leaf != other_node->header->is_leaf )
		return BTREE_ERR_CANNOT_MERGE;

	// Calculate the smallest amount of space that all the cells
	// from other_node could take up if they were overflow nodes.
	u32 min_reasonable_size_per_cell = btree_node_heap_required_for_insertion(
		btree_cell_overflow_min_disk_size());
	u32 min_size_required =
		min_reasonable_size_per_cell * other_node->header->num_keys;

	if( stable_node->header->free_heap < min_size_required )
		return BTREE_ERR_NODE_NOT_ENOUGH_SPACE;

	struct InsertionIndex index = {0};
	for( int i = 0; i < other_node->header->num_keys; i++ )
	{
		result = btree_node_move_cell(other_node, stable_node, i, pager);
		if( result != BTREE_OK )
			break;
	}

	if( result == BTREE_OK )
		result = btpage_err(pager_write_page(pager, stable_node->page));

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
bta_rotate(struct Cursor* cursor, enum bta_rebalance_mode_e mode)
{
	assert(
		mode == BTA_REBALANCE_MODE_ROTATE_LEFT ||
		mode == BTA_REBALANCE_MODE_ROTATE_RIGHT);
	enum btree_e result = BTREE_OK;
	struct Page* source_page = NULL;
	struct Page* dest_page = NULL;
	struct Page* parent_page = NULL;
	struct BTreeNode nodes[3] = {0};
	// 	noderc_

	// 		result = btpage_err(page_create(cursor->tree->pager, &source_page));
	// 	if( result != BTREE_OK )
	// 		goto end;

	// 	result = btree_node_init_from_page(&source_node, source_page);
	// 	if( result != BTREE_OK )
	// 		goto end;

	// 	result = btpage_err(page_create(cursor->tree->pager, &dest_page));
	// 	if( result != BTREE_OK )
	// 		goto end;

	// 	result = btree_node_init_from_page(&dest_node, dest_page);
	// 	if( result != BTREE_OK )
	// 		goto end;

	// 	result = btpage_err(page_create(cursor->tree->pager, &parent_page));
	// 	if( result != BTREE_OK )
	// 		goto end;

	// 	result = btree_node_init_from_page(&parent_node, parent_page);
	// 	if( result != BTREE_OK )
	// 		goto end;

	// 	result = cursor_read_parent(cursor, &parent_node);
	// 	if( result != BTREE_OK )
	// 		goto end;

	// 	struct ChildListIndex parent_index = {0};
	// 	if( mode == BTA_REBALANCE_MODE_ROTATE_LEFT )
	// 	{
	// 		// The cursor points to the left child.
	// 		result = btree_node_init_from_read(
	// 			&dest_node,
	// 			dest_page,
	// 			cursor->tree->pager,
	// 			cursor->current_page_id);
	// 		if( result != BTREE_OK )
	// 			goto end;

	// 		result = cursor_parent_index(cursor, &parent_index);
	// 		if( result != BTREE_OK )
	// 			goto end;

	// 		result = cursor_sibling(cursor, CURSOR_SIBLING_RIGHT);
	// 		if( result != BTREE_OK )
	// 			goto end;

	// 		result = btree_node_init_from_read(
	// 			&source_node,
	// 			source_page,
	// 			cursor->tree->pager,
	// 			cursor->current_page_id);
	// 		if( result != BTREE_OK )
	// 			goto end;
	// 	}
	// 	else
	// 	{
	// 		result = btree_node_init_from_read(
	// 			&dest_node,
	// 			dest_page,
	// 			cursor->tree->pager,
	// 			cursor->current_page_id);
	// 		if( result != BTREE_OK )
	// 			goto end;

	// 		result = cursor_sibling(cursor, CURSOR_SIBLING_LEFT);
	// 		if( result != BTREE_OK )
	// 			goto end;

	// 		result = btree_node_init_from_read(
	// 			&source_node,
	// 			source_page,
	// 			cursor->tree->pager,
	// 			cursor->current_page_id);
	// 		if( result != BTREE_OK )
	// 			goto end;

	// 		result = cursor_parent_index(cursor, &parent_index);
	// 		if( result != BTREE_OK )
	// 			goto end;
	// 	}

	// 	// This  becomes the moved key.
	// 	u32 parent_key = parent_node.keys[parent_index.index].key;

	// 	struct InsertionIndex insert_index = {0};
	// 	if( mode == BTA_REBALANCE_MODE_ROTATE_RIGHT )
	// 	{
	// 		insert_index.mode = KLIM_INDEX;
	// 		insert_index.index = 0;
	// 	}
	// 	else
	// 	{
	// 		insert_index.mode = KLIM_END;
	// 		insert_index.index = dest_node.header->num_keys;
	// 	}

	// 	u32 source_index = 0;
	// 	if( mode == BTA_REBALANCE_MODE_ROTATE_RIGHT )
	// 	{
	// 		source_index = source_node.header->num_keys;
	// 	}
	// 	else
	// 	{
	// 		source_index = 0;
	// 	}

	// 	result = btree_node_write_ex(
	// 		&dest_node, cursor->tree->pager, &insert_index, parent_key, );
	// 	result = btree_node_move_cell_ex_to(
	// 		&source_node,
	// 		&dest_node,
	// 		source_index,
	// 		&parent_key,
	// 		// For rotate right, this will eventually be updated to
	// 		// point to the prev r-most of the left node.
	// 		// For rotate left, this will point to the child of the prev
	// 		// leftmost node.
	// 		parent_key,
	// 		cursor->tree->pager);

	// 	if( result != BTREE_OK )
	// 		goto end;

	// end:
	// 	if( left_page )
	// 		page_destroy(cursor->tree->pager, left_page);
	// 	if( right_page )
	// 		page_destroy(cursor->tree->pager, right_page);
	// 	if( parent_page )
	// 		page_destroy(cursor->tree->pager, parent_page);
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
	struct CursorBreadcrumb base_crumb = {0};
	struct CursorBreadcrumb parent_crumb = {0};

	result = cursor_pop(cursor, &base_crumb);
	if( result != BTREE_OK )
		goto end;
	result = cursor_pop(cursor, &parent_crumb);
	if( result != BTREE_OK )
		goto end;
	result = cursor_push_crumb(cursor, &parent_crumb);
	if( result != BTREE_OK )
		goto end;
	result = cursor_push_crumb(cursor, &base_crumb);
	if( result != BTREE_OK )
		goto end;

	result = btpage_err(page_create(cursor->tree->pager, &page));
	if( result != BTREE_OK )
		goto end;

	result = cursor_sibling(cursor, sibling);
	if( result != BTREE_OK )
		goto restore;

	result = btree_node_init_from_read(
		&node, page, cursor->tree->pager, cursor->current_page_id);
	if( result != BTREE_OK )
		goto restore;

	// TODO: Underflow condition.
	if( node.header->num_keys <= 1 )
		result = BTREE_ERR_NODE_NOT_ENOUGH_SPACE;

	// If we've been passed a deficient node. This will fail.
	// So we allow this to pass if there is no sibling.
	// result = cursor_sibling(
	// 	cursor,
	// 	sibling == CURSOR_SIBLING_LEFT ? CURSOR_SIBLING_RIGHT
	// 								   : CURSOR_SIBLING_LEFT);
	// if( result != BTREE_OK || result != BTREE_ERR_CURSOR_NO_SIBLING )
	// 	goto end;

restore:
	check_result = result;
	result = cursor_pop(cursor, NULL);
	if( result != BTREE_OK )
		goto end;
	result = cursor_pop(cursor, NULL);
	if( result != BTREE_OK )
		goto end;
	result = cursor_push_crumb(cursor, &parent_crumb);
	if( result != BTREE_OK )
		goto end;
	result = cursor_push_crumb(cursor, &base_crumb);
	if( result != BTREE_OK )
		goto end;
	result = check_result;
end:
	if( page )
		page_destroy(cursor->tree->pager, page);
	if( result == BTREE_OK || result == BTREE_ERR_CURSOR_NO_SIBLING )
		result = check_result;
	return result;
}

static enum btree_e
decide_rebalance_mode(
	struct Cursor* cursor, enum bta_rebalance_mode_e* out_mode)
{
	enum btree_e result = BTREE_OK;
	enum btree_e right_result = BTREE_OK;

	right_result = check_sibling(cursor, CURSOR_SIBLING_RIGHT);
	result = right_result;
	if( result == BTREE_OK )
	{
		*out_mode = BTA_REBALANCE_MODE_ROTATE_LEFT;
		goto end;
	}

	if( result != BTREE_ERR_CURSOR_NO_SIBLING &&
		result != BTREE_ERR_NODE_NOT_ENOUGH_SPACE )
		goto end;

	result = check_sibling(cursor, CURSOR_SIBLING_LEFT);
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
		result = decide_rebalance_mode(cursor, &mode);
		if( result == BTREE_ERR_CURSOR_NO_PARENT )
		{
			// Can't rebalance root.
			result = BTREE_OK;
			goto end;
		}

		switch( mode )
		{
		case BTA_REBALANCE_MODE_ROTATE_LEFT:
		case BTA_REBALANCE_MODE_ROTATE_RIGHT:
			// result = bta_rotate(cursor, mode);
			break;
		case BTA_REBALANCE_MODE_MERGE_LEFT:
		case BTA_REBALANCE_MODE_MERGE_RIGHT:
			// result = bta_merge(cursor, mode);
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