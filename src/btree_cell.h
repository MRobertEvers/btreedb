#ifndef BTREE_CELL_H_
#define BTREE_CELL_H_

#include "btree_defs.h"
#include "buffer_writer.h"

enum btree_cell_type_e
{
	CELL_TYPE_NONE,
	CELL_TYPE_INLINE,
	CELL_TYPE_OVERFLOW
};

// TODO : Need to use these structs in the writers.
struct BTreeCellInline
{
	// Size of payload not on an overflow page.
	unsigned int inline_size;
	void* payload;
};

/**
 * @brief Get the size of the cell inline with payload data of size.
 *
 * @return int
 */
unsigned int btree_cell_inline_get_inline_size(unsigned int data_size);

/**
 * @brief Write inline cell to buffer.
 *
 * // TODO: Bounds chekc
 *
 * @param cell
 * @param buffer
 * @param buffer_size
 */
void btree_cell_write_inline(struct BTreeCellInline* cell, void* buffer);

void btree_cell_read_inline(
	void* cell_buffer,
	unsigned int cell_buffer_size,
	struct BTreeCellInline* cell,
	void* buffer,
	unsigned int buffer_size);

struct BTreeCellOverflow
{
	unsigned int inline_size;
	unsigned int total_size;
	unsigned int overflow_page_id;
	void* inline_payload;
};

/**
 * @brief Get the minimum size required for an overflow cell.
 *
 * @return int
 */
unsigned int btree_cell_overflow_get_min_inline_size(void);

enum btree_e btree_cell_write_overflow_ex(
	struct BTreeCellOverflow* cell, struct BufferWriter* writer);

/**
 * @brief Reads cell meta information and places payload into buffer.
 *
 * Does NOT use the payload point in the cell struct.
 *
 * @param reader
 * @param cell
 * @param buffer
 * @param buffer_size
 * @return enum btree_e
 */
enum btree_e btree_cell_read_overflow_ex(
	struct BufferReader* reader,
	struct BTreeCellOverflow* cell,
	void* buffer,
	unsigned int buffer_size);

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