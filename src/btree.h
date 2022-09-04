#ifndef BTREE_H_
#define BTREE_H_

#include "pager.h"

// "Table b-trees" use a 64-bit signed integer key and store all data in the
// leaves. "Index b-trees" use arbitrary keys and store no data at all.
enum btree_e
{
	BTREE_ERR_CHILD_OVERWRITE,
	BTREE_OK,
	BTREE_ERR_NODE_NOT_ENOUGH_SPACE,
	BTREE_NEED_ROOT_INIT,
	BTREE_UNK_ERR,
	BTREE_NEED_ALLOC,
	BTREE_NEED_PAGE
};

struct BTreePageHeader
{
	char is_leaf;
	char persisted;
	unsigned int free_heap;
	unsigned int num_keys;
	unsigned int right_child;

	// Offset from cell_base_offset
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

enum btree_type
{
	bplus_tree,
	btree
};

struct BTreeHeader
{
	int page_high_water;
	enum btree_type type;
};

struct BTree
{
	struct BTreeHeader header;
	struct Pager* pager;

	int root_page_id;
};

struct Cursor
{
	struct BTree* tree;
	int current_page_id;

	int current_key;
};

/**
 * @brief Unlike CellKey; this does not contain the actual data. Only a
 * reference to the data.
 *
 */
struct CellData
{
	void* pointer;
	int* size;
};

enum btree_e
btree_node_init_from_page(struct BTreeNode* node, struct Page* page);

void read_cell(struct BTreeNode* node, int index, struct CellData* cell);

struct Cursor* create_cursor(struct BTree* tree);
void destroy_cursor(struct Cursor* cursor);

enum btree_e split_node(struct BTree* tree, struct BTreeNode* node);

enum btree_e
btree_node_create_from_page(struct BTree*, struct BTreeNode**, struct Page*);
enum btree_e btree_node_destroy(struct BTree*, struct BTreeNode*);

enum btree_e btree_alloc(struct BTree**);
enum btree_e btree_dealloc(struct BTree*);
enum btree_e btree_init(struct BTree* tree, struct Pager* pager);
enum btree_e btree_deinit(struct BTree* tree);

enum btree_e btree_insert(struct BTree*, int key, void* data, int data_size);
enum btree_e btree_traverse_to(struct Cursor* cursor, int key, char* found);
#endif