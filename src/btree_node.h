#ifndef BTREE_NODE_H
#define BTREE_NODE_H

#include "btree_defs.h"
#include "page.h"

enum btree_e btree_node_create_from_page(struct BTreeNode**, struct Page*);
enum btree_e btree_node_destroy(struct BTreeNode*);

enum btree_e
btree_node_init_from_page(struct BTreeNode* node, struct Page* page);
enum btree_e btree_node_insert(
	struct BTreeNode* node,
	int index,
	unsigned int key,
	void* data,
	int data_size);
#endif