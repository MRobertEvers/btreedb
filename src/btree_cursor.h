#ifndef BTREE_CURSOR_H
#define BTREE_CURSOR_H

#include "btree_defs.h"

struct Cursor* cursor_create(struct BTree* tree);
void cursor_destroy(struct Cursor* cursor);
enum btree_e cursor_select_parent(struct Cursor* cursor);
enum btree_e cursor_traverse_to(struct Cursor* cursor, int key, char* found);

#endif