#ifndef IBTREE_ALG_H
#define IBTREE_ALG_H

#include "btree_alg.h"
#include "btree_defs.h"
#include "btree_node_writer.h"

/**
 * @brief Split node algorithm.
 *
 * Create a left and right child node; place half the data in the
 * left and right child each, then place the key for the left child
 * in the input node and set the rightmost child field.
 *
 * Input node becomes parent.
 * Input node, left and right children are written to disk.
 *
 * WARNING! The parent page must be on disk already. Unlike the other algo.
 *
 * @param tree
 * @param node
 * @return enum btree_e
 */
enum btree_e ibta_split_node_as_parent(
	struct BTreeNode* node,
	struct BTreeNodeRC* rcer,
	struct SplitPageAsParent* split_page);

/**
 * @brief Split node algorithm
 *
 * Split Page->left_child_high_key becomes the index of the key that was not
 * included from the parent. (The index as it was in the parent node.)
 *
 * If the input node has more than 1 key, the holding_node will contain one
 * key
 *
 * @param tree
 * @param node
 * @param split_page [Optional] If the page_id of the right child is needed,
 * provide this field.
 * @return enum btree_e
 */
enum btree_e ibta_split_node(
	struct BTreeNode* node,
	struct BTreeNodeRC* rcer,
	struct BTreeNode* holding_node,
	struct SplitPage* split_page);

struct ibta_insert_at
{
	void* data;
	u32 data_size;
	u32 flags;
	u32 key;
	enum writer_ex_mode_e mode;
};
void ibta_insert_at_init(
	struct ibta_insert_at* insert_at, void* data, u32 data_size);
void ibta_insert_at_init_ex(
	struct ibta_insert_at* insert_at,
	void* data,
	u32 data_size,
	u32 flags,
	u32 key,
	enum writer_ex_mode_e mode);

enum btree_e
ibta_insert_at(struct Cursor* cursor, struct ibta_insert_at* insert_at);

enum btree_e ibta_rotate(struct Cursor* cursor, enum bta_rebalance_mode_e mode);

enum merge_mode_e
{
	MERGE_MODE_LEFT,
	MERGE_MODE_RIGHT
};
/**
 * @brief Merges right unless it's the last sibling, then it merges left.
 *
 * Assumes the cursor is pointing to the left child; special logic if rightmost
 * child
 *
 * @param cursor
 * @return enum btree_e
 */
enum btree_e ibta_merge(struct Cursor* cursor, enum bta_rebalance_mode_e mode);

/**
 * @brief Expects the cursor to be at the node that just underflowed.
 *
 * @param cursor
 * @return enum btree_e
 */
enum btree_e ibta_rebalance(struct Cursor* cursor);

/**
 * @brief Rebalance root is called when the root is underflown;
 *
 * The root only underflows when there is only one child. I.e.
 * regardless of any other underflow condition.
 *
 * If the root has any children more than 1, then this returns BTREE_OK.
 *
 * @param cursor
 * @return enum btree_e
 */
enum btree_e ibta_rebalance_root(struct Cursor* cursor);

#endif