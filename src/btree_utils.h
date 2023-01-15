#ifndef BTREE_UTILS_H
#define BTREE_UTILS_H

#include "btint.h"
#include "btree_defs.h"
#include "pager_e.h"

/**
 * @brief Convert page error to btree error.
 *
 * @param page_err
 * @return enum btree_e
 */
enum btree_e btpage_err(enum pager_e page_err);

/**
 * @deprecated
 * @brief
 * @param node
 * @param index
 * @param cell
 */
void btu_read_cell(struct BTreeNode* node, int index, struct CellData* cell);

int btu_get_node_heap_size(struct BTreeNode* node);
int btu_get_node_size(struct BTreeNode* node);
byte* btu_get_node_buffer(struct BTreeNode* node);

/**
 * @brief Calculates the left edge of a pointer at highwater in the heap.
 *
 * @param node
 * @param highwater
 * @return char*
 */
byte* btu_calc_highwater_offset(struct BTreeNode* node, int highwater);

/**
 * @brief Return a pointer to the cell left edge.
 *
 * @param node
 * @param index
 * @return char*
 */
byte* btu_get_cell_buffer(struct BTreeNode* node, int index);

/**
 * @brief Peeks at the cell size and returns it.
 *
 * @param node
 * @param index
 * @return char*
 */
u32 btu_get_cell_buffer_size(struct BTreeNode* node, int index);

u32 btu_get_cell_flags(struct BTreeNode* node, int index);

/**
 * @deprecated
 * @brief Calculates the amount of space needed to store data of size in the
 * heap.
 *
 * Accounts for bookkeeping data as well as the data itself.
 *
 * @param size
 * @return int
 */
int btu_calc_cell_size(int size);

/**
 * @brief Returns the index of the value in the key array
 *
 * If the value is not in the array, it will return the index of the position
 * in the array where it would go in sorted order.
 *
 * The returned index is the index in the array at which the value of key is
 * less than or equal to the values in the array at indexes greater than or
 * equal to the returned index.
 *
 * Ex.
 *
 * [1, 2, 5]
 * search(3) => index 2 i.e. The index of 5.
 *
 * @param arr
 * @param num_keys
 * @param key
 * @param found 1 if the value was found. 0 otherwise.
 * @return int
 */
int btu_binary_search_keys(
	struct BTreePageKey* arr, unsigned char num_keys, int key, char* found);

/**
 * @brief Converts an index key to the approprate ListIndex for breadcrumbs.
 *
 * @param keylistindex
 * @param node
 * @param index
 * @return int
 */
void btu_init_keylistindex_from_index(
	struct ChildListIndex* keylistindex,
	struct BTreeNode const* node,
	int index);

/**
 * @brief Converts an index key to the approprate InsertionIndex for insertion
 * into a node.
 *
 * @param keylistindex
 * @param node
 * @param index
 * @return int
 */
void btu_init_insertion_index_from_index(
	struct InsertionIndex* keylistindex,
	struct BTreeNode const* node,
	int index);

#endif