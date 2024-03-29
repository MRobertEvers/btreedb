#ifndef BTREE_CURSOR_H
#define BTREE_CURSOR_H

#include "btint.h"
#include "btree_defs.h"
#include "noderc.h"

struct Cursor* cursor_create(struct BTree* tree);
struct Cursor* cursor_create_ex(struct BTree* tree, void* compare_context);
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
enum btree_e
cursor_pop_n(struct Cursor* cursor, struct CursorBreadcrumb* crumb, u32 num);
enum btree_e cursor_peek(struct Cursor* cursor, struct CursorBreadcrumb* crumb);

/**
 * @brief Move the cursor to the smallest child.
 *
 * @param cursor
 * @param crumb
 * @return enum btree_e
 */
enum btree_e cursor_iter_begin(struct Cursor* cursor);

/**
 * @brief Move the cursor to point to the next cell in the node or move the
 * cursor to the next node.
 *
 * @param cursor
 * @param crumb
 * @return enum btree_e
 */
enum btree_e cursor_iter_next(struct Cursor* cursor);

enum btree_e
cursor_read_current(struct Cursor* cursor, struct NodeView* out_nv);

enum btree_e cursor_restore(
	struct Cursor* cursor, struct CursorBreadcrumb* crumb, u32 num_restore);

enum btree_e cursor_traverse_to(struct Cursor* cursor, u32 key, char* found);
enum btree_e cursor_traverse_to_ex(
	struct Cursor* cursor, void* key, u32 key_size, char* found);

/**
 * @brief Starting from cursor, find the largest element.
 *
 * @param cursor
 * @return enum btree_e
 */
enum btree_e cursor_traverse_largest(struct Cursor* cursor);

/**
 * @brief Starting from cursor, find the smallest element.
 *
 * @param cursor
 * @return enum btree_e
 */
enum btree_e cursor_traverse_smallest(struct Cursor* cursor);

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
 * @brief Reads the parent node into out_view.
 *
 * @param cursor
 * @param out_node
 * @return enum btree_e
 */
enum btree_e
cursor_read_parent(struct Cursor* cursor, struct NodeView* out_view);

/**
 * @brief Reads the current node into out_view.
 *
 * @param cursor
 * @param out_node
 * @return enum btree_e
 */
enum btree_e
cursor_read_current(struct Cursor* cursor, struct NodeView* out_nv);

/**
 * @brief Returns the index of the parent cell in the parent node.
 */
enum btree_e
cursor_parent_index(struct Cursor* cursor, struct ChildListIndex* out_index);
enum btree_e
cursor_parent_crumb(struct Cursor* cursor, struct CursorBreadcrumb* out_crumb);

struct BTreeNodeRC* cursor_rcer(struct Cursor* cursor);
struct BTree* cursor_tree(struct Cursor* cursor);
enum btree_type_e cursor_tree_type(struct Cursor* cursor);
struct Pager* cursor_pager(struct Cursor* cursor);
struct ChildListIndex* cursor_curr_ind(struct Cursor* cursor);

#endif