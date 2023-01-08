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
	struct Pager* pager,
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

struct MergedPage
{};

/**
 * @brief Nodes must be in order such that all keys of right a greater than all
 * keys of left.
 *
 * TODO: Need to handle when merging into page 1.
 *
 * @param left
 * @param right
 * @param pager
 * @return enum btree_e
 */
enum btree_e bta_merge_nodes(
	struct BTreeNode* stable_node,
	struct BTreeNode* other_node,
	struct Pager* pager,
	struct MergedPage* merged_page);

#endif