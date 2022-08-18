#include "btree.h"

#include "binary_search.h"

#include <stdlib.h>
#include <string.h>

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

		node->header->cell_high_water_offset +
		data_size
		// Include int in size to record length of data.
		+ sizeof(int);
	node->header->num_keys += 1;

	// TODO: Page Size
	char* cell = ((char*)node->page->page_buffer) + 0x1000 -
				 node->keys[index].cell_offset;
	memcpy(cell, &data_size, sizeof(data_size));

	cell = cell + sizeof(data_size);
	memcpy(cell, data, data_size);
	node->header->cell_high_water_offset += data_size;

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

static enum btree_e
btree_page_new(struct BTree* tree, struct Page** r_page, int page_number)
{
	enum pager_e pager_status = PAGER_OK;

	pager_status = pager_page_alloc(tree->pager, r_page);
	if( pager_status != PAGER_OK )
		return BTREE_UNK_ERR;

	pager_status = pager_page_init(tree->pager, *r_page, page_number);
	if( pager_status != PAGER_OK )
		return BTREE_UNK_ERR;

	pager_status = pager_read_page(tree->pager, *r_page);
	if( pager_status != PAGER_OK )
		return BTREE_UNK_ERR;

	return BTREE_OK;
}

static enum btree_e
btree_load_root(struct BTree* tree)
{
	enum btree_e result = BTREE_OK;

	struct Page* page = 0;
	result = btree_page_new(tree, &page, 1);
	if( result != BTREE_OK )
		return result;

	result = btree_node_alloc(tree, &tree->root);
	if( result != BTREE_OK )
		return result;

	result = btree_node_init_from_page(tree, page, tree->root);
	if( result != BTREE_OK )
		return result;

	return BTREE_OK;
}

enum btree_e
btree_node_alloc(struct BTree* tree, struct BTreeNode** r_node)
{
	*r_node = (struct BTreeNode*)malloc(sizeof(struct BTreeNode));
	return BTREE_OK;
}

enum btree_e
btree_node_init_from_page(
	struct BTree* tree, struct Page* page, struct BTreeNode* node)
{
	unsigned char* data = page->page_buffer;
	node->page = page;

	node->header = (struct BTreePageHeader*)page->page_buffer;
	// Trick to get a pointer to the address immediately after the header.
	node->keys = (struct BTreePageKey*)&node->header[1];

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

enum btree_e
btree_init(struct BTree* tree, struct Pager* pager)
{
	enum btree_e btree_result = BTREE_OK;

	tree->root = 0;
	tree->pager = pager;

	btree_result = btree_load_root(tree);
	if( btree_result != BTREE_OK )
		return btree_result;

	return BTREE_OK;
}

enum btree_e
btree_insert(struct BTree* tree, int key, void* data, int data_size)
{
	enum btree_e result = BTREE_OK;
	int index = 0;
	char found = 0;

	struct BTreeNode* node = tree->root;

	index = binary_search_keys(node->keys, node->header->num_keys, key, &found);

	result = insert_data_into_node(node, index, key, data, data_size);

	pager_write_page(tree->pager, tree->root->page);
	// do
	// {
	// 	if( result != BTREE_OK )
	// 		break;
	// } while( node );

	return BTREE_OK;
}