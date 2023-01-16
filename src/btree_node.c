#include "btree_node.h"

#include "btree_cell.h"
#include "btree_node_debug.h"
#include "btree_overflow.h"
#include "btree_utils.h"
#include "serialization.h"

#include <assert.h>
#include <stdbool.h>
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

static u32
cell_type_code(enum btree_page_key_flags_e flag)
{
	switch( flag )
	{
	case PKEY_FLAG_CELL_TYPE_INLINE:
		return 1;
	case PKEY_FLAG_CELL_TYPE_OVERFLOW:
		return 2;
	default:
		return 0;
	}
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
btree_pkey_set_cell_type(u32 flags, enum btree_page_key_flags_e flag)
{
	u32 code = cell_type_code(flag);
	if( code == 0 )
		return flags;

	assert(code < (1 << 4));
	return flags | (code);
}

/**
 * @brief Returns 1 if flag is present; 0 otherwise
 *
 * @param flags
 * @return u32
 */
u32
btree_pkey_is_cell_type(u32 flags, enum btree_page_key_flags_e flag)
{
	u32 test = cell_type_code(flag);
	if( test == 0 )
		return 0;

	return (flags & 0xF) == test;
}

/**
 * See header for details.
 */
enum btree_e
btree_node_init_from_page(struct BTreeNode* node, struct Page* page)
{
	u32 offset = page->page_id == 1 ? BTREE_HEADER_SIZE : 0;
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

enum btree_e
btree_node_init_from_read(
	struct BTreeNode* node, struct Page* page, struct Pager* pager, u32 page_id)
{
	enum btree_e result = BTREE_OK;
	struct PageSelector selector = {0};
	pager_reselect(&selector, page_id);

	result = btpage_err(pager_read_page(pager, &selector, page));
	if( result != BTREE_OK )
		return result;

	result = btree_node_init_from_page(node, page);
	if( result != BTREE_OK )
		return result;

	return result;
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
	assert(array_size >= index_number);
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
		btree_pkey_set_cell_type(0, PKEY_FLAG_CELL_TYPE_INLINE));
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

	byte* cell_left_edge = btu_calc_highwater_offset(
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

	byte* cell_left_edge = btu_calc_highwater_offset(
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
		btree_pkey_set_cell_type(0, PKEY_FLAG_CELL_TYPE_OVERFLOW));
	if( result != BTREE_OK )
		return result;

	return BTREE_OK;
}

enum btree_e
btree_node_move_cell(
	struct BTreeNode* source_node,
	struct BTreeNode* other,
	u32 source_index,
	struct Pager* pager)
{
	return btree_node_move_cell_ex(
		source_node,
		other,
		source_index,
		source_node->keys[source_index].key,
		pager);
}

enum btree_e
btree_node_move_cell_ex(
	struct BTreeNode* source_node,
	struct BTreeNode* other,
	u32 source_index,
	u32 new_key,
	struct Pager* pager)
{
	struct InsertionIndex insert_end = {.mode = KLIM_END};

	return btree_node_move_cell_ex_to(
		source_node, other, source_index, &insert_end, new_key, pager);
}

enum btree_e
btree_node_move_cell_ex_to(
	struct BTreeNode* source_node,
	struct BTreeNode* dest_node,
	u32 source_index,
	struct InsertionIndex* dest_index,
	u32 new_key,
	struct Pager* pager)
{
	u32 flags = source_node->keys[source_index].flags;

	byte* cell_data = btu_get_cell_buffer(source_node, source_index);
	u32 cell_data_size = btu_get_cell_buffer_size(source_node, source_index);

	return btree_node_move_cell_from_data(
		dest_node,
		dest_index,
		new_key,
		flags,
		cell_data,
		cell_data_size,
		pager);
}

enum btree_e
btree_node_move_cell_from_data(
	struct BTreeNode* dest_node,
	struct InsertionIndex* insert_index,
	u32 key,
	u32 flags,
	byte* cell_buffer,
	u32 cell_buffer_size,
	struct Pager* pager)
{
	enum btree_e result = BTREE_OK;

	struct BTreeCellInline cell = {0};
	btree_cell_read_inline(cell_buffer, cell_buffer_size, &cell, NULL, 0);

	u32 dest_max_size = btree_node_max_cell_size(dest_node);
	if( cell_buffer_size <= dest_max_size )
	{
		result = btree_node_insert_inline_ex(
			dest_node, insert_index, key, &cell, flags);
	}
	else
	{
		// Assumes that a btree can only be created that
		// restricts the min cell size to be greater than
		// the size required to fit an overflow cell.
		u32 is_overflow =
			btree_pkey_is_cell_type(flags, PKEY_FLAG_CELL_TYPE_OVERFLOW);

		u32 follow_page_id = 0;
		u32 bytes_overflown =
			btree_node_heap_required_for_insertion(cell_buffer_size) -
			dest_max_size;

		char* overflow_payload = NULL;
		char* payload = NULL;
		u32 overflow_payload_size = 0;
		u32 inline_size = 0;

		if( is_overflow )
		{
			// Overflow page -> Overflow page.
			struct BTreeCellOverflow read_cell = {0};
			struct BufferReader reader = {0};
			btree_cell_init_overflow_reader(
				&reader, cell_buffer, cell_buffer_size);

			btree_cell_read_overflow_ex(&reader, &read_cell, NULL, 0);
			u32 previous_inline_payload_size =
				btree_cell_overflow_calc_inline_payload_size(cell.inline_size);
			inline_size = cell.inline_size - bytes_overflown;
			payload = (char*)read_cell.inline_payload;
			u32 new_inline_payload_size =
				btree_cell_overflow_calc_inline_payload_size(inline_size);
			overflow_payload = payload + new_inline_payload_size;
			overflow_payload_size =
				previous_inline_payload_size - new_inline_payload_size;
			follow_page_id = read_cell.overflow_page_id;
		}
		else
		{
			inline_size = cell.inline_size - bytes_overflown;
			payload = cell.payload;
			u32 new_inline_payload_size =
				btree_cell_overflow_calc_inline_payload_size(inline_size);
			overflow_payload = cell.payload + new_inline_payload_size;
			overflow_payload_size = cell.inline_size - new_inline_payload_size;
			follow_page_id = 0;
		}

		struct BTreeOverflowWriteResult write_result = {0};
		result = btree_overflow_write(
			pager,
			overflow_payload,
			overflow_payload_size,
			follow_page_id,
			&write_result);
		if( result != BTREE_OK )
			goto end;

		struct BTreeCellOverflow write_cell = {0};
		write_cell.inline_payload = payload;
		write_cell.inline_size = inline_size;
		write_cell.overflow_page_id = write_result.page_id;
		write_cell.total_size = cell.inline_size;

		result = btree_node_insert_overflow(
			dest_node, insert_index, key, &write_cell);
	}

end:
	return result;
}

enum btree_e
btree_node_copy(struct BTreeNode* dest_node, struct BTreeNode* src_node)
{
	memcpy(
		btu_get_node_buffer(dest_node),
		btu_get_node_buffer(src_node),
		btu_get_node_size(src_node));

	return btree_node_init_from_page(dest_node, dest_node->page);
}

enum btree_e
btree_node_reset(struct BTreeNode* node)
{
	memset(btu_get_node_buffer(node), 0x00, btu_get_node_size(node));

	return btree_node_init_from_page(node, node->page);
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
		byte* src = btu_calc_highwater_offset(node, offset);
		byte* dest = btu_calc_highwater_offset(
			node, offset - (deleted_inline_size + sizeof(u32)));
		memmove(
			(void*)dest, (void*)src, btree_cell_get_size(&cell) + sizeof(u32));
	}

	node->header->cell_high_water_offset -= deleted_inline_size + sizeof(u32);
	node->header->free_heap += btree_node_heap_required_for_insertion(
		deleted_inline_size + sizeof(u32));

	return BTREE_OK;
}

enum btree_e
ibtree_node_remove(
	struct BTreeNode* node,
	struct ChildListIndex* index,
	struct BTreeNode* holding_node)
{
	enum btree_e result = BTREE_OK;
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
		struct BTreeCellInline removed_cell = {0};
		struct BTreePageKey rightmost_key =
			node->keys[node->header->num_keys - 1];
		u32 child_value = rightmost_key.key;

		struct ChildListIndex delete_index = {0};
		delete_index.index = node->header->num_keys - 1;
		delete_index.mode = KLIM_INDEX;
		enum btree_e next_right_child_result =
			ibtree_node_remove(node, &delete_index, holding_node);

		if( next_right_child_result != BTREE_OK )
			return next_right_child_result;

		node->header->right_child = child_value;

		return BTREE_OK;
	}

	index_number = index->index;

	if( holding_node )
	{
		result = btree_node_move_cell(node, holding_node, index_number, NULL);
		if( result != BTREE_OK )
			return result;
	}

	btu_read_cell(node, index_number, &cell);
	int deleted_inline_size = btree_cell_get_size(&cell);

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
		byte* src = btu_calc_highwater_offset(node, offset);
		byte* dest = btu_calc_highwater_offset(
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

/**
 * See header for details.
 */
int
btu_binary_search_node_keys(
	struct BTreeNode* node,
	struct BTreePageKey* arr,
	unsigned char num_keys,
	int key,
	char* found)
{
	int left = 0;
	int right = num_keys - 1;
	int mid = 0;
	u32 mid_key = 0;
	*found = 0;

	while( left <= right )
	{
		mid = (right - left) / 2 + left;
		mid_key = arr[mid].key;

		// TODO: Key compare function.
		if( mid_key == key )
		{
			if( found )
				*found = 1;
			return mid;
		}
		else if( mid_key < key )
		{
			left = mid + 1;
		}
		else
		{
			right = mid - 1;
		}
	}

	return left;
}

// TODO: Return error
// TODO: Might have to read onto overflow page...
static byte*
get_cell_payload(
	struct BTreeNode* node,
	u32 index,
	u32* out_size,
	u32* out_total_size,
	u32* out_follow_page)
{
	struct BTreePageKey* key = &node->keys[index];
	bool is_overflow =
		btree_pkey_is_cell_type(key->flags, PKEY_FLAG_CELL_TYPE_OVERFLOW);

	byte* cell_data = btu_get_cell_buffer(node, index);
	u32 cell_data_size = btu_get_cell_buffer_size(node, index);

	byte* payload = NULL;
	*out_size = 0;
	*out_total_size = 0;
	*out_follow_page = 0;
	if( is_overflow )
	{
		// Overflow page -> Overflow page.
		struct BTreeCellOverflow read_cell = {0};
		struct BufferReader reader = {0};
		btree_cell_init_overflow_reader(&reader, cell_data, cell_data_size);

		btree_cell_read_overflow_ex(&reader, &read_cell, NULL, 0);

		payload = (byte*)read_cell.inline_payload;
		*out_size =
			btree_cell_overflow_calc_inline_payload_size(read_cell.inline_size);
		*out_total_size = read_cell.total_size;
		*out_follow_page = read_cell.overflow_page_id;
	}
	else
	{
		struct BTreeCellInline cell = {0};
		btree_cell_read_inline(cell_data, cell_data_size, &cell, NULL, 0);

		payload = (byte*)cell.payload;
		*out_size = cell.inline_size;
		*out_total_size = cell.inline_size;
	}

	return payload;
}

// Returns 1 if key is less than index
// 0 if equal
// -1 if index is less.
enum btree_e
btree_node_compare_cell(
	struct BTree* tree,
	struct BTreeNode* node,
	u32 index,
	void* key,
	u32 key_size,
	int* out_result)
{
	u32 bytes_compared = 0;
	enum btree_e result = BTREE_OK;
	u32 cmp_total_size = 0;
	u32 next_page_id = 0;
	u32 cmp_size = 0;
	u32 comparison_bytes_count = 0;

	byte* cmp = NULL;
	cmp = get_cell_payload(
		node, index, &cmp_size, &cmp_total_size, &next_page_id);

	*out_result = tree->compare(
		cmp, cmp_size, key, key_size, bytes_compared, &comparison_bytes_count);
	bytes_compared += comparison_bytes_count;

	if( *out_result != 0 )
	{
		return BTREE_OK;
	}
	else
	{
		if( next_page_id == 0 )
			return BTREE_OK;
		// We have to go to overflow pages to find out.
		struct Page* page = NULL;
		struct BTreeOverflowReadResult ov = {0};

		result = btpage_err(page_create(tree->pager, &page));
		if( result != BTREE_OK )
			goto end_overflow;

		do
		{
			result =
				btree_overflow_peek(tree->pager, page, next_page_id, &cmp, &ov);
			if( result != BTREE_OK )
				goto end_overflow;

			cmp_size = ov.payload_bytes;
			next_page_id = ov.next_page_id;

			*out_result = tree->compare(
				cmp,
				cmp_size,
				key,
				key_size,
				bytes_compared,
				&comparison_bytes_count);

			bytes_compared += comparison_bytes_count;

			if( *out_result != 0 )
				break;

		} while( next_page_id != 0 && bytes_compared < key_size );

		// If they were equal all the way up to the end
		if( *out_result == 0 )
		{
			// There are no more pages and there are key bytes left.
			if( bytes_compared < key_size )
			{
				// Key is less than because it is longer than cmp.
				*out_result = 1;
			}
			// There are no more key bytes and there are pages left.
			else if( (next_page_id != 0 || comparison_bytes_count != cmp_size) )
			{
				// Key is less than than because it is shorter than cmp
				*out_result = -1;
			}
			else
			{
				// Keys are same length;
				*out_result = 0;
			}
		}

	end_overflow:
		if( page )
			page_destroy(tree->pager, page);
	}
done:
	return result;
}

enum btree_e
btree_node_search_keys(
	struct BTree* tree,
	struct BTreeNode* node,
	void* key,
	u32 key_size,
	u32* out_index)
{
	assert(tree->compare != NULL);
	u32 num_keys = node->header->num_keys;
	enum btree_e result = BTREE_OK;
	int left = 0;
	int right = num_keys - 1;
	int mid = 0;
	int compare_result = 0;

	while( left <= right )
	{
		mid = (right - left) / 2 + left;

		result = btree_node_compare_cell(
			tree, node, mid, key, key_size, &compare_result);
		if( result != BTREE_OK )
			goto err;

		if( compare_result == 0 )
		{
			*out_index = mid;
			return BTREE_OK;
		}
		else if( compare_result == -1 )
		{
			left = mid + 1;
		}
		else
		{
			right = mid - 1;
		}
	}

	*out_index = left;
	return BTREE_ERR_KEY_NOT_FOUND;

err:
	return result;
}