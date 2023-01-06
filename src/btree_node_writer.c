#include "btree_node_writer.h"

#include "btree_node.h"
#include "btree_overflow.h"
#include "btree_utils.h"

#include <assert.h>
#include <stdlib.h>

static int
min(int left, int right)
{
	return left < right ? left : right;
}

struct BTreeOverflowPayload
{
	// This is the data that is written to this page.
	void* data;
	unsigned int data_size;

	unsigned int full_payload_size;
	unsigned int overflow_page_next_id;
};

static int
overflow_payload_writer(
	void* self,
	btree_node_writer_partial_t write,
	struct BTreeNodeWriterState* writer_data,
	unsigned int max_write_size)
{
	struct BTreeOverflowPayload* write_payload =
		(struct BTreeOverflowPayload*)self;

	unsigned int written_size = 0;

	written_size +=
		write(writer_data, write_payload->data, write_payload->data_size);

	written_size += write(
		writer_data,
		&write_payload->overflow_page_next_id,
		sizeof(write_payload->overflow_page_next_id));

	written_size += write(
		writer_data,
		&write_payload->full_payload_size,
		sizeof(write_payload->full_payload_size));

	return written_size;
}

enum btree_e
btree_node_write(
	struct BTreeNode* node,
	struct Pager* pager,
	unsigned int key,
	void* data,
	unsigned int data_size)
{
	unsigned int insertion_index_number;
	char found;
	enum btree_e result = BTREE_OK;
	struct BTreeCellInline cell = {0};

	// We want the page to be able to fit at least 4 keys.
	unsigned int min_cells_per_page = 4;

	// This is max size including key!
	// I.e. key+payload_size must fit within this.
	unsigned int max_data_size =
		btu_get_node_heap_size(node) / min_cells_per_page;
	unsigned int max_heap_usage = min(max_data_size, node->header->free_heap);

	// Size check
	unsigned int inline_payload_size =
		btree_node_get_heap_required_for_insertion(
			btree_cell_inline_get_inline_size(data_size));

	insertion_index_number =
		btu_binary_search_keys(node->keys, node->header->num_keys, key, &found);

	struct InsertionIndex insertion_index = {0};
	btu_init_insertion_index_from_index(
		&insertion_index, node, insertion_index_number);

	// TODO: Should this be max_heap_usage?
	if( inline_payload_size <= max_data_size )
	{
		cell.inline_size = data_size;
		cell.payload = data;
		result = btree_node_insert_inline(node, &insertion_index, key, &cell);
		if( result == BTREE_OK )
			pager_write_page(pager, node->page);

		return result;
	}
	else
	{
		// Overflow

		// Check if the smallest possible overflow page will fit.
		unsigned int min_heap_required =
			btree_node_get_heap_required_for_insertion(
				btree_cell_overflow_get_min_inline_size());

		if( min_heap_required > max_heap_usage )
			return BTREE_ERR_NODE_NOT_ENOUGH_SPACE;

		unsigned int inline_payload_size = max_heap_usage - min_heap_required;

		unsigned int payload_bytes_writable_to_overflow_page =
			btree_overflow_max_write_size(pager);

		// TODO: Better way?
		unsigned int bytes_written = 0;
		char* overflow_data = (char*)data;
		overflow_data += inline_payload_size;

		unsigned int num_overflow_pages = 1;
		unsigned int page_write_size = data_size - inline_payload_size;
		// Get the last slice of data.
		while( page_write_size > payload_bytes_writable_to_overflow_page )
		{
			overflow_data += payload_bytes_writable_to_overflow_page;
			page_write_size -= payload_bytes_writable_to_overflow_page;
			num_overflow_pages += 1;
		}

		unsigned int last_page_id = 0;
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

		struct BTreeOverflowPayload write_payload = {0};
		write_payload.full_payload_size = data_size;
		write_payload.overflow_page_next_id = last_page_id;
		write_payload.data_size = inline_payload_size;
		write_payload.data = overflow_data;

		result = btree_node_insert_ex(
			node,
			&insertion_index,
			key,
			&overflow_payload_writer,
			&write_payload,
			CELL_FLAG_OVERFLOW);

		if( result == BTREE_OK )
			pager_write_page(pager, node->page);

		return result;
	}
}