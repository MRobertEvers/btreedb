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

enum btree_e
btree_node_write_at(
	struct BTreeNode* node,
	struct Pager* pager,
	struct InsertionIndex* insertion_index,
	u32 key,
	void* data,
	u32 data_size)
{
	return btree_node_write_ex(
		node,
		pager,
		insertion_index,
		key,
		0,
		data,
		data_size,
		WRITER_EX_MODE_RAW);
}

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
	enum btree_e result = BTREE_OK;

	if( mode == WRITER_EX_MODE_CELL_MOVE )
	{
		// TODO: Could probably implement all inserts in terms of,
		// 1. Write payload to empty page,
		// 2. Move cell.

		result = btree_node_move_cell_from_data(
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

			assert(num_overflow_pages > 0);
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

enum btree_e
btree_node_delete(
	struct BTree* tree, struct NodeView* nv, struct ChildListIndex* index)
{
	assert(index->mode == KLIM_INDEX);

	struct Pager* pager = nv_pager(nv);

	enum btree_e result = BTREE_OK;
	u32 index_num = index->index;
	byte* cell_buffer = btu_get_cell_buffer(nv_node(nv), index->index);
	// TODO: Not this
	u32 cell_buffer_size =
		btu_calc_highwater_offset(nv_node(nv), 0) - cell_buffer;

	char is_overflow_cell = btree_pkey_is_cell_type(
		btu_get_cell_flags(nv_node(nv), index_num),
		PKEY_FLAG_CELL_TYPE_OVERFLOW);

	if( is_overflow_cell )
	{
		struct BTreeCellOverflow cell = {0};
		struct BufferReader reader = {0};

		btree_cell_init_overflow_reader(&reader, cell_buffer, cell_buffer_size);

		result = btree_cell_read_overflow_ex(&reader, &cell, NULL, 0);
		if( result != BTREE_OK )
			return result;

		int next_page_id = cell.overflow_page_id;

		struct Page* temp_page = NULL;
		result = btpage_err(page_create(pager, &temp_page));
		if( result != BTREE_OK )
			goto skip;

		struct BTreeOverflowReadResult peek = {0};
		while( next_page_id != 0 )
		{
			result = btree_overflow_peek(
				pager, temp_page, next_page_id, &peek, NULL);
			if( result != BTREE_OK )
				break;

			result = btpage_err(pager_free_page_id(pager, next_page_id));
			if( result != BTREE_OK )
				break;

			next_page_id = peek.next_page_id;
		}

	skip:
		if( temp_page )
			page_destroy(pager, temp_page);

		result = noderc_reinit_read(tree->rcer, nv, nv_page(nv)->page_id);
		if( result != BTREE_OK )
			goto end;
	}

	// Remove the cell that we just freed.
	switch( tree->type )
	{
	case BTREE_INDEX:
	{
		result = ibtree_node_remove(nv_node(nv), index, NULL);
		if( result != BTREE_OK )
			return result;
		break;
	}
	case BTREE_TBL:
	{
		result = btree_node_remove(nv_node(nv), index, NULL, NULL, 0);
		if( result != BTREE_OK )
			return result;
		break;
	}
	}

	result = noderc_persist_n(tree->rcer, 1, nv);
	if( result != BTREE_OK )
		return result;

end:
	return BTREE_OK;
}