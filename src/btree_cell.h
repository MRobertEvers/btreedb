#ifndef BTREE_CELL_H_
#define BTREE_CELL_H_

#include "btree_defs.h"

/**
 * @brief Unpack the cell size from a cell.
 *
 * Needed because the size field of the cell struct contains some flags.
 *
 * @param cell
 * @return int
 */
int btree_cell_get_size(struct CellData* cell);

enum btree_cell_flag_e
{
	CELL_FLAG_NONE = 0,
	CELL_FLAG_OVERFLOW
};

/**
 * @brief Returns whether the cell has flags or not.
 *
 * @param cell
 * @param flags
 * @return int 1 if has flag
 */
int btree_cell_has_flag(struct CellData* cell, enum btree_cell_flag_e flag);

/**
 * @brief Sets the flags on a size buffer.
 *
 * @param cell
 * @param flags
 * @return int
 */
int btree_cell_set_flag(unsigned int* buffer, enum btree_cell_flag_e flag);

#endif