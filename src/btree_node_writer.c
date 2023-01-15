#include "btree_node_writer.h"

#include "btree_cell.h"
#include "btree_node.h"
#include "btree_overflow.h"
#include "btree_utils.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static int
min(int left, int right)
{
	return left < right ? left : right;
}

enum btree_e
btree_node_write(
	struct BTreeNode* node,
	struct Pager* pager,
	u32 key,
	void* data,
	u32 data_size)
{
	char found;
	u32 insertion_index_number =
		btu_binary_search_keys(node->keys, node->header->num_keys, key, &found);

	struct InsertionIndex insertion_index = {0};
	btu_init_insertion_index_from_index(
		&insertion_index, node, insertion_index_number);

	return btree_node_write_ex(
		node,
		pager,
		&insertion_index,
		key,
		0,
		data,
		data_size,
		WRITER_EX_MODE_RAW);
}

// enum btree_e
// node_write_unknown(
// 	struct BTreeNode* node,
// 	struct Pager* pager,
// 	struct InsertionIndex* insertion_index,
// 	cell_type_e cell_type,
// 	u32 key,
// 	void* data,
// 	u32 data_size)
// {}

enum btree_e
btree_node_write_ex(
	struct BTreeNode* node,
	struct Pager* pager,
	struct InsertionIndex* insertion_index,
	u32 key,
	u32 flags,
	void* data,
	u32 data_size,
	enum writer_ex_mode_e mode)
{
	char found;
	enum btree_e result = BTREE_OK;

	if( node->page->page_id == 1 && key == 32 )
		printf("WOWOW?\n");

	if( mode == WRITER_EX_MODE_CELL_MOVE )
	{
		// TODO: Could probably implement all inserts in terms of,
		// 1. Write payload to empty page,
		// 2. Move cell.

		result = btree_node_move_from_data(
			node, insertion_index, key, flags, data, data_size, pager);

		if( result == BTREE_OK )
			result = btpage_err(pager_write_page(pager, node->page));

		return result;
	}

	// This is max size including key!
	// I.e. key+payload_size must fit within this.
	u32 max_data_size = btree_node_max_cell_size(node);
	u32 max_heap_usage = min(max_data_size, node->header->free_heap);

	// Size check
	u32 heap_size = btree_node_heap_required_for_insertion(
		btree_cell_inline_disk_size(data_size));

	// TODO: Should this be max_heap_usage?
	if( heap_size <= max_data_size )
	{
		struct BTreeCellInline cell = {0};
		cell.inline_size = data_size;
		cell.payload = data;
		result = btree_node_insert_inline(node, insertion_index, key, &cell);
		if( result == BTREE_OK )
			result = btpage_err(pager_write_page(pager, node->page));

		return result;
	}
	else
	{
		// Overflow

		// Check if the smallest possible overflow page will fit.
		u32 min_heap_required = btree_node_heap_required_for_insertion(
			btree_cell_overflow_min_disk_size());

		if( min_heap_required > max_heap_usage )
			return BTREE_ERR_NODE_NOT_ENOUGH_SPACE;

		u32 inline_payload_size = max_heap_usage - min_heap_required;

		u32 payload_bytes_writable_to_overflow_page =
			btree_overflow_max_write_size(pager);

		// TODO: Better way?
		u32 bytes_written = 0;
		char* overflow_data = (char*)data;
		overflow_data += inline_payload_size;

		u32 num_overflow_pages = 1;
		u32 page_write_size = data_size - inline_payload_size;
		// Get the last slice of data.
		while( page_write_size > payload_bytes_writable_to_overflow_page )
		{
			overflow_data += payload_bytes_writable_to_overflow_page;
			page_write_size -= payload_bytes_writable_to_overflow_page;
			num_overflow_pages += 1;
		}

		u32 last_page_id = 0;
		struct BTreeOverflowWriteResult write_result = {0};
		do
		{
			result = btree_overflow_write(
				pager,
				overflow_data,
				page_write_size,
				last_page_id,
				&write_result);
			if( result != BTREE_OK )
				return result;

			assert(num_overflow_pages >= 0);
			num_overflow_pages--;
			if( num_overflow_pages == 0 )
			{
				overflow_data -= inline_payload_size;
			}
			else
			{
				overflow_data -= payload_bytes_writable_to_overflow_page;
			}
			bytes_written += page_write_size;
			last_page_id = write_result.page_id;

			page_write_size = payload_bytes_writable_to_overflow_page;
		} while( bytes_written < data_size - inline_payload_size );

		struct BTreeCellOverflow write_payload = {0};
		write_payload.total_size = data_size;
		write_payload.overflow_page_id = last_page_id;
		write_payload.inline_size = btree_cell_inline_size_from_disk_size(
			btree_cell_overflow_disk_size(inline_payload_size));
		write_payload.inline_payload = overflow_data;

		result = btree_node_insert_overflow(
			node, insertion_index, key, &write_payload);

		if( result == BTREE_OK )
			result = btpage_err(pager_write_page(pager, node->page));

		return result;
	}
}