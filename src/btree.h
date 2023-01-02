#ifndef BTREE_H_
#define BTREE_H_

#include "btree_defs.h"
#include "pager.h"

enum btree_e btree_alloc(struct BTree**);
enum btree_e btree_dealloc(struct BTree*);
enum btree_e btree_init(struct BTree* tree, struct Pager* pager);
enum btree_e btree_deinit(struct BTree* tree);

enum btree_e btree_insert(struct BTree*, int key, void* data, int data_size);

#endif