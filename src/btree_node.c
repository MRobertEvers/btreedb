#include "btree_node.h"

#include "btree_cell.h"
#include "btree_utils.h"

#include <assert.h>
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
	char* byte_buffer = (char*)page->page_buffer;
	void* data = byte_buffer + offset;

	node->page = page;
	node->page_number = page->page_id;
	node->header = (struct BTreePageHeader*)data;
	// Trick to get a pointer to the address immediately after the header.
	node->keys = (struct BTreePageKey*)&node->header[1];

	if( node->header->num_keys == 0 )
		node->header->free_heap =
			node->page->page_size - sizeof(struct BTreePageHeader) - offset;

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
	struct InsertionIndex* index,
	unsigned int key,
	void* data,
	int data_size)
{
	assert(index->mode != KLIM_RIGHT_CHILD);

	unsigned int index_number = 0;

	index_number =
		index->mode == KLIM_END ? node->header->num_keys : index->index;

	// Size check
	int size_needed =
		btu_calc_cell_size(data_size) + sizeof(struct BTreePageKey);
	if( node->header->free_heap < size_needed )
		return BTREE_ERR_NODE_NOT_ENOUGH_SPACE;

	// The Raw insertion
	memmove(
		&node->keys[index_number + 1],
		&node->keys[index_number],
		(node->header->num_keys - index_number) *
			sizeof(node->keys[index_number]));

	node->keys[index_number].key = key;
	node->keys[index_number].cell_offset =
		node->header->cell_high_water_offset + btu_calc_cell_size(data_size);
	node->header->num_keys += 1;

	char* cell =
		btu_calc_highwater_offset(node, node->keys[index_number].cell_offset);
	memcpy(cell, &data_size, sizeof(data_size));

	cell = cell + sizeof(data_size);
	memcpy(cell, data, data_size);
	node->header->cell_high_water_offset += btu_calc_cell_size(data_size);

	node->header->free_heap -= size_needed;

	return BTREE_OK;
}

static int
write_partial(
	struct BTreeNodeWriterState* state, void* data, unsigned int data_size)
{
	// TODO: Bounds check.
	char* left_edge = (char*)state->buffer_right_edge;
	left_edge -= data_size;

	memcpy(left_edge, data, data_size);

	state->buffer_right_edge = left_edge;

	return data_size;
}

/**
 * See header
 */
enum btree_e
btree_node_insert_ex(
	struct BTreeNode* node,
	struct InsertionIndex* index,
	unsigned int key,
	btree_node_writer_t writer,
	void* writer_data,
	enum btree_cell_flag_e flags)
{
	assert(index->mode != KLIM_RIGHT_CHILD);

	unsigned int index_number = 0;
	struct BTreeNodeWriterState writer_state = {0};

	char* cell_right_edge =
		btu_calc_highwater_offset(node, node->header->cell_high_water_offset);
	writer_state.buffer_right_edge = cell_right_edge;

	unsigned int written_size = writer(
		writer_data, write_partial, &writer_state, node->header->free_heap);

	assert(written_size > 0);
	unsigned int cell_size_buffer = written_size;
	btree_cell_set_flag(&cell_size_buffer, flags);
	write_partial(&writer_state, &cell_size_buffer, sizeof(unsigned int));

	// Insert the key.
	index_number =
		index->mode == KLIM_END ? node->header->num_keys : index->index;
	memmove(
		&node->keys[index_number + 1],
		&node->keys[index_number],
		(node->header->num_keys - index_number) *
			sizeof(node->keys[index_number]));

	node->keys[index_number].key = key;
	node->keys[index_number].cell_offset =
		node->header->cell_high_water_offset + btu_calc_cell_size(written_size);
	node->header->num_keys += 1;

	node->header->cell_high_water_offset += btu_calc_cell_size(written_size);

	node->header->free_heap -=
		btree_node_get_heap_required_for_insertion(written_size);

	return BTREE_OK;
}

/**
 * See header
 */
enum btree_e
btree_node_delete(struct BTreeNode* node, struct ChildListIndex* index)
{
	struct CellData cell = {0};
	unsigned int index_number = 0;

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
		struct BTreePageKey rightmost_key =
			node->keys[node->header->num_keys - 1];
		struct ChildListIndex delete_index = {0};
		delete_index.index = node->header->num_keys - 1;
		delete_index.mode = KLIM_INDEX;
		enum btree_e next_right_child_result =
			btree_node_delete(node, &delete_index);
		if( next_right_child_result != BTREE_OK )
			return BTREE_ERR_UNK;

		node->header->right_child = rightmost_key.key;

		return BTREE_OK;
	}

	index_number = index->index;

	btu_read_cell(node, index_number, &cell);
	int deleted_cell_size = btree_cell_get_size(&cell);

	// Slide keys over.
	memmove(
		&node->keys[index_number],
		&node->keys[index_number + 1],
		(node->header->num_keys - index_number) *
			sizeof(node->keys[index_number]));
	memset(&cell, 0x00, sizeof(cell));
	node->header->num_keys -= 1;

	// Garbage collection in the heap.
	int old_highwater = node->header->cell_high_water_offset;
	char* src = btu_calc_highwater_offset(node, old_highwater);
	char* dest =
		btu_calc_highwater_offset(node, old_highwater - deleted_cell_size);
	memmove((void*)dest, (void*)src, deleted_cell_size);

	node->header->cell_high_water_offset -= deleted_cell_size;

	return BTREE_OK;
}

/**
 * See header
 */
unsigned int
btree_node_get_heap_required_for_insertion(unsigned int cell_size)
{
	return cell_size + sizeof(struct BTreePageKey);
}

/**
 * See header
 */
int
btree_node_arity(struct BTreeNode* node)
{
	if( node->header->is_leaf )
	{
		return node->header->num_keys;
	}
	else
	{
		return node->header->num_keys +
			   (node->header->right_child != 0 ? 1 : 0);
	}
}