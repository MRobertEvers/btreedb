#include "btree_cell.h"

#include "serialization.h"

#include <string.h>

static int
min(int left, int right)
{
	return left < right ? left : right;
}

u32
btree_cell_inline_size_from_disk_size(u32 heap_size)
{
	return heap_size - sizeof(((struct BTreeCellInline*)0)->inline_size);
}

/**
 * See header for details.
 */
u32
btree_cell_inline_disk_size(u32 data_size)
{
	/**
	 * Attention! I do not want to pack my BTreeCellInline struct,
	 *
	 * Simply sum up the size of all the non-pointer fields.
	 */
	u32 on_disk_size = sizeof(((struct BTreeCellInline*)0)->inline_size);

	on_disk_size += data_size;

	return on_disk_size;
}

/**
 * See header for details.
 */
enum btree_e
btree_cell_write_inline(
	struct BTreeCellInline* cell, void* buffer, u32 buffer_size)
{
	char* payload = (char*)cell->payload;
	char* cell_left_edge = buffer;
	ser_write_32bit_le(cell_left_edge, cell->inline_size);

	cell_left_edge = cell_left_edge + sizeof(cell->inline_size);
	memcpy(cell_left_edge, payload, cell->inline_size);

	return BTREE_OK;
}

/**
 * See header for details.
 */
void
btree_cell_read_inline(
	void* cell_buffer,
	u32 cell_buffer_size,
	struct BTreeCellInline* cell,
	void* buffer,
	u32 buffer_size)
{
	// TODO: Bounds check.
	char* cell_left_edge = cell_buffer;
	ser_read_32bit_le(&cell->inline_size, cell_left_edge);

	cell_left_edge += sizeof(cell->inline_size);
	if( buffer != NULL && buffer_size != 0 )
	{
		u32 size = min(cell->inline_size, buffer_size);
		memcpy(buffer, cell_left_edge, size);
		cell->payload = buffer;
		cell->inline_size = size;
	}
	else
	{
		cell->payload = cell_left_edge;
		cell->inline_size = cell->inline_size;
	}
}

/**
 * See header for details.
 */
u32
btree_cell_overflow_min_disk_size()
{
	/**
	 * Attention! I do not want to pack my BTreeCellInline struct,
	 *
	 * Simply sum up the size of all the non-pointer fields.
	 */
	u32 on_disk_size = 0;

	on_disk_size += sizeof(((struct BTreeCellOverflow*)0)->inline_size);
	on_disk_size += sizeof(((struct BTreeCellOverflow*)0)->total_size);
	on_disk_size += sizeof(((struct BTreeCellOverflow*)0)->overflow_page_id);

	return on_disk_size;
}

/**
 * See header for details.
 */
u32
btree_cell_overflow_disk_size(u32 payload_size)
{
	return btree_cell_overflow_min_disk_size() + payload_size;
}

/**
 * See header for details.
 */
u32
btree_cell_overflow_calc_inline_payload_size(u32 inline_size)
{
	return inline_size - btree_cell_overflow_min_disk_size() +
		   sizeof(((struct BTreeCellOverflow*)0)->inline_size);
}

/**
 * See header for details.
 */
enum btree_e
btree_cell_write_overflow_ex(
	struct BTreeCellOverflow* cell, struct BufferWriter* writer)
{
	u32 written_size = 0;

	unsigned char buf[sizeof(cell->overflow_page_id)];

	ser_write_32bit_le(buf, cell->inline_size);
	written_size += buffer_writer_write(writer, buf, sizeof(buf));

	ser_write_32bit_le(buf, cell->total_size);
	written_size += buffer_writer_write(writer, buf, sizeof(buf));

	ser_write_32bit_le(buf, cell->overflow_page_id);
	written_size += buffer_writer_write(writer, buf, sizeof(buf));

	u32 inline_payload_size =
		btree_cell_overflow_calc_inline_payload_size(cell->inline_size);
	written_size +=
		buffer_writer_write(writer, cell->inline_payload, inline_payload_size);

	return BTREE_OK;
}

/**
 * See header for details.
 */
enum btree_e
btree_cell_read_overflow_ex(
	struct BufferReader* reader,
	struct BTreeCellOverflow* cell,
	void* buffer,
	u32 buffer_size)
{
	memset(cell, 0x00, sizeof(*cell));

	unsigned char buf[sizeof(cell->overflow_page_id)];
	buffer_reader_read(reader, buf, sizeof(buf));
	ser_read_32bit_le(&cell->inline_size, buf);

	buffer_reader_read(reader, buf, sizeof(buf));
	ser_read_32bit_le(&cell->total_size, buf);

	buffer_reader_read(reader, buf, sizeof(buf));
	ser_read_32bit_le(&cell->overflow_page_id, buf);

	u32 inline_payload_size =
		btree_cell_overflow_calc_inline_payload_size(cell->inline_size);

	u32 read_size = min(inline_payload_size, buffer_size);
	buffer_reader_read(reader, buffer, read_size);

	// TODO: I don't like this.
	if( buffer )
	{
		cell->inline_payload = buffer;
	}
	else
	{
		cell->inline_payload = reader->read_head;
	}

	return BTREE_OK;
}

/**
 * See header for details.
 */
enum btree_e
btree_cell_init_overflow_writer(
	struct BufferWriter* writer, void* buffer, u32 buffer_size)
{
	buffer_writer_init(writer, buffer, buffer_size, bw_write_lr);

	return BTREE_OK;
}

enum btree_e
btree_cell_init_overflow_reader(
	struct BufferReader* reader, void* buffer, u32 buffer_size)
{
	buffer_reader_init(reader, buffer, buffer_size, bw_read_lr);

	return BTREE_OK;
}

int
btree_cell_get_size(struct CellData* cell)
{
	// u32 size_buffer = (*cell->size);
	// u32 mask = ~(1 << (sizeof(size_buffer) * 8 - 1));
	// return size_buffer & mask;

	return *cell->size;
}
