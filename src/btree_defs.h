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
	BTREE_ERR_INVALID_PAGE_SIZE_TOO_SMALL,
	BTREE_ERR_NODE_NOT_ENOUGH_SPACE,
	BTREE_ERR_KEY_NOT_FOUND,
	BTREE_ERR_CURSOR_NO_PARENT,
	BTREE_ERR_CURSOR_DEPTH_EXCEEDED,
	BTREE_ERR_CORRUPT_CELL,
	BTREE_NEED_ROOT_INIT,
	BTREE_ERR_UNK,
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
	unsigned int flags;		  // 1 means overflow page.
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

enum key_list_index_mode_e
{
	KLIM_END = 0,
	KLIM_RIGHT_CHILD,
	KLIM_INDEX,
};

struct ChildListIndex
{
	enum key_list_index_mode_e mode;

	// If mode=KLIM_INDEX, then this should be populated
	unsigned int index;
};

/**
 * @brief Used as an insertion point by the insertion functions.
 *
 */
struct LeftInsertionIndex
{
	enum key_list_index_mode_e mode;

	// If mode=KLIM_INDEX, then this should be populated
	unsigned int index;
};

// This is like "TID" in postgres
struct CursorBreadcrumb
{
	struct ChildListIndex key_index;
	int page_id;
};

struct Cursor
{
	struct BTree* tree;
	int current_page_id;

	// Index of the key in the current page.
	struct ChildListIndex current_key_index;

	struct CursorBreadcrumb breadcrumbs[8]; // TODO: Dynamic?
	int breadcrumbs_size;
};

struct CellData
{
	void* pointer;
	// High bit indicates the pointer is a LinkListWrapperData
	int* size;
};

#endif