#include "btree_alg.h"

#include "btree_cell.h"
#include "btree_node.h"
#include "btree_node_debug.h"
#include "btree_overflow.h"
#include "btree_utils.h"
#include "pager.h"
#include "serialization.h"

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
	char* cell_data = NULL;

	key = source_node->keys[index].key;
	btu_read_cell(source_node, index, &read_cell);

	ser_read_32bit_le(buffer, read_cell.pointer);
}

static enum btree_e
copy_cell(
	struct BTreeNode* source_node, struct BTreeNode* other, unsigned int index)
{
	struct InsertionIndex insert_end = {.mode = KLIM_END};
	struct CellData read_cell = {0};
	u32 key = 0;
	u32 cell_size = 0;
	u32 flags = 0;
	char* cell_data = btu_get_cell_buffer(source_node, index);
	u32 cell_data_size = btu_get_cell_buffer_size(source_node, index);

	struct BTreeCellInline cell = {0};
	btree_cell_read_inline(cell_data, cell_data_size, &cell, NULL, 0);

	u32 dest_max_size = btree_node_max_cell_size(other);
	if( cell.inline_size <= dest_max_size )
	{
		key = source_node->keys[index].key;
		flags = source_node->keys[index].key;

		return btree_node_insert_inline_ex(
			other, &insert_end, key, &cell, flags);
	}
	else
	{
		return BTREE_ERR_NODE_NOT_ENOUGH_SPACE;
	}
}

static enum btree_e
copy_cell_with_overflow(
	struct BTreeNode* source_node,
	struct BTreeNode* other,
	unsigned int index,
	struct Pager* pager)
{
	enum btree_e result = BTREE_OK;
	struct InsertionIndex insert_end = {.mode = KLIM_END};

	u32 key = 0;
	u32 cell_size = 0;
	u32 flags = 0;

	key = source_node->keys[index].key;
	flags = source_node->keys[index].key;

	char* cell_data = btu_get_cell_buffer(source_node, index);
	u32 cell_data_size = btu_get_cell_buffer_size(source_node, index);

	struct BTreeCellInline cell = {0};
	btree_cell_read_inline(cell_data, cell_data_size, &cell, NULL, 0);

	u32 dest_max_size = btree_node_max_cell_size(other);
	if( cell_data_size <= dest_max_size )
	{
		return btree_node_insert_inline_ex(
			other, &insert_end, key, &cell, flags);
	}
	else
	{
		// Assumes that a btree can only be created that
		// restricts the min cell size to be greater than
		// the size required to fit an overflow cell.
		u32 is_overflow =
			btree_pkey_flags_get(flags, PKEY_FLAG_CELL_TYPE_OVERFLOW);
		// u32 source_max_size = btree_node_max_cell_size(source_node);
		// u32 cell_heap_size =
		// 	is_overflow ? btree_cell_overflow_disk_size(
		// 					  btree_cell_overflow_calc_inline_payload_size(
		// 						  cell.inline_size))
		// 				: cell.inline_size;
		// u32 new_heap_required =
		// 	btree_node_heap_required_for_insertion(cell_heap_size);
		u32 follow_page_id = 0;
		u32 bytes_overflown =
			btree_node_heap_required_for_insertion(cell_data_size) -
			dest_max_size;

		char* overflow_payload = NULL;
		char* payload = NULL;
		u32 overflow_payload_size = 0;
		u32 inline_size = 0;

		if( is_overflow )
		{
			// Overflow page -> Overflow page.
			struct BTreeCellOverflow read_cell = {0};
			struct BufferReader reader = {0};
			btree_cell_init_overflow_reader(&reader, cell_data, cell_data_size);

			btree_cell_read_overflow_ex(&reader, &read_cell, NULL, 0);
			u32 previous_inline_payload_size =
				btree_cell_overflow_calc_inline_payload_size(cell.inline_size);
			inline_size = cell.inline_size - bytes_overflown;
			payload = (char*)read_cell.inline_payload;
			u32 new_inline_payload_size =
				btree_cell_overflow_calc_inline_payload_size(inline_size);
			overflow_payload = payload + new_inline_payload_size;
			overflow_payload_size =
				previous_inline_payload_size - new_inline_payload_size;
			follow_page_id = read_cell.overflow_page_id;
		}
		else
		{
			inline_size = cell.inline_size - bytes_overflown;
			payload = cell.payload;
			u32 new_inline_payload_size =
				btree_cell_overflow_calc_inline_payload_size(inline_size);
			overflow_payload = cell.payload + new_inline_payload_size;
			overflow_payload_size = cell.inline_size - new_inline_payload_size;
			follow_page_id = 0;
		}

		struct BTreeOverflowWriteResult write_result = {0};
		btree_overflow_write(
			pager,
			overflow_payload,
			overflow_payload_size,
			follow_page_id,
			&write_result);

		struct BTreeCellOverflow write_cell = {0};
		write_cell.inline_payload = payload;
		write_cell.inline_size = inline_size;
		write_cell.overflow_page_id = write_result.page_id;
		write_cell.total_size = cell.inline_size;

		return btree_node_insert_overflow(other, &insert_end, key, &write_cell);
	}
}

struct split_node_t
{
	unsigned int left_child_high_key;
};

static enum btree_e
split_node(
	struct BTreeNode* source_node,
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
		copy_cell(source_node, left, i);
	}

	if( !source_node->header->is_leaf )
		read_key_cell(source_node, &left->header->right_child, first_half - 1);
	else
		copy_cell(source_node, left, first_half - 1);

	for( int i = first_half; i < source_node->header->num_keys; i++ )
	{
		copy_cell(source_node, right, i);
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
	btree_node_create_as_page_number(&parent, node->page_number, parent_page);

	page_create(pager, &left_page);
	btree_node_create_from_page(&left, left_page);

	page_create(pager, &right_page);
	btree_node_create_from_page(&right, right_page);

	struct split_node_t split_result = {0};
	result = split_node(node, left, right, &split_result);
	if( result != BTREE_OK )
		goto end;

	// We need to write the pages out to get the page ids.
	left->header->is_leaf = node->header->is_leaf;
	pager_write_page(pager, left_page);

	right->header->is_leaf = node->header->is_leaf;
	pager_write_page(pager, right_page);

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
	btree_node_insert_inline(
		parent, &insert_end, split_result.left_child_high_key, &write_cell);

	memcpy(
		btu_get_node_buffer(node),
		btu_get_node_buffer(parent),
		btu_get_node_size(parent));

	pager_write_page(pager, node->page);

	if( split_page != NULL )
	{
		split_page->left_child_page_id = left_page->page_id;
		split_page->right_child_page_id = right_page->page_id;
		split_page->left_child_high_key = split_result.left_child_high_key;
	}

end:
	btree_node_destroy(left);
	page_destroy(pager, left_page);

	btree_node_destroy(right);
	page_destroy(pager, right_page);

	btree_node_destroy(parent);
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

	struct BTreeNode* left = NULL;
	struct BTreeNode* right = NULL;

	struct Page* left_page = NULL;
	struct Page* right_page = NULL;

	// We want right page to remain stable since pointers
	// in parent nodes are already pointing to the high-key of the input
	// node which becomes the high key of the right child.
	page_create(pager, &right_page);
	btree_node_create_as_page_number(&right, node->page_number, right_page);

	page_create(pager, &left_page);
	btree_node_create_from_page(&left, left_page);

	struct split_node_t split_result = {0};
	result = split_node(node, left, right, &split_result);
	if( result != BTREE_OK )
		goto end;

	right->header->is_leaf = node->header->is_leaf;
	left->header->is_leaf = node->header->is_leaf;
	// We need to write the pages out to get the page ids.
	pager_write_page(pager, right_page);
	// Write out the input page.
	pager_write_page(pager, left_page);

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
	btree_node_destroy(left);
	page_destroy(pager, left_page);

	btree_node_destroy(right);
	page_destroy(pager, right_page);

	return BTREE_OK;
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
		result = copy_cell_with_overflow(other_node, stable_node, i, pager);
		if( result != BTREE_OK )
			break;
	}

	if( result == BTREE_OK )
		pager_write_page(pager, stable_node->page);

	return result;
}