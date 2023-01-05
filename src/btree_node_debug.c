#include "btree_node_debug.h"

#include "btree_utils.h"

#include <stdio.h>
#include <string.h>

void
dbg_print_node(struct BTreeNode* node)
{
	printf(
		"Page %d (leaf? %d) (heap? %d) (off? %d): ",
		node->page_number,
		node->header->is_leaf,
		node->header->free_heap,
		node->header->cell_high_water_offset);
	for( int i = 0; i < node->header->num_keys && i < 10; i++ )
	{
		struct CellData cell = {0};

		int key = node->keys[i].key;
		btu_read_cell(node, i, &cell);
		unsigned int page_id = 0;
		memcpy(&page_id, cell.pointer, sizeof(page_id));
		printf("%d (p.%u), ", node->keys[i].key, page_id);
	}
	if( !node->header->is_leaf )
	{
		printf(";%d", node->header->right_child);
	}
	printf("\n");
}

void
dbg_print_buffer(void const* buffer, unsigned int buffer_size)
{
	unsigned char const* p = buffer;
	for( int i = 0; i < buffer_size; i++ )
	{
		unsigned char c = p[i];

		if( 32 <= c && c <= 126 )
			printf("%c", c);
		else
			printf("\\x%02x", c);
	}
	printf("\n");
}