#ifndef BTREE_UTILS_H
#define BTREE_UTILS_H

#include "btree_defs.h"

void btu_read_cell(struct BTreeNode* node, int index, struct CellData* cell);
int btu_get_node_size(struct BTreeNode* node);
char* btu_get_node_buffer(struct BTreeNode* node);
int btu_calc_cell_size(int size);

int btu_binary_search_keys(
	struct BTreePageKey* arr, unsigned char num_keys, int key, char* found);

#endif