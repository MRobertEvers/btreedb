#include "btree_node_debug.h"

#include "btree_utils.h"

#include <stdio.h>

void
dbg_print_node(struct BTreeNode* node)
{
	printf(
		"Page %d (leaf? %d) (heap? %d) (off? %d): ",
		node->page_number,
		node->header->is_leaf,
		node->header->free_heap,
		node->header->cell_high_water_offset);
	for( int i = 0; i < node->header->num_keys; i++ )
	{
		struct CellData cell = {0};

		int key = node->keys[i].key;
		btu_read_cell(node, i, &cell);
		printf("%d (p.%d), ", node->keys[i], *(int*)cell.pointer);
	}
	if( !node->header->is_leaf )
	{
		printf(";%d", node->header->right_child);
	}
	printf("\n");
}