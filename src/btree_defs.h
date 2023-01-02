#ifndef BTREE_DEFS_H
#define BTREE_DEFS_H

#include "page.h"

#define BTREE_HEADER_SIZE 100

// "Table b-trees" use a 64-bit signed integer key and store all data in the
// leaves. "Index b-trees" use arbitrary keys and store no data at all.
enum btree_e
{
	BTREE_ERR_CHILD_OVERWRITE,
	BTREE_OK,
	BTREE_ERR_NODE_NOT_ENOUGH_SPACE,
	BTREE_ERR_CURSOR_NO_PARENT,
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

// This is like "TID" in postgres
struct CursorBreadcrumb
{
	int key;
	int page_id;
};

struct Cursor
{
	struct BTree* tree;
	int current_page_id;

	int current_key;

	struct CursorBreadcrumb breadcrumbs[8]; // TODO: Dynamic?
	int breadcrumbs_size;
};

struct CellData
{
	void* pointer;
	int* size;
};

#endif