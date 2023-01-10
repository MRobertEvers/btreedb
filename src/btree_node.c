#include "btree_node.h"

#include "btree_cell.h"
#include "btree_node_debug.h"
#include "btree_utils.h"
#include "serialization.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

static int
min(int left, int right)
{
	return left < right ? left : right;
}

static enum btree_e
btree_node_alloc(struct BTreeNode** r_node)
{
	*r_node = (struct BTreeNode*)malloc(sizeof(struct BTreeNode));
	return BTREE_OK;
}

static enum btree_e
btree_node_dealloc(struct BTreeNode* node)
{
	free(node);
	return BTREE_OK;
}

static enum btree_e
btree_node_deinit(struct BTreeNode* node)
{
	return BTREE_OK;
}

u32
btree_node_max_cell_size(struct BTreeNode* node)
{
	// We want the page to be able to fit at least 4 keys.
	u32 min_cells_per_page = 4;

	// This is max size including key!
	// I.e. key+payload_size must fit within this.
	u32 max_data_size = btu_get_node_heap_size(node) / min_cells_per_page;

	return max_data_size;
}

u32
btree_pkey_flags_set(u32 flags, enum btree_page_key_flags_e flag)
{
	return flags | (1 << flag);
}

/**
 * @brief Returns 1 if flag is present; 0 otherwise
 *
 * @param flags
 * @return u32
 */
u32
btree_pkey_flags_get(u32 flags, enum btree_page_key_flags_e flag)
{
	return (flags & (1 << flag)) != 0;
}

/**
 * See header for details.
 */
enum btree_e
btree_node_init_from_page(struct BTreeNode* node, struct Page* page)
{
	int offset = page->page_id == 1 ? BTREE_HEADER_SIZE : 0;
	char* byte_buffer = (char*)page->page_buffer;
	void* data = byte_buffer + offset;

	node->page = page;
	node->page_number = page->page_id;
	node->header = (struct BTreePageHeader*)data;
	// Trick to get a pointer to the address immediately after the header.
	node->keys = (struct BTreePageKey*)&node->header[1];

	if( node->header->num_keys == 0 )
		node->header->free_heap = btree_node_calc_heap_capacity(node);

	return BTREE_OK;
}

/**
 * See header for details.
 */
enum btree_e
btree_node_create_from_page(struct BTreeNode** r_node, struct Page* page)
{
	btree_node_alloc(r_node);
	btree_node_init_from_page(*r_node, page);

	return BTREE_OK;
}

/**
 * See header for details.
 */
enum btree_e
btree_node_create_as_page_number(
	struct BTreeNode** r_node, int page_number, struct Page* page)
{
	page->page_id = page_number;
	return btree_node_create_from_page(r_node, page);
}

/**
 * See header for details.
 */
enum btree_e
btree_node_create_empty(
	struct BTreeNode** r_node, int page_id, struct Page* page)
{
	btree_node_alloc(r_node);
	btree_node_init_from_page(*r_node, page);

	return BTREE_OK;
}

/**
 * See header for details.
 */
enum btree_e
btree_node_destroy(struct BTreeNode* node)
{
	btree_node_deinit(node);
	btree_node_dealloc(node);
	return BTREE_OK;
}

/**
 * @brief Assumes space check has already been done.
 *
 * @param array
 */
static void
insert_page_key(
	struct BTreePageKey* array,
	u32 array_size,
	struct BTreePageKey* key,
	u32 index_number)
{
	memmove(
		&array[index_number + 1],
		&array[index_number],
		(array_size - index_number) * sizeof(array[0]));

	memcpy(&array[index_number], key, sizeof(*key));
}

static enum btree_e
write_page_key(
	struct BTreeNode* node,
	struct InsertionIndex* index,
	u32 key,
	u32 cell_size,
	int flags)
{
	u32 index_number =
		index->mode == KLIM_END ? node->header->num_keys : index->index;
	u32 cell_left_edge_offset =
		node->header->cell_high_water_offset + cell_size;

	// The Raw insertion
	struct BTreePageKey page_key = {0};
	page_key.cell_offset = cell_left_edge_offset;
	page_key.key = key;
	page_key.flags = flags;

	insert_page_key(
		node->keys, node->header->num_keys, &page_key, index_number);
	node->header->num_keys += 1;

	node->header->cell_high_water_offset += cell_size;
	node->header->free_heap -=
		btree_node_heap_required_for_insertion(cell_size);

	return BTREE_OK;
}

enum btree_e
btree_node_insert_inline(
	struct BTreeNode* node,
	struct InsertionIndex* index,
	u32 key,
	struct BTreeCellInline* cell)
{
	return btree_node_insert_inline_ex(
		node,
		index,
		key,
		cell,
		btree_pkey_flags_set(0, PKEY_FLAG_CELL_TYPE_INLINE));
}

enum btree_e
btree_node_insert_inline_ex(
	struct BTreeNode* node,
	struct InsertionIndex* index,
	u32 key,
	struct BTreeCellInline* cell,
	u32 flags)
{
	enum btree_e result = BTREE_OK;
	u32 cell_size = btree_cell_inline_disk_size(cell->inline_size);

	u32 heap_needed = btree_node_heap_required_for_insertion(cell_size);
	if( node->header->free_heap < heap_needed )
		return BTREE_ERR_NODE_NOT_ENOUGH_SPACE;

	char* cell_left_edge = btu_calc_highwater_offset(
		node, node->header->cell_high_water_offset + cell_size);

	result = btree_cell_write_inline(cell, cell_left_edge, cell_size);
	if( result != BTREE_OK )
		return result;

	result = write_page_key(node, index, key, cell_size, flags);
	if( result != BTREE_OK )
		return result;

	return BTREE_OK;
}

/**
 * @brief Inserts overflow cell into a node.
 *
 * @param node
 * @param index
 * @param cell
 * @return enum btree_e
 */
enum btree_e
btree_node_insert_overflow(
	struct BTreeNode* node,
	struct InsertionIndex* index,
	u32 key,
	struct BTreeCellOverflow* cell)
{
	enum btree_e result = BTREE_OK;
	struct BufferWriter writer = {0};

	// This is correct; since the input cell contains the inline size, NOT the
	// inline payload size, we want the size of this cell as if it were inline.
	u32 cell_size = btree_cell_inline_disk_size(cell->inline_size);

	u32 heap_needed = btree_node_heap_required_for_insertion(cell_size);
	if( node->header->free_heap < heap_needed )
		return BTREE_ERR_NODE_NOT_ENOUGH_SPACE;

	char* cell_left_edge = btu_calc_highwater_offset(
		node, node->header->cell_high_water_offset + cell_size);

	btree_cell_init_overflow_writer(&writer, cell_left_edge, cell_size);

	result = btree_cell_write_overflow_ex(cell, &writer);
	if( result != BTREE_OK )
		return result;

	result = write_page_key(
		node,
		index,
		key,
		cell_size,
		btree_pkey_flags_set(0, PKEY_FLAG_CELL_TYPE_OVERFLOW));
	if( result != BTREE_OK )
		return result;

	return BTREE_OK;
}

// /**
//  * See header
//  */
enum btree_e
btree_node_remove(
	struct BTreeNode* node,
	struct ChildListIndex* index,
	struct BTreeCellInline* removed_cell,
	void* buffer,
	u32 buffer_size)
{
	struct CellData cell = {0};
	u32 index_number = 0;

	assert(index->mode != KLIM_END);

	if( index->mode == KLIM_RIGHT_CHILD )
	{
		// Must not be the case for leaf nodes.
		if( node->header->is_leaf )
			return BTREE_ERR_KEY_NOT_FOUND;

		// It is up to the btree alg to correctly
		// remove this node.
		// TODO: Throw here as we've failed to maintain some invariant?
		if( node->header->num_keys == 0 )
		{
			return BTREE_ERR_UNK;
		}

		// The rightmost non-right-child key becomes the right-child.
		char buf[sizeof(u32)] = {0};
		struct BTreeCellInline removed_cell = {0};
		struct BTreePageKey rightmost_key =
			node->keys[node->header->num_keys - 1];
		struct ChildListIndex delete_index = {0};
		delete_index.index = node->header->num_keys - 1;
		delete_index.mode = KLIM_INDEX;
		enum btree_e next_right_child_result = btree_node_remove(
			node, &delete_index, &removed_cell, buf, sizeof(buf));

		if( next_right_child_result != BTREE_OK )
			return BTREE_ERR_UNK;

		u32 child_value = 0;
		ser_read_32bit_le(&child_value, buf);

		node->header->right_child = child_value;

		return BTREE_OK;
	}

	index_number = index->index;

	btu_read_cell(node, index_number, &cell);
	int deleted_inline_size = btree_cell_get_size(&cell);

	if( buffer )
	{
		memcpy(buffer, cell.pointer, min(buffer_size, deleted_inline_size));
	}

	if( removed_cell )
	{
		removed_cell->inline_size = deleted_inline_size;
		removed_cell->payload = buffer;
	}

	// Slide keys over.
	memmove(
		&node->keys[index_number],
		&node->keys[index_number + 1],
		(node->header->num_keys - index_number) *
			sizeof(node->keys[index_number]));
	memset(&cell, 0x00, sizeof(cell));
	node->header->num_keys -= 1;

	// Garbage collection in the heap.
	// TODO: This is seriously flaky.
	for( int i = index_number; i < node->header->num_keys; i++ )
	{
		u32 offset = node->keys[i].cell_offset;
		btu_read_cell(node, index_number, &cell);
		char* src = btu_calc_highwater_offset(node, offset);
		char* dest = btu_calc_highwater_offset(
			node, offset - (deleted_inline_size + sizeof(u32)));
		memmove(
			(void*)dest, (void*)src, btree_cell_get_size(&cell) + sizeof(u32));
	}

	node->header->cell_high_water_offset -= deleted_inline_size + sizeof(u32);
	node->header->free_heap += btree_node_heap_required_for_insertion(
		deleted_inline_size + sizeof(u32));

	return BTREE_OK;
}

/**
 * See header
 */
u32
btree_node_heap_required_for_insertion(u32 cell_disk_size)
{
	return cell_disk_size + sizeof(struct BTreePageKey);
}

u32
btree_node_calc_heap_capacity(struct BTreeNode* node)
{
	u32 offset = node->page->page_id == 1 ? BTREE_HEADER_SIZE : 0;
	return node->page->page_size - sizeof(struct BTreePageHeader) - offset;
}