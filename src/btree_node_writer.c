#include "btree_node_writer.h"

#include "btree_node.h"
#include "btree_overflow.h"
#include "btree_utils.h"

#include <stdlib.h>

struct BTreeOverflowPayload
{
	// This is the data that is written to this page.
	void* data;
	unsigned int data_size;

	unsigned int full_payload_size;
	unsigned int overflow_page_next_id;
};

int
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

	// 1. Determine if the payload can fit on the page with restrictions.
	// We want the page to be able to fit at least 4 keys.
	unsigned int min_cells_per_page = 4;

	// This is max size including key!
	// I.e. key+payload_size must fit within this.
	unsigned int max_data_size =
		btu_get_node_storage_size(node) / min_cells_per_page;

	// Size check
	unsigned int size_needed_on_first_page =
		btu_calc_cell_size(data_size) + sizeof(struct BTreePageKey);

	insertion_index_number =
		btu_binary_search_keys(node->keys, node->header->num_keys, key, &found);

	struct ChildListIndex child_index = {0};
	btu_init_keylistindex_from_index(
		&child_index, node, insertion_index_number);

	child_index.mode =
		child_index.mode == KLIM_RIGHT_CHILD ? KLIM_END : KLIM_INDEX;

	if( size_needed_on_first_page <= max_data_size )
	{
		// struct LeftInsertionIndex insertion_index = {0};
		// btu_get_left_insertion_from_keylistindex(&insertion_index,
		// &child_index);

		result = btree_node_insert(node, &child_index, key, data, data_size);
		if( result == BTREE_OK )
			pager_write_page(pager, node->page);

		return result;
	}
	else
	{
		// Overflow

		unsigned int min_size_needed_on_first_page =
			// TODO: Sizeof overfload cell
			btu_calc_cell_size(sizeof(unsigned int) * 2) +
			sizeof(struct BTreePageKey);

		unsigned max_data_size_available =
			node->header->free_heap > max_data_size ? max_data_size
													: node->header->free_heap;

		if( max_data_size_available < min_size_needed_on_first_page )
			return BTREE_ERR_NODE_NOT_ENOUGH_SPACE;

		// TODO: Algorithm to maximize page usage.
		// -2*int because we store the next page id and the size of the data on
		// the page. another int for cell size
		unsigned int payload_bytes_to_write_on_first_page =
			max_data_size_available - 3 * sizeof(unsigned int) -
			sizeof(struct BTreePageKey);

		unsigned int payload_bytes_writable_to_overflow_page =
			btree_overflow_max_write_size(pager);

		// TODO: Better way?
		unsigned int bytes_written = 0;
		char* overflow_data = (char*)data;
		overflow_data += payload_bytes_to_write_on_first_page;

		unsigned int num_overflow_pages = 1;
		unsigned int page_write_size =
			data_size - payload_bytes_to_write_on_first_page;
		// Get the last slice of data.
		while( page_write_size > payload_bytes_writable_to_overflow_page )
		{
			overflow_data += payload_bytes_writable_to_overflow_page;
			page_write_size -= payload_bytes_writable_to_overflow_page;
			num_overflow_pages += 1;
		}

		unsigned int last_page_id = 0;

		struct BTreeOverflowWriteResult write_result = {0};
		btree_overflow_write(
			pager, overflow_data, page_write_size, last_page_id, &write_result);

		num_overflow_pages--;
		if( num_overflow_pages == 0 )
		{
			overflow_data -= payload_bytes_to_write_on_first_page;
		}
		else
		{
			overflow_data -= payload_bytes_writable_to_overflow_page;
		}
		bytes_written += page_write_size;
		last_page_id = write_result.page_id;

		page_write_size = payload_bytes_writable_to_overflow_page;

		while( bytes_written <
			   data_size - payload_bytes_to_write_on_first_page )
		{
			btree_overflow_write(
				pager,
				overflow_data,
				page_write_size,
				last_page_id,
				&write_result);
			num_overflow_pages--;
			if( num_overflow_pages == 0 )
			{
				overflow_data -= payload_bytes_to_write_on_first_page;
			}
			else
			{
				overflow_data -= page_write_size;
			}
			bytes_written += page_write_size;
			last_page_id = write_result.page_id;
		}

		struct BTreeOverflowPayload write_payload = {0};
		write_payload.full_payload_size = data_size;
		write_payload.overflow_page_next_id = last_page_id;
		write_payload.data_size = payload_bytes_to_write_on_first_page;
		write_payload.data = overflow_data;

		btree_node_insert_ex(
			node,
			&child_index,
			key,
			&overflow_payload_writer,
			&write_payload,
			CELL_FLAG_OVERFLOW);

		pager_write_page(pager, node->page);

		return BTREE_OK;
	}
}