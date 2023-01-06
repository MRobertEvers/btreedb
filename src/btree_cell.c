#include "btree_cell.h"

#include "serialization.h"

#include <string.h>

unsigned int
btree_cell_inline_get_inline_size(unsigned int data_size)
{
	return sizeof(struct BTreeCellInline) - sizeof(void*) + data_size;
}

void
btree_cell_write_inline(struct BTreeCellInline* cell, void* buffer)
{
	char* cell_left_edge = buffer;
	ser_write_32bit_le(cell_left_edge, cell->inline_size);

	cell_left_edge = cell_left_edge + sizeof(cell->inline_size);
	memcpy(cell_left_edge, cell->payload, cell->inline_size);
}

void
btree_cell_read_inline(
	void* cell_buffer,
	unsigned int cell_buffer_size,
	struct BTreeCellInline* cell,
	void* buffer,
	unsigned int buffer_size)
{
	// TODO: Bounds check.
	char* cell_left_edge = buffer;
	ser_read_32bit_le(&cell->inline_size, cell_left_edge);

	cell_left_edge += sizeof(cell->inline_size);
	memcpy(cell_left_edge, cell_buffer, cell->inline_size);
	cell->payload = buffer;
}

unsigned int
btree_cell_overflow_get_min_inline_size()
{
	return sizeof(struct BTreeCellOverflow) - sizeof(void*);
}

enum btree_e
btree_cell_write_overflow_ex(
	struct BTreeCellOverflow* cell, struct BufferWriter* writer)
{
	unsigned int written_size = 0;

	written_size += buffer_writer_write(
		writer, cell->inline_payload, sizeof(cell->inline_size));

	// TODO: Something better
	unsigned char buf[sizeof(cell->overflow_page_id)];
	ser_write_32bit_le(buf, cell->overflow_page_id);
	written_size += buffer_writer_write(writer, buf, sizeof(buf));

	ser_write_32bit_le(buf, cell->total_size);
	written_size += buffer_writer_write(writer, buf, sizeof(buf));

	ser_write_32bit_le(buf, cell->inline_size);
	written_size += buffer_writer_write(writer, buf, sizeof(buf));

	return written_size;
}

enum btree_e
btree_cell_read_overflow_ex(
	struct BufferReader* reader,
	struct BTreeCellOverflow* cell,
	void* buffer,
	unsigned int buffer_size)
{
	memset(cell, 0x00, sizeof(*cell));

	// Assumes LR Reader.
	// TODO: Make this not assume
	unsigned char buf[sizeof(cell->overflow_page_id)];
	buffer_reader_read(reader, buf, sizeof(buf));
	ser_read_32bit_le(&cell->inline_size, buf);

	buffer_reader_read(reader, buf, sizeof(buf));
	ser_read_32bit_le(&cell->total_size, buf);

	buffer_reader_read(reader, buf, sizeof(buf));
	ser_read_32bit_le(&cell->overflow_page_id, buf);

	buffer_reader_read(reader, buffer, buffer_size);
	cell->inline_payload = buffer;
}

int
btree_cell_get_size(struct CellData* cell)
{
	unsigned int size_buffer = (*cell->size);
	unsigned int mask = ~(1 << (sizeof(size_buffer) * 8 - 1));
	return size_buffer & mask;
}

int
btree_cell_has_flag(struct CellData* cell, enum btree_cell_flag_e flag)
{
	if( flag == CELL_FLAG_OVERFLOW )
		return ((*cell->size) & (1 << (sizeof(*cell->size) * 8 - 1))) != 0;

	return 0;
}

int
btree_cell_set_flag(unsigned int* buffer, enum btree_cell_flag_e flag)
{
	unsigned int flags = (*buffer);

	if( flag == CELL_FLAG_OVERFLOW )
		flags = flags | (1 << (sizeof(*buffer) * 8 - 1));

	*buffer = flags;
	return flags;
}