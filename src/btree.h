#ifndef BTREE_H_
#define BTREE_H_

#include "btint.h"
#include "btree_defs.h"
#include "pager.h"

u32 btree_min_page_size(void);

enum btree_e btree_alloc(struct BTree**);
enum btree_e btree_dealloc(struct BTree*);
enum btree_e
btree_init(struct BTree* tree, struct Pager* pager, u32 root_page_id);
enum btree_e btree_deinit(struct BTree* tree);

enum btree_e btree_insert(struct BTree*, int key, void* data, int data_size);
enum btree_e btree_delete(struct BTree*, int key);

#endif