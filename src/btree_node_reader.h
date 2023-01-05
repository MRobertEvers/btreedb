#ifndef BTREE_NODE_READER_H_
#define BTREE_NODE_READER_H_

#include "btree_defs.h"

/**
 * @brief Reads data from a node. Including from overflow nodes.
 *
 * @param tree
 * @param node
 * @param key
 * @param data
 * @param data_size
 * @return enum btree_e
 */
enum btree_e btree_node_read(
	struct BTreeNode* node,
	struct Pager* tree,
	unsigned int key,
	void* buffer,
	unsigned int buffer_size);

#endif