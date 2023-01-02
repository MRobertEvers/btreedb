#include "btree_node.h"

#include "btree_utils.h"

#include <stdlib.h>
#include <string.h>

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

/**
 * See header for details.
 */
enum btree_e
btree_node_init_from_page(struct BTreeNode* node, struct Page* page)
{
	int offset = page->page_id == 1 ? BTREE_HEADER_SIZE : 0;
	void* data = ((char*)page->page_buffer) + offset;

	node->page = page;
	node->page_number = page->page_id;
	node->header = (struct BTreePageHeader*)data;
	// Trick to get a pointer to the address immediately after the header.
	node->keys = (struct BTreePageKey*)&node->header[1];

	// TODO: Don't hardcode page size
	if( node->header->num_keys == 0 )
		node->header->free_heap = 0x1000 - sizeof(struct BTreePageHeader);

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
 * See header for details.
 */
enum btree_e
btree_node_insert(
	struct BTreeNode* node,
	int index,
	unsigned int key,
	void* data,
	int data_size)
{
	// Size check
	int size_needed =
		btu_calc_cell_size(data_size) + sizeof(struct BTreePageKey);
	if( node->header->free_heap < size_needed )
		return BTREE_ERR_NODE_NOT_ENOUGH_SPACE;
	node->header->free_heap -= size_needed;

	// The Raw insertion
	memmove(
		&node->keys[index + 1],
		&node->keys[index],
		(node->header->num_keys - index) * sizeof(node->keys[index]));

	node->keys[index].key = key;
	node->keys[index].cell_offset =
		node->header->cell_high_water_offset + btu_calc_cell_size(data_size);
	node->header->num_keys += 1;

	char* cell = btu_get_node_buffer(node) + btu_get_node_size(node) -
				 node->keys[index].cell_offset;
	memcpy(cell, &data_size, sizeof(data_size));

	cell = cell + sizeof(data_size);
	memcpy(cell, data, data_size);
	node->header->cell_high_water_offset += btu_calc_cell_size(data_size);

	return BTREE_OK;
}