#include "btree_alg.h"
#include "btree_cell.h"
#include "btree_node.h"
#include "btree_node_debug.h"
#include "btree_overflow.h"
#include "btree_utils.h"
#include "pager.h"
#include "serialization.h"

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
		btree_node_move(source_node, left, i, pager);
	}

	// The last child on the left node is not included in ibtrees split. That
	// must go to the parent.
	if( first_half > 0 )
	{
		if( holding_node )
		{
			btree_node_move(source_node, holding_node, first_half - 1, pager);
		}

		// The left node's right child becomes the page of the key that gets
		// promoted to the parent.
		left->header->right_child = source_node->keys[first_half - 1].key;
	}

	for( int i = first_half; i < source_node->header->num_keys; i++ )
	{
		btree_node_move(source_node, right, i, pager);
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

	result = btree_node_move_ex(
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

	memcpy(
		btu_get_node_buffer(node),
		btu_get_node_buffer(right),
		btu_get_node_size(right));
	node->page_number = right_page->page_id;

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