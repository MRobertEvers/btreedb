#ifndef BTREE_H_
#define BTREE_H_

#include "btree_defs.h"
#include "pager.h"

enum btree_e
btree_node_init_from_page(struct BTreeNode* node, struct Page* page);

struct Cursor* create_cursor(struct BTree* tree);
void destroy_cursor(struct Cursor* cursor);
enum btree_e cursor_select_parent(struct Cursor* cursor);

enum btree_e btree_alloc(struct BTree**);
enum btree_e btree_dealloc(struct BTree*);
enum btree_e btree_init(struct BTree* tree, struct Pager* pager);
enum btree_e btree_deinit(struct BTree* tree);

enum btree_e btree_insert2(struct BTree*, int key, void* data, int data_size);
enum btree_e btree_insert(struct BTree*, int key, void* data, int data_size);
enum btree_e btree_traverse_to(struct Cursor* cursor, int key, char* found);
#endif