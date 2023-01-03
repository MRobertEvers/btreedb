#ifndef BTREE_UTILS_H
#define BTREE_UTILS_H

#include "btree_defs.h"

void btu_read_cell(struct BTreeNode* node, int index, struct CellData* cell);
int btu_get_node_size(struct BTreeNode* node);
char* btu_get_node_buffer(struct BTreeNode* node);

/**
 * @brief Calculates the left edge of a pointer at highwater in the heap.
 *
 * @param node
 * @param highwater
 * @return char*
 */
char* btu_calc_highwater_offset(struct BTreeNode* node, int highwater);

/**
 * @brief Calculates the amount of space needed to store data of size in the
 * heap.
 *
 * Accounts for bookkeeping data as well as the data itself.
 *
 * @param size
 * @return int
 */
int btu_calc_cell_size(int size);

int btu_binary_search_keys(
	struct BTreePageKey* arr, unsigned char num_keys, int key, char* found);

int btu_init_keylistindex_from_index(
	struct KeyListIndex* keylistindex, struct BTreeNode const* node, int index);

#endif