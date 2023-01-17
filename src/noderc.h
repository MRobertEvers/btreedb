#ifndef NODERC_H_
#define NODERC_H_

#include "btree_defs.h"
#include "pager.h"

/**
 * @brief Node reference manager.
 *
 */

void noderc_init(struct BTreeNodeRC* rcer, struct Pager* pager);

enum btree_e
noderc_acquire(struct BTreeNodeRC* rcer, struct NodeView* out_view);

enum btree_e
noderc_acquire_n(struct BTreeNodeRC* rcer, struct NodeView* out_view, u32 num);

/**
 * @brief Initialize a nodeview with a page loaded.
 *
 * @param rcer
 * @param out_view
 * @param page_id
 * @return enum btree_e
 */
enum btree_e noderc_acquire_load(
	struct BTreeNodeRC* rcer, struct NodeView* out_view, u32 page_id);

void noderc_release(struct BTreeNodeRC* rcer, struct NodeView* out_view);

struct BTreeNode* nodeview_node(struct NodeView* view);
struct Page* nodeview_page(struct NodeView* view);
struct Pager* nodeview_pager(struct NodeView* view);

#endif