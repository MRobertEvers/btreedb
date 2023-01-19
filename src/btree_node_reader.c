#include "btree_node_reader.h"

#include "btree_cell.h"
#include "btree_node.h"
#include "btree_overflow.h"
#include "btree_utils.h"
#include "buffer_writer.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

enum btree_e
btree_node_read(
	struct BTreeNode* node,
	struct Pager* pager,
	u32 key,
	void* buffer,
	u32 buffer_size)
{
	enum btree_e result = BTREE_OK;
	char found;
	int key_index =
		btu_binary_search_keys(node->keys, node->header->num_keys, key, &found);

	if( !found )
		return BTREE_ERR_KEY_NOT_FOUND;

	u32 next_page_id = 0;

	char is_overflow_cell = btree_pkey_is_cell_type(
		btu_get_cell_flags(node, key_index), PKEY_FLAG_CELL_TYPE_OVERFLOW);

	byte* cell_buffer = btu_get_cell_buffer(node, key_index);
	// TODO: Not this
	u32 cell_buffer_size = btu_calc_highwater_offset(node, 0) - cell_buffer;

	if( !is_overflow_cell )
	{
		struct BTreeCellInline cell = {0};

		btree_cell_read_inline(
			cell_buffer, cell_buffer_size, &cell, buffer, buffer_size);

		result = BTREE_OK;
	}
	else
	{
		struct BTreeCellOverflow cell = {0};
		struct BufferReader reader = {0};
		char* next_buffer = (char*)buffer;

		btree_cell_init_overflow_reader(&reader, cell_buffer, cell_buffer_size);

		result = btree_cell_read_overflow_ex(
			&reader, &cell, next_buffer, buffer_size);
		if( result != BTREE_OK )
			return result;

		// TODO: read_overflow_ex should return bytes written to buffer.
		u32 written_size =
			btree_cell_overflow_calc_inline_payload_size(cell.inline_size);
		next_buffer += written_size;

		next_page_id = cell.overflow_page_id;

		struct BTreeOverflowReadResult overflow_read_result = {0};
		while( next_page_id != 0 && buffer_size > written_size )
		{
			result = btree_overflow_read(
				pager,
				next_page_id,
				next_buffer,
				buffer_size - written_size,
				&overflow_read_result);
			if( result != BTREE_OK )
				break;

			next_page_id = overflow_read_result.next_page_id;
			written_size += overflow_read_result.payload_bytes;
			next_buffer += overflow_read_result.payload_bytes;
		}
	}

	return result;
}

enum btree_e
btree_node_read_ex(
	struct BTree* tree,
	struct BTreeNode* node,
	void* key,
	u32 key_size,
	void* buffer,
	u32 buffer_size)
{
	enum btree_e result = BTREE_OK;
	struct Pager* pager = tree->pager;
	u32 key_index = 0;

	struct BTreeCompareContext ctx = {
		.compare = tree->compare,
		.reset = tree->reset_compare,
		.compare_context = NULL,
		.pager = tree->pager};
	result = btree_node_search_keys(&ctx, node, key, key_size, &key_index);
	if( result != BTREE_OK )
		return result;

	u32 next_page_id = 0;

	char is_overflow_cell = btree_pkey_is_cell_type(
		btu_get_cell_flags(node, key_index), PKEY_FLAG_CELL_TYPE_OVERFLOW);

	byte* cell_buffer = btu_get_cell_buffer(node, key_index);
	// TODO: Not this
	u32 cell_buffer_size = btu_calc_highwater_offset(node, 0) - cell_buffer;

	if( !is_overflow_cell )
	{
		struct BTreeCellInline cell = {0};

		btree_cell_read_inline(
			cell_buffer, cell_buffer_size, &cell, buffer, buffer_size);

		result = BTREE_OK;
	}
	else
	{
		struct BTreeCellOverflow cell = {0};
		struct BufferReader reader = {0};
		char* next_buffer = (char*)buffer;

		btree_cell_init_overflow_reader(&reader, cell_buffer, cell_buffer_size);

		result = btree_cell_read_overflow_ex(
			&reader, &cell, next_buffer, buffer_size);
		if( result != BTREE_OK )
			return result;

		// TODO: read_overflow_ex should return bytes written to buffer.
		u32 written_size =
			btree_cell_overflow_calc_inline_payload_size(cell.inline_size);
		next_buffer += written_size;

		next_page_id = cell.overflow_page_id;

		struct BTreeOverflowReadResult overflow_read_result = {0};
		while( next_page_id != 0 && buffer_size > written_size )
		{
			result = btree_overflow_read(
				pager,
				next_page_id,
				next_buffer,
				buffer_size - written_size,
				&overflow_read_result);
			if( result != BTREE_OK )
				break;

			next_page_id = overflow_read_result.next_page_id;
			written_size += overflow_read_result.payload_bytes;
			next_buffer += overflow_read_result.payload_bytes;
		}
	}

	return result;
}