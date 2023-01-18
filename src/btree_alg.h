#ifndef BTREE_ALG_H
#define BTREE_ALG_H

#include "btree_defs.h"

struct SplitPageAsParent
{
	int left_child_page_id;
	int left_child_high_key;
	int right_child_page_id;
};

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
enum btree_e bta_split_node_as_parent(
	struct BTreeNode* node,
	struct BTreeNodeRC* rcer,
	struct SplitPageAsParent* split_page);

struct SplitPage
{
	int left_page_id;
	int left_page_high_key;
};

/**
 * @brief Split node algorithm
 *
 * Create a right child node. Move the upper half the data to the right child,
 * then keep the lower half in the left child.
 * Input node becomes right child.
 * Input node and left child are both written to disk.
 *
 * @param tree
 * @param node
 * @param split_page [Optional] If the page_id of the right child is needed,
 * provide this field.
 * @return enum btree_e
 */
enum btree_e bta_split_node(
	struct BTreeNode* node, struct Pager* pager, struct SplitPage* split_page);

enum bta_rebalance_mode_e
{
	BTA_REBALANCE_MODE_UNK,
	BTA_REBALANCE_MODE_ROTATE_RIGHT,
	BTA_REBALANCE_MODE_ROTATE_LEFT,
	BTA_REBALANCE_MODE_MERGE_RIGHT,
	BTA_REBALANCE_MODE_MERGE_LEFT,
};
enum btree_e bta_rotate(struct Cursor* cursor, enum bta_rebalance_mode_e mode);
enum btree_e bta_merge(struct Cursor* cursor, enum bta_rebalance_mode_e mode);

enum btree_e bta_rebalance(struct Cursor* cursor);

enum btree_e bta_rebalance_root(struct Cursor* cursor);

#endif