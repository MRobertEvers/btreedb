#include "btree_node_reader.h"

#include "btree_cell.h"
#include "btree_overflow.h"
#include "btree_utils.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int
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
	char found;
	int key_index =
		btu_binary_search_keys(node->keys, node->header->num_keys, key, &found);

	if( !found )
		return BTREE_ERR_KEY_NOT_FOUND;

	unsigned int total_payload_size = 0;
	unsigned int next_page_id = 0;

	struct CellData cell = {0};

	btu_read_cell(node, key_index, &cell);

	unsigned int cell_size = btree_cell_get_size(&cell);

	int written_size = overflow_payload_reader(
		cell.pointer,
		cell_size,
		buffer,
		buffer_size,
		&next_page_id,
		&total_payload_size);

	char* next_buffer = (char*)buffer;
	next_buffer += written_size;

	struct BTreeOverflowReadResult overflow_read_result = {0};
	while( next_page_id != 0 )
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

	return BTREE_OK;
}