#ifndef BTREE_NODE_H
#define BTREE_NODE_H

#include "btree_defs.h"
#include "page.h"

/**
 * @brief Used to initialize a node from a page.
 *
 * @param node
 * @param page An already loaded page.
 * @return enum btree_e
 */
enum btree_e btree_node_create_from_page(struct BTreeNode**, struct Page*);

/**
 * @brief Used to initialize a node from an empty (unloaded) page.
 *
 * The page is not expected to be read from disk.
 *
 * @param node
 * @param page An already loaded page.
 * @return enum btree_e
 */
enum btree_e btree_node_create_as_page_number(
	struct BTreeNode**, int page_number, struct Page*);

enum btree_e btree_node_destroy(struct BTreeNode*);

enum btree_e
btree_node_init_from_page(struct BTreeNode* node, struct Page* page);

/**
 * @brief Inserts data into a node.
 *
 * Keys grow from left side (after meta) to the right.
 * Data heap grows from right edge to left.
 * |------|----------|---------------|
 * | Meta | Keys ~~> | <~~ Data heap |
 * |------|----------|---------------|
 *                          ^-- High Water Mark
 *
 * High Water Mark is distance from right edge to left heap edge.
 *
 * Note: The key argument is discarded in the case of KLIM_RIGHT_CHILD.
 *
 * @param node
 * @param index
 * @param key
 * @param data
 * @param data_size
 * @return enum btree_e
 */
enum btree_e btree_node_insert(
	struct BTreeNode* node,
	struct KeyListIndex* index,
	unsigned int key,
	void* data,
	int data_size);

/**
 * @brief Deletes data from a node.
 *
 * Removes key at index and moves data in the heap to fill the gap.
 *
 * @param node
 * @param index
 * @param key
 * @param data
 * @param data_size
 * @return enum btree_e
 */
enum btree_e
btree_node_delete(struct BTreeNode* node, struct KeyListIndex* index);

/**
 * @brief Returns the number of children for non-leaf nodes and the number of
 * cells of leaf nodes.
 *
 */
int btree_node_arity(struct BTreeNode* node);

#endif