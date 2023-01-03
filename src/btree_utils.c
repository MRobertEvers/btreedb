#include "btree_utils.h"

#include <string.h>

void
btu_read_cell(struct BTreeNode* node, int index, struct CellData* cell)
{
	memset(cell, 0x00, sizeof(struct CellData));
	int offset = node->keys[index].cell_offset;

	char* cell_buffer =
		btu_get_node_buffer(node) + btu_get_node_size(node) - offset;

	cell->size = (int*)cell_buffer;
	cell->pointer = &cell->size[1];
}

int
btu_get_node_size(struct BTreeNode* node)
{
	// 4kb
	return node->page->page_size -
		   (node->page_number == 1 ? BTREE_HEADER_SIZE : 0);
}

char*
btu_get_node_buffer(struct BTreeNode* node)
{
	return ((char*)node->page->page_buffer) +
		   (node->page_number == 1 ? BTREE_HEADER_SIZE : 0);
}

/**
 * See header for details.
 */
char*
btu_calc_highwater_offset(struct BTreeNode* node, int highwater)
{
	char* base = btu_get_node_buffer(node);
	base += btu_get_node_size(node);
	base -= highwater;
	return base;
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
	unsigned int mid_key = 0;
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

int
btu_init_keylistindex_from_index(
	struct KeyListIndex* keylistindex, struct BTreeNode const* node, int index)
{
	if( index < node->header->num_keys )
	{
		keylistindex->mode = KLIM_INDEX;
		keylistindex->index = index;
	}
	else if( !node->header->is_leaf )
	{
		keylistindex->mode = KLIM_RIGHT_CHILD;
		keylistindex->index = 0;
	}
	else
	{
		keylistindex->mode = KLIM_END;
		keylistindex->index = 0;
	}

	return 0;
}