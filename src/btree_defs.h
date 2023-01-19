#ifndef BTREE_DEFS_H
#define BTREE_DEFS_H

#include "btint.h"
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
	BTREE_ERR_CURSOR_NO_SIBLING,
	BTREE_ERR_CURSOR_DEPTH_EXCEEDED,
	BTREE_ERR_CORRUPT_CELL,
	BTREE_ERR_CANNOT_MERGE,
	BTREE_ERR_PARENT_DEFICIENT,
	BTREE_ERR_UNDERFLOW,
	BTREE_ERR_PAGING,
	BTREE_ERR_NO_MEM,
	BTREE_NEED_ROOT_INIT,
	BTREE_ERR_UNK,
	BTREE_NEED_ALLOC,
	BTREE_NEED_PAGE
};

struct BTreePageHeader
{
	char is_leaf;
	char is_root;
	u32 free_heap;
	u32 num_keys;
	u32 right_child;

	// Offset from cell_base_offset
	u32 cell_high_water_offset;
};

struct BTreePageKey
{
	u32 key; // Or left child.
	u32 cell_offset;
	u32 flags;
};

struct BTreeNode
{
	int page_number;
	// Required to know because the root page may be different.
	u32 root_page_id;

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
	u32 underflow;
	enum btree_type type;
};

struct BTreeNodeRC
{
	struct Pager* pager;
};

struct NodeView
{
	struct BTreeNode node;
	struct Page* page;

	struct Pager* pager;
};

/**
 * @brief Return -1 if left is less than right, 1 if right is; 0 if equal.
 *
 * If compare needs to be called more than once. The right hand side should be
 * used as the buffer that gets re-compared.
 */
typedef int (*btree_compare_fn)(
	void* compare_context,
	void* left,
	u32 left_size,
	void* right,
	u32 right_size,
	u32 right_offset,
	u32* out_bytes_compared);

typedef void (*btree_compare_reset_fn)(void* compare_context);

enum btree_type_e
{
	BTREE_TBL,
	BTREE_INDEX
};

struct BTree
{
	struct BTreeHeader header;
	struct Pager* pager;
	struct BTreeNodeRC* rcer;

	u32 root_page_id;

	enum btree_type_e type;
	btree_compare_fn compare;
	btree_compare_reset_fn reset_compare;
};

struct BTreeCompareContext
{
	btree_compare_fn compare;
	btree_compare_reset_fn reset;

	void* compare_context;
	struct Pager* pager;
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
	u32 index;
};

/**
 * @brief Used as an insertion point by the insertion functions.
 *
 */
struct InsertionIndex
{
	enum key_list_index_mode_e mode;

	// If mode=KLIM_INDEX, then this should be populated
	u32 index;
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

	void* compare_context;
};

/**
 * @deprecated
 *
 */
struct CellData
{
	void* pointer;
	// High bit indicates the pointer is a LinkListWrapperData
	int* size;
};

#endif
