#ifndef BTREE_ALG_H
#define BTREE_ALG_H

#include "btree_defs.h"

struct SplitPageAsParent
{
	int left_child_page_id;
	int right_child_page_id;
	int right_child_low_key;
};

/**
 * @brief Split node algorithm.
 *
 * Create a left and right child node; place half the data in the
 * left and right child each, then place the key for the left child
 * in the input node and set the rightmost child field.
 *
 * @param tree
 * @param node
 * @return enum btree_e
 */
enum btree_e bta_split_node_as_parent(
	struct BTree* tree,
	struct BTreeNode* node,
	struct SplitPageAsParent* split_page);

struct SplitPage
{
	int right_page_id;
	int right_page_low_key;
};

/**
 * @brief Split node algorithm
 *
 * Create a right child node. Move the upper half the data to the right child,
 * then keep the lower half in the left child.
 *
 * @param tree
 * @param node
 * @param split_page [Optional] If the page_id of the right child is needed,
 * provide this field.
 * @return enum btree_e
 */
enum btree_e bta_split_node(
	struct BTree* tree, struct BTreeNode* node, struct SplitPage* split_page);

#endif