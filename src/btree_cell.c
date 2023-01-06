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

unsigned int
btree_cell_overflow_get_min_inline_size()
{
	return sizeof(struct BTreeCellOverflow) - sizeof(void*);
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