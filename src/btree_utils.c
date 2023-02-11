#include "btree_utils.h"

#include "serialization.h"

#include <string.h>

enum btree_e
btpage_err(enum pager_e err)
{
	switch( err )
	{
	case PAGER_OK:
		return BTREE_OK;
	case PAGER_ERR_NO_MEM:
		return BTREE_ERR_NO_MEM;
	default:
		return BTREE_ERR_PAGING;
	}
}

/**
 * @deprecated
 *
 * @param node
 * @param index
 * @param cell
 */
void
btu_read_cell(struct BTreeNode* node, int index, struct CellData* cell)
{
	memset(cell, 0x00, sizeof(struct CellData));
	int offset = node->keys[index].cell_offset;

	byte* cell_buffer = btu_calc_highwater_offset(node, offset);
	// btu_get_node_buffer(node) + btu_get_node_size(node) - offset;

	cell->size = (int*)cell_buffer;
	cell->pointer = &cell->size[1];
}

int
btu_get_node_heap_size(struct BTreeNode* node)
{
	return btu_get_node_size(node) - sizeof(struct BTreePageHeader);
}

int
btu_get_node_size(struct BTreeNode* node)
{
	// 4kb
	return node->page->page_size -
		   (node->page_number == 1 ? BTREE_HEADER_SIZE : 0);
}

byte*
btu_get_node_buffer(struct BTreeNode* node)
{
	return ((byte*)node->page->page_buffer) +
		   (node->page_number == 1 ? BTREE_HEADER_SIZE : 0);
}

/**
 * See header for details.
 */
byte*
btu_calc_highwater_offset(struct BTreeNode* node, int highwater)
{
	byte* base = btu_get_node_buffer(node);
	base += btu_get_node_size(node);
	base -= highwater;
	return base;
}

/**
 * See header for details.
 */
byte*
btu_get_cell_buffer(struct BTreeNode* node, int index)
{
	u32 offset = node->keys[index].cell_offset;

	byte* cell_buffer = btu_calc_highwater_offset(node, offset);

	return cell_buffer;
}

/**
 * See header for details.
 */
u32
btu_get_cell_buffer_size(struct BTreeNode* node, int index)
{
	byte* buffer = btu_get_cell_buffer(node, index);

	u32 inline_size = 0;
	ser_read_32bit_le(&inline_size, buffer);

	// The size of the cell does not include the size value itself, so add it
	// here.
	return inline_size + sizeof(u32);
}

/**
 * See header for details.
 */
u32
btu_get_cell_flags(struct BTreeNode* node, int index)
{
	// TODO: This is here for debug.
	struct BTreePageKey key = node->keys[index];
	u32 flags = key.flags;

	return flags;
}

/**
 * See header for details.
 */
int
btu_calc_cell_size(int size)
{
	return size + sizeof(int);
}

/**
 * See header for details.
 */
int
btu_binary_search_keys(
	struct BTreePageKey* arr, unsigned char num_keys, int key, char* found)
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

void
btu_init_keylistindex_from_index(
	struct ChildListIndex* keylistindex,
	struct BTreeNode const* node,
	int index)
{
	if( index < node->header->num_keys )
	{
		keylistindex->mode = KLIM_INDEX;
		keylistindex->index = index;
	}
	else if( !node->header->is_leaf )
	{
		keylistindex->mode = KLIM_RIGHT_CHILD;
		keylistindex->index = node->header->num_keys;
	}
	else
	{
		keylistindex->mode = KLIM_END;
		keylistindex->index = node->header->num_keys;
	}
}

void
btu_init_insertion_index_from_index(
	struct InsertionIndex* keylistindex,
	struct BTreeNode const* node,
	int index)
{
	if( index < node->header->num_keys )
	{
		keylistindex->mode = KLIM_INDEX;
		keylistindex->index = index;
	}
	else
	{
		keylistindex->mode = KLIM_END;
		keylistindex->index = node->header->num_keys;
	}
}
