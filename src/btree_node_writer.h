#ifndef BTREE_NODE_WRITER_H_
#define BTREE_NODE_WRITER_H_

#include "btint.h"
#include "btree_defs.h"
#include "noderc.h"
#include "pager.h"

/**
 * @brief Writes data to a node. Use overflow page if needed.
 *
 * Writes to disk!
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
	struct Pager* pager,
	u32 key,
	void* data,
	u32 data_size);

enum btree_e btree_node_write_at(
	struct BTreeNode* node,
	struct Pager* pager,
	struct InsertionIndex* insertion_index,
	u32 key,
	void* data,
	u32 data_size);

enum writer_ex_mode_e
{
	WRITER_EX_MODE_RAW,
	WRITER_EX_MODE_CELL_MOVE,
};

/**
 * @brief Writes data to a node. Use overflow page if needed.
 *
 * Flags arg is ignored in RAW mode.
 *
 * Writes to disk!
 *
 * @param tree
 * @param node
 * @param key
 * @param data
 * @param data_size
 * @return enum btree_e
 */
enum btree_e btree_node_write_ex(
	struct BTreeNode* node,
	struct Pager* pager,
	struct InsertionIndex* insertion_index,
	u32 key,
	u32 flags,
	void* data,
	u32 data_size,
	enum writer_ex_mode_e mode);

enum btree_e btree_node_delete(
	struct BTree* tree, struct NodeView* node, struct ChildListIndex* index);

#endif
