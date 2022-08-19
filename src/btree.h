#ifndef BTREE_H_
#define BTREE_H_

#include "pager.h"

// "Table b-trees" use a 64-bit signed integer key and store all data in the
// leaves. "Index b-trees" use arbitrary keys and store no data at all.
enum btree_e
{
	BTREE_ERR_CHILD_OVERWRITE,
	BTREE_OK,
	BTREE_UNK_ERR,
	BTREE_NEED_ALLOC,
	BTREE_NEED_PAGE
};

struct BTreePageHeader
{
	unsigned int num_keys;
	char is_leaf;
	unsigned int cell_high_water_offset;
};

struct BTreePageKey
{
	unsigned int key;
	unsigned int cell_offset; // TODO: union
};

struct BTreeNode
{
	int page_number;

	struct BTreePageHeader* header;
	// Start of the keys area; this area is contiguous and sorted
	struct BTreePageKey* keys;

	struct Page* page; // Page backing this node.
};

struct BTree
{
	struct Pager* pager;
	struct BTreeNode* root;
};

enum btree_e btree_node_alloc(struct BTree*, struct BTreeNode**);
enum btree_e
btree_node_init_from_page(struct BTree*, struct Page*, struct BTreeNode*);

enum btree_e btree_alloc(struct BTree**);
enum btree_e btree_dealloc(struct BTree*);
enum btree_e btree_init(struct BTree* tree, struct Pager* pager);
enum btree_e btree_deinit(struct BTree* tree);

enum btree_e btree_insert(struct BTree*, int key, void* data, int data_size);

#endif