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
	return 0x1000 - (node->page_number == 1 ? BTREE_HEADER_SIZE : 0);
}

char*
btu_get_node_buffer(struct BTreeNode* node)
{
	return ((char*)node->page->page_buffer) +
		   (node->page_number == 1 ? BTREE_HEADER_SIZE : 0);
}

int
btu_calc_cell_size(int size)
{
	return size + sizeof(int);
}

int
btu_binary_search_keys(
	struct BTreePageKey* arr, unsigned char num_keys, int key, char* found)
{
	int left = 0;
	int right = num_keys - 1;
	int mid = 0;
	*found = 0;

	while( left <= right )
	{
		mid = (right - left) / 2 + left;

		// TODO: Key compare function.
		if( arr[mid].key == key )
		{
			if( found )
				*found = 1;
			return mid;
		}
		else if( arr[mid].key < key )
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
