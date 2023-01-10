#ifndef BTREE_CURSOR_H
#define BTREE_CURSOR_H

#include "btint.h"
#include "btree_defs.h"

struct Cursor* cursor_create(struct BTree* tree);
void cursor_destroy(struct Cursor* cursor);
enum btree_e cursor_select_parent(struct Cursor* cursor);

/**
 * @brief Pushes the current index head to the crumbs
 *
 * @param cursor
 * @return enum btree_e
 */
enum btree_e cursor_push(struct Cursor* cursor);
enum btree_e
cursor_push_crumb(struct Cursor* cursor, struct CursorBreadcrumb* crumb);
enum btree_e cursor_pop(struct Cursor* cursor, struct CursorBreadcrumb* crumb);

enum btree_e cursor_traverse_to(struct Cursor* cursor, int key, char* found);

#endif