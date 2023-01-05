#include "btree_cell.h"

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