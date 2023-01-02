#ifndef BTREE_ALG_H
#define BTREE_ALG_H

#include "btree_defs.h"

/**
 * @brief B+ Tree split node algorithm
 *
 * Create a left and right child node; keep a copy of the key in
 * the parent, insert the key and value in child
 *
 * @param tree
 * @param node
 * @return enum btree_e
 */
enum btree_e bta_bplus_split_node(struct BTree* tree, struct BTreeNode* node);

struct SplitPage
{
	int right_page_id;
	int right_page_low_key;
};

/**
 * @brief B Tree split node algorithm
 *
 * Create a right child node. Move the upper half the data to the right child,
 * then keep the lower half in the left child.
 *
 * @param tree
 * @param node
 * @return enum btree_e
 */
enum btree_e bta_split_node(
	struct BTree* tree, struct BTreeNode* node, struct SplitPage* split_page);

#endif