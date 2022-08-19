#include "btree.h"

#include "binary_search.h"

#include <stdlib.h>
#include <string.h>

#define BTREE_HEADER_SIZE 100

static int
get_node_cell_offset(struct BTreeNode* node)
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
	char* cell = get_node_buffer(node) + get_node_cell_offset(node) -
				 node->keys[index].cell_offset;
	memcpy(cell, &data_size, sizeof(data_size));

	cell = cell + sizeof(data_size);
	memcpy(cell, data, data_size);
	node->header->cell_high_water_offset += data_size + sizeof(int);

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

struct CellData
{
	void* pointer;
	int* size;
};
void
read_cell(struct BTreeNode* node, int index, struct CellData* cell)
{
	memset(cell, 0x00, sizeof(struct CellData));
	int offset = node->keys[index].cell_offset;

	char* cell_buffer =
		get_node_buffer(node) + get_node_cell_offset(node) - offset;

	// memcpy(&cell->size, cell_buffer, sizeof (cell->size));
	cell->size = (int*)cell_buffer;
	cell->pointer = &cell->size[1];
}

enum btree_e
split_node(struct BTree* tree, struct BTreeNode* node)
{
	// TODO: Optimize this
	// TODO: Free list
	struct BTreeNode* left = NULL;
	struct BTreeNode* right = NULL;
	struct BTreeNode* parent = NULL;

	struct Page* temp_page = NULL;
	struct Page* parent_page = NULL;
	struct Page* right_page = NULL;

	pager_page_alloc(tree->pager, &temp_page);
	pager_page_init(tree->pager, temp_page, tree->header->page_high_water);
	btree_node_alloc(tree, &left);
	btree_node_init_from_page(tree, temp_page, left);

	pager_page_alloc(tree->pager, &parent_page);
	pager_page_init(tree->pager, parent_page, node->page_number);
	// pager_read_page(tree->pager, parent_page);
	btree_node_alloc(tree, &parent);
	btree_node_init_from_page(tree, parent_page, parent);

	pager_page_alloc(tree->pager, &right_page);
	pager_page_init(tree->pager, right_page, tree->header->page_high_water + 1);
	// pager_read_page(tree->pager, right_page);
	btree_node_alloc(tree, &right);
	btree_node_init_from_page(tree, right_page, right);

	for( int i = 0; i < node->header->num_keys; i++ )
	{
		struct CellData cell = {0};
		int key = node->keys[i].key;
		read_cell(node, i, &cell);
		if( i < node->header->num_keys / 2 )
		{
			insert_data_into_node(left, i / 2, key, cell.pointer, *cell.size);
		}
		else
		{
			insert_data_into_node(right, i / 2, key, cell.pointer, *cell.size);
		}
	}

	insert_data_into_node(
		parent,
		0,
		left->keys[left->header->num_keys - 1].key,
		&node->page_number,
		sizeof(unsigned int));

	parent->header->is_leaf = 0;
	right->header->is_leaf = 1;
	left->header->is_leaf = 1;

	parent->header->right_child = right->page_number;

	memcpy(
		get_node_buffer(node),
		get_node_buffer(parent),
		// TODO: Offset must be the same...
		get_node_cell_offset(parent));

	pager_write_page(tree->pager, node->page);
	pager_write_page(tree->pager, left->page);
	pager_write_page(tree->pager, right->page);

end:
	btree_node_deinit(left);
	btree_node_dealloc(left);
	pager_page_deinit(temp_page);
	pager_page_dealloc(temp_page);

	btree_node_deinit(right);
	btree_node_dealloc(right);
	pager_page_deinit(right_page);
	pager_page_dealloc(right_page);

	btree_node_deinit(parent);
	btree_node_dealloc(parent);
	pager_page_deinit(parent_page);
	pager_page_dealloc(parent_page);
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
btree_node_dealloc(struct BTreeNode* node)
{
	free(node);
	return BTREE_OK;
}

enum btree_e
btree_node_deinit(struct BTreeNode* node)
{
	return BTREE_OK;
}

enum btree_e
btree_node_init_from_page(
	struct BTree* tree, struct Page* page, struct BTreeNode* node)
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
btree_init_from_root_node(struct BTree* tree, struct BTreeNode* root)
{
	tree->header = (struct BTreeHeader*)root->page->page_buffer;
	// TODO: We are using the high water as a trick to see if this page
	// was initialized already...
	if( !tree->header->page_high_water )
	{
		root->header->is_leaf = 1;

		tree->header->page_high_water = 2;
	}

	return BTREE_OK;
}

enum btree_e
btree_init(struct BTree* tree, struct Pager* pager)
{
	enum btree_e btree_result = BTREE_OK;

	tree->root = 0;
	tree->pager = pager;

	// TODO: This uses functions that rely on the btree_init being already
	// completed.
	btree_result = btree_load_root(tree);
	if( btree_result != BTREE_OK )
		return btree_result;

	btree_result = btree_init_from_root_node(tree, tree->root);
	if( btree_result != BTREE_OK )
		return btree_result;

	return BTREE_OK;
}

enum btree_e
btree_deinit(struct BTree* tree)
{
	// TODO: Nodes?
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