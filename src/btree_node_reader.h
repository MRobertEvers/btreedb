#ifndef BTREE_NODE_READER_H_
#define BTREE_NODE_READER_H_

#include "btint.h"
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
	struct BTree* tree,
	struct BTreeNode* node,
	u32 key,
	void* buffer,
	u32 buffer_size);

enum btree_e btree_node_read_ex(
	struct BTree* tree,
	struct BTreeNode* node,
	void* key,
	u32 key_size,
	void* buffer,
	u32 buffer_size);

enum btree_e btree_node_read_ex2(
	struct BTree* tree,
	void* compare_ctx,
	struct BTreeNode* node,
	void* key,
	u32 key_size,
	void* buffer,
	u32 buffer_size);

enum btree_e btree_node_read_at(
	struct BTree* tree,
	struct BTreeNode* node,
	u32 index,
	void* buffer,
	u32 buffer_size);

#endif