#ifndef BTREE_NODE_H
#define BTREE_NODE_H

#include "btint.h"
#include "btree_cell.h"
#include "btree_defs.h"
#include "page.h"

u32 btree_node_max_cell_size(struct BTreeNode* node);

enum btree_page_key_flags_e
{
	PKEY_FLAG_UNKNOWN = 0,
	PKEY_FLAG_CELL_TYPE_OVERFLOW,
	PKEY_FLAG_CELL_TYPE_INLINE,
};

u32 btree_pkey_set_cell_type(u32 flags, enum btree_page_key_flags_e);

/**
 * @brief Returns 1 if flag is present; 0 otherwise
 *
 * @param flags
 * @return u32
 */
u32 btree_pkey_is_cell_type(u32 flags, enum btree_page_key_flags_e);

/**
 * @brief Used to initialize a node from a page.
 *
 * @param node
 * @param page An already loaded page.
 * @return enum btree_e
 */
enum btree_e btree_node_create_from_page(struct BTreeNode**, struct Page*);

/**
 * @brief Used to initialize a node from an empty (unloaded) page.
 *
 * The page is not expected to be read from disk.
 *
 * @param node
 * @param page An already loaded page.
 * @return enum btree_e
 */
enum btree_e btree_node_create_as_page_number(
	struct BTreeNode**, int page_number, struct Page*);

enum btree_e btree_node_destroy(struct BTreeNode*);

enum btree_e
btree_node_init_from_page(struct BTreeNode* node, struct Page* page);

/**
 * @brief Inserts inline cell into a node.
 *
 * Keys grow from left side (after meta) to the right.
 * Data heap grows from right edge to left.
 * |------|----------|---------------|
 * | Meta | Keys ~~> | <~~ Data heap |
 * |------|----------|---------------|
 *                          ^-- High Water Mark
 *
 * High Water Mark is distance from right edge to left heap edge.
 *
 * @param node
 * @param index
 * @param key
 * @param data
 * @param data_size
 * @return enum btree_e
 */
enum btree_e btree_node_insert_inline(
	struct BTreeNode* node,
	struct InsertionIndex* index,
	u32 key,
	struct BTreeCellInline* cell);

/**
 * @brief Writes inline cell with flags.
 *  Since other cell types are compatible with the 'inline' cell type,
 * we can copy any type of cell without inspecting it, however, we
 * do need to copy flags over. This inline writer allows that.
 * @param node
 * @param index
 * @param key
 * @param cell
 * @return enum btree_e
 */
enum btree_e btree_node_insert_inline_ex(
	struct BTreeNode* node,
	struct InsertionIndex* index,
	u32 key,
	struct BTreeCellInline* cell,
	u32 flags);

/**
 * @brief Inserts overflow cell into a node. Inline payload capped at
 * max_heap_usage
 *
 * @param node
 * @param index
 * @param cell
 * @return enum btree_e
 */
enum btree_e btree_node_insert_overflow(
	struct BTreeNode* node,
	struct InsertionIndex* index,
	u32 key,
	struct BTreeCellOverflow* cell);

enum btree_e btree_node_move(
	struct BTreeNode* source_node,
	struct BTreeNode* other,
	u32 index,
	struct Pager* pager);

enum btree_e btree_node_move_ex(
	struct BTreeNode* source_node,
	struct BTreeNode* other,
	u32 index,
	u32 new_key,
	struct Pager* pager);

enum btree_e btree_node_move_from_data(
	struct BTreeNode* dest_node,
	struct InsertionIndex* insert_index,
	u32 new_key,
	u32 flags,
	byte* cell_buffer,
	u32 cell_buffer_size,
	struct Pager* pager);

/**
 * @brief Removes data from a node.
 *
 * Removes key at index and moves data in the heap to fill the gap.
 *
 * @param node
 * @param index
 * @param key
 * @param data
 * @param data_size
 * @return enum btree_e
 */
enum btree_e btree_node_remove(
	struct BTreeNode* node,
	struct ChildListIndex* index,
	struct BTreeCellInline* removed_cell,
	void* buffer,
	u32 buffer_size);

/**
 * @brief Get the heap required for insertion; Accounts for page key size
 *
 * @param cell_disk_size Size of cell in the heap (i.e. including size value)
 * @return int
 */
u32 btree_node_heap_required_for_insertion(u32 cell_disk_size);

u32 btree_node_calc_heap_capacity(struct BTreeNode* node);

enum btree_e btree_node_compare_cell(
	struct BTree* tree,
	struct BTreeNode* node,
	u32 index,
	void* key,
	u32 key_size,
	int* out_result);

/**
 * @brief Returns index of key; BTREE_ERR_KEY_NOT_FOUND if not found.
 *
 * @param node
 * @param key
 * @param key_size
 * @param found
 * @return enum btree_e
 */
enum btree_e btree_node_search_keys(
	struct BTree* tree,
	struct BTreeNode* node,
	void* key,
	u32 key_size,
	u32* out_index);

#endif