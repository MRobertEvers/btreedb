#include "btree.h"

#include "binary_search.h"

#include <stdlib.h>
#include <string.h>

#define BTREE_HEADER_SIZE 100

static int
get_node_size(struct BTreeNode* node)
{
	// 4kb
	return 0x1000 - (node->page_number == 1 ? BTREE_HEADER_SIZE : 0);
}

static char*
get_node_buffer(struct BTreeNode* node)
{
	return ((char*)node->page->page_buffer) +
		   (node->page_number == 1 ? BTREE_HEADER_SIZE : 0);
}

static int
calc_cell_size(int size)
{
	return size + sizeof(int);
}

static enum btree_e
insert_data_into_node(
	struct BTreeNode* node,
	int index,
	unsigned int key,
	void* data,
	int data_size)
{
	memmove(
		&node->keys[index + 1],
		&node->keys[index],
		(node->header->num_keys - index) * sizeof(node->keys[index]));

	node->keys[index].key = key;
	node->keys[index].cell_offset =
		node->header->cell_high_water_offset + calc_cell_size(data_size);
	node->header->num_keys += 1;

	char* cell = get_node_buffer(node) + get_node_size(node) -
				 node->keys[index].cell_offset;
	memcpy(cell, &data_size, sizeof(data_size));

	cell = cell + sizeof(data_size);
	memcpy(cell, data, data_size);
	node->header->cell_high_water_offset += calc_cell_size(data_size);

	return BTREE_OK;
}

static int
binary_search_keys(
	struct BTreePageKey* arr, unsigned char num_keys, int key, char* found)
{
	int left = 0;
	int right = num_keys - 1;
	int mid = 0;
	*found = 0;

	while( left <= right )
	{
		mid = (right - left) / 2 + left;

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

void
read_cell(struct BTreeNode* node, int index, struct CellData* cell)
{
	memset(cell, 0x00, sizeof(struct CellData));
	int offset = node->keys[index].cell_offset;

	char* cell_buffer = get_node_buffer(node) + get_node_size(node) - offset;

	cell->size = (int*)cell_buffer;
	cell->pointer = &cell->size[1];
}

/**
 * @brief B+ Tree split node algorithm
 *
 * Create a parent and right child node; keep a copy of the key in
 * the parent, keep a key and value in child
 *
 * @param tree
 * @param node
 * @return enum btree_e
 */
enum btree_e
split_node(struct BTree* tree, struct BTreeNode* node)
{
	// TODO: Optimize this
	// TODO: Free list
	struct BTreeNode* parent = NULL;
	struct BTreeNode* left = NULL;
	struct BTreeNode* right = NULL;

	struct Page* parent_page = NULL;
	struct Page* left_page = NULL;
	struct Page* right_page = NULL;

	page_create(tree->pager, node->page_number, &parent_page);
	btree_node_create_from_page(tree, &parent, parent_page);

	page_create(tree->pager, PAGE_CREATE_NEW_PAGE, &left_page);
	btree_node_create_from_page(tree, &left, left_page);

	page_create(tree->pager, PAGE_CREATE_NEW_PAGE, &right_page);
	btree_node_create_from_page(tree, &right, right_page);

	int first_half = ((node->header->num_keys + 1) / 2);
	for( int i = 0; i < node->header->num_keys; i++ )
	{
		struct CellData cell = {0};
		int key = node->keys[i].key;
		read_cell(node, i, &cell);
		if( i < first_half )
		{
			insert_data_into_node(left, i, key, cell.pointer, *cell.size);
		}
		else
		{
			insert_data_into_node(
				right, (i - first_half), key, cell.pointer, *cell.size);
		}
	}

	// We need to write the pages out to get the page ids.
	right->header->is_leaf = 1;
	left->header->is_leaf = 1;
	pager_write_page(tree->pager, left_page);
	pager_write_page(tree->pager, right_page);

	insert_data_into_node(
		parent,
		0,
		left->keys[left->header->num_keys - 1].key,
		&left_page->page_id,
		sizeof(unsigned int));

	parent->header->is_leaf = 0;

	parent->header->right_child = right_page->page_id;

	memcpy(
		get_node_buffer(node), get_node_buffer(parent), get_node_size(parent));

	pager_write_page(tree->pager, node->page);

end:
	btree_node_destroy(tree, left);
	page_destroy(tree->pager, left_page);

	btree_node_destroy(tree, right);
	page_destroy(tree->pager, right_page);

	btree_node_destroy(tree, parent);
	page_destroy(tree->pager, parent_page);

	return BTREE_OK;
}

static enum btree_e
init_new_root_page(struct BTree* tree, struct Page* page)
{
	struct BTreeNode temp_node = {0};

	// TODO: Hack need function to get page id before writing...
	// That way it wont be "PAGE_NEW" here.
	page->page_id = 1;
	btree_node_init_from_page(&temp_node, page);

	struct BTreeHeader* temp_btree_header =
		(struct BTreeHeader*)page->page_buffer;

	temp_node.header->is_leaf = 1;

	temp_btree_header->page_high_water = 2;

	page->page_id = PAGE_CREATE_NEW_PAGE;

	return BTREE_OK;
}

static enum btree_e
btree_init_root_page(struct BTree* tree, struct Page* page)
{
	enum pager_e pager_status = PAGER_OK;

	pager_status = pager_read_page(tree->pager, page);
	if( pager_status == PAGER_ERR_NIF )
	{
		page_reselect(page, PAGE_CREATE_NEW_PAGE);

		init_new_root_page(tree, page);

		pager_status = pager_write_page(tree->pager, page);
		if( pager_status == PAGER_OK )
		{
			return BTREE_OK;
		}
		else
		{
			return BTREE_UNK_ERR;
		}
	}
	else if( pager_status != PAGER_OK )
	{
		return BTREE_UNK_ERR;
	}
	else
	{
		return BTREE_OK;
	}
}

static enum btree_e
btree_node_alloc(struct BTree* tree, struct BTreeNode** r_node)
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

	return BTREE_OK;
}

enum btree_e
btree_node_create_from_page(
	struct BTree* tree, struct BTreeNode** r_node, struct Page* page)
{
	btree_node_alloc(tree, r_node);
	btree_node_init_from_page(*r_node, page);

	return BTREE_OK;
}

enum btree_e
btree_node_destroy(struct BTree* tree, struct BTreeNode* node)
{
	btree_node_deinit(node);
	btree_node_dealloc(node);
	return BTREE_OK;
}

enum btree_e
btree_alloc(struct BTree** r_tree)
{
	*r_tree = (struct BTree*)malloc(sizeof(struct BTree));
	return BTREE_OK;
}

enum btree_e
btree_dealloc(struct BTree* tree)
{
	free(tree);
	return BTREE_OK;
}

/**
 * @brief Load the root page into memory and read out information from the btree
 * header.
 *
 * @param tree
 * @return enum btree_e
 */

enum btree_e
btree_init(struct BTree* tree, struct Pager* pager)
{
	enum btree_e btree_result = BTREE_OK;

	tree->root_page_id = 1;
	tree->pager = pager;

	struct Page* page = NULL;
	page_create(tree->pager, 1, &page);
	btree_result = btree_init_root_page(tree, page);
	if( btree_result != BTREE_OK )
		return BTREE_UNK_ERR;

	tree->header = *(struct BTreeHeader*)page->page_buffer;

	return BTREE_OK;
}

enum btree_e
btree_deinit(struct BTree* tree)
{
	// TODO: Nodes?
	return BTREE_OK;
}

struct Cursor*
create_cursor(struct BTree* tree)
{
	struct Cursor* cursor = (struct Cursor*)malloc(sizeof(struct Cursor));

	cursor->tree = tree;
	cursor->current_key = 0;
	cursor->current_page_id = tree->root_page_id;
	return cursor;
}

void
destroy_cursor(struct Cursor* cursor)
{
	free(cursor);
}

enum btree_e
btree_insert(struct BTree* tree, int key, void* data, int data_size)
{
	enum btree_e result = BTREE_OK;
	int index = 0;
	char found = 0;

	struct Cursor* cursor = create_cursor(tree);

	index = btree_traverse_to(cursor, key, &found);

	struct Page* page = NULL;
	struct BTreeNode* node = NULL;
	page_create(tree->pager, cursor->current_page_id, &page);
	pager_read_page(tree->pager, page);
	btree_node_create_from_page(tree, &node, page);

	result =
		insert_data_into_node(node, cursor->current_key, key, data, data_size);

	pager_write_page(tree->pager, page);

	btree_node_destroy(tree, node);
	page_destroy(tree->pager, page);

	return BTREE_OK;
}

enum btree_e
btree_traverse_to(struct Cursor* cursor, int key, char* found)
{
	int index = 0;
	struct Page* page = NULL;
	struct BTreeNode node = {0};
	struct CellData cell = {0};

	page_create(cursor->tree->pager, cursor->current_page_id, &page);
	do
	{
		page_reselect(page, cursor->current_page_id);
		pager_read_page(cursor->tree->pager, page);
		btree_node_init_from_page(&node, page);

		index =
			binary_search_keys(node.keys, node.header->num_keys, key, found);

		if( node.header->is_leaf )
		{
			cursor->current_key = index;
		}
		else
		{
			if( index == node.header->num_keys )
			{
				cursor->current_page_id = node.header->right_child;
			}
			else
			{
				read_cell(&node, index, &cell);
				memcpy(&cursor->current_page_id, cell.pointer, *cell.size);
			}
		}
	} while( !node.header->is_leaf );

	page_destroy(cursor->tree->pager, page);

	return BTREE_OK;
}