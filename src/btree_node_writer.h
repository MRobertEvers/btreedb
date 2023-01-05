#ifndef BTREE_NODE_WRITER_H_
#define BTREE_NODE_WRITER_H_

#include "btree_defs.h"
#include "pager.h"

/**
 * @brief Writes data to a node. Use overflow page if needed.
 *
 * @param tree
 * @param node
 * @param key
 * @param data
 * @param data_size
 * @return enum btree_e
 */
enum btree_e btree_node_write(
	struct BTreeNode* node,
	struct Pager* tree,
	unsigned int key,
	void* data,
	unsigned int data_size);

#endif