#include "btree_node_reader.h"

#include "btree_cell.h"
#include "btree_node.h"
#include "btree_overflow.h"
#include "btree_utils.h"
#include "buffer_writer.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

static int
overflow_payload_reader(
	void* data,
	unsigned int data_size,
	void* buffer,
	unsigned int buffer_size,
	unsigned int* next_page,
	unsigned int* total_size)
{
	// TODO: Check buffer size.
	assert(data_size >= sizeof(unsigned int) * 2);

	// NOTE! Read reverse from the writer.
	char* payload_buffer = (char*)data;
	memcpy(total_size, payload_buffer, sizeof(*total_size));

	payload_buffer += sizeof(*total_size);
	memcpy(next_page, payload_buffer, sizeof(*next_page));

	payload_buffer += sizeof(*next_page);
	int bytes_read_to_buffer =
		data_size - sizeof(*next_page) - sizeof(*total_size);
	memcpy(buffer, payload_buffer, bytes_read_to_buffer);

	return bytes_read_to_buffer;
}

enum btree_e
btree_node_read(
	struct BTreeNode* node,
	struct Pager* pager,
	unsigned int key,
	void* buffer,
	unsigned int buffer_size)
{
	enum btree_e result = BTREE_OK;
	char found;
	int key_index =
		btu_binary_search_keys(node->keys, node->header->num_keys, key, &found);

	if( !found )
		return BTREE_ERR_KEY_NOT_FOUND;

	unsigned int total_payload_size = 0;
	unsigned int next_page_id = 0;

	char is_overflow_cell = btree_pkey_flags_get(
		btu_get_cell_flags(node, key_index), PKEY_FLAG_CELL_TYPE_OVERFLOW);

	char* cell_buffer = btu_get_cell_buffer(node, key_index);
	// TODO: Not this
	u32 cell_buffer_size = btu_calc_highwater_offset(node, 0) - cell_buffer;

	if( !is_overflow_cell )
	{
		struct BTreeCellInline cell = {0};

		btree_cell_read_inline(
			cell_buffer, cell_buffer_size, &cell, buffer, buffer_size);

		return BTREE_OK;
	}
	else
	{
		struct BTreeCellOverflow cell = {0};
		struct BufferReader reader = {0};
		char* next_buffer = (char*)buffer;

		result = btree_cell_init_overflow_reader(
			&reader, cell_buffer, cell_buffer_size);
		if( result != BTREE_OK )
			return result;

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
			btree_overflow_read(
				pager,
				next_page_id,
				next_buffer,
				buffer_size - written_size,
				&overflow_read_result);

			next_page_id = overflow_read_result.next_page_id;
			written_size += overflow_read_result.bytes_read;
			next_buffer += overflow_read_result.bytes_read;
		}
	}

	return BTREE_OK;
}