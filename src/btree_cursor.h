#ifndef BTREE_CURSOR_H
#define BTREE_CURSOR_H

#include "btint.h"
#include "btree_defs.h"

struct Cursor* cursor_create(struct BTree* tree);
void cursor_destroy(struct Cursor* cursor);

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
enum btree_e cursor_peek(struct Cursor* cursor, struct CursorBreadcrumb* crumb);

enum btree_e cursor_traverse_to(struct Cursor* cursor, int key, char* found);
enum btree_e cursor_traverse_to_ex(
	struct Cursor* cursor, void* key, u32 key_size, char* found);
enum btree_e cursor_traverse_largest(struct Cursor* cursor);

enum cursor_sibling_e
{
	CURSOR_SIBLING_LEFT,
	CURSOR_SIBLING_RIGHT,
};

/**
 * @brief Moves the cursor to the input sibling.
 *
 * To restore
 *
 * @param cursor
 * @param sibling
 * @return enum btree_e
 */
enum btree_e
cursor_sibling(struct Cursor* cursor, enum cursor_sibling_e sibling);

/**
 * @brief Expects node to already be initialized with a page!!!
 *
 * @param cursor
 * @param out_node
 * @return enum btree_e
 */
enum btree_e
cursor_read_parent(struct Cursor* cursor, struct BTreeNode* out_node);

/**
 * @brief Returns the index of the parent cell in the parent node.
 *
 */
enum btree_e
cursor_parent_index(struct Cursor* cursor, struct ChildListIndex* out_index);

#endif