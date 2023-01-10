#ifndef BTREE_CELL_H_
#define BTREE_CELL_H_

#include "btint.h"
#include "btree_defs.h"
#include "buffer_writer.h"

/**
 * Attention!!! Terminology!
 *
 * inline_size      := Size of cell payload not including the size field.
 * inline_heap_size := Size of cell payload including size field... i.e. size on
 *  disk.
 *
 * inline_payload_size := For inline cells, this is everything but the size
 *  field; for overflow cells, it is the size of the non-meta payload... i.e.
 *  excluding overflow_page, total_size, etc.
 *
 */

/**
 * @brief Get the size of the cell not including the size field.
 *
 * @return int
 */
u32 btree_cell_inline_size_from_disk_size(u32 heap_size);

// Attention! If you change this, you must change the size calculation!!!
struct BTreeCellInline
{
	// inline_size is size of everything except this int.
	// I.e. inline_payload_size
	u32 inline_size;
	void* payload;
};

/**
 * @brief Get the size of the cell inline with payload data of size.
 *
 * @return int
 */
u32 btree_cell_inline_disk_size(u32 data_size);

/**
 * @brief Write inline cell to buffer.
 *
 * // TODO: Bounds chekc
 *
 * @param cell
 * @param buffer
 * @param buffer_size
 */
enum btree_e btree_cell_write_inline(
	struct BTreeCellInline* cell, void* buffer, u32 buffer_size);

/**
 * @brief Read inline and copy inline payload to buffer.
 *
 * If buffer is NULL, then this will return the cell with the payload pointer
 * pointing to the payload in the cell buffer. (i.e. Aliasing the cell_buffer)
 * Otherwise, cell->payload is a pointer to the input buffer.
 *
 * @param cell_buffer
 * @param cell_buffer_size
 * @param cell
 * @param buffer
 * @param buffer_size
 */
void btree_cell_read_inline(
	void* cell_buffer,
	u32 cell_buffer_size,
	struct BTreeCellInline* cell,
	void* buffer,
	u32 buffer_size);

// Attention! If you change this, you must change the size calculation!!!
struct BTreeCellOverflow
{
	// inline_size is size of everything except this int.
	// I.e. sizeof(total_size) + sizeof(overflow_page_id) + inline_payload_size
	u32 inline_size;
	u32 total_size;
	u32 overflow_page_id;
	void* inline_payload;
};

/**
 * @brief Get the minimum size required for an overflow cell in the heap.
 *
 * Note: This includes the size of the inline_size field as well.
 *
 * @return int
 */
u32 btree_cell_overflow_min_disk_size(void);

/**
 * @brief Get the size required of an overflow cell with payload size to be
 * stored on disk.
 *
 * Note: This includes the size of the inline_size field as well.
 *
 * ATTENTION! This is inline_payload_size, NOT inline_size.
 *
 * @return u32
 */
u32 btree_cell_overflow_disk_size(u32 payload_size);

/**
 * @brief Get the inline payload size of an overflow cell with inline size.
 *
 * ATTENTION! NOT inline_heap_size...
 *
 * @return u32
 */
u32 btree_cell_overflow_calc_inline_payload_size(u32 inline_size);

/**
 * @brief Writes overflow cell with BufferWriter
 *
 * @param cell
 * @param writer
 * @return enum btree_e
 */
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
	u32 buffer_size);

enum btree_e btree_cell_init_overflow_writer(
	struct BufferWriter* writer, void* buffer, u32 buffer_size);

enum btree_e btree_cell_init_overflow_reader(
	struct BufferReader* reader, void* buffer, u32 buffer_size);

/**
 * @brief Unpack the cell size from a cell.
 *
 * Needed because the size field of the cell struct contains some flags.
 *
 * @param cell
 * @return int
 */
int btree_cell_get_size(struct CellData* cell);

#endif