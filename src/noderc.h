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

/**
 * @brief Acquire n NodeRCs, args in the order BTreeNode*, u32... Use 0 for
 * no-load.
 *
 * Ex.
 *
 * noderc_acquire_load_n(
 * 	rcer,
 * 	3,
 * 	&node_one,
 *  0, // Create a new page
 * 	&node_two,
 *  1, // Load from page.
 * )
 *
 * @param rcer
 * @param num
 * @param ...
 * @return enum btree_e
 */
enum btree_e noderc_acquire_load_n(struct BTreeNodeRC* rcer, u32 num, ...);

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

enum btree_e noderc_persist_n(struct BTreeNodeRC* rcer, u32 num, ...);

void noderc_release(struct BTreeNodeRC* rcer, struct NodeView* out_view);

void noderc_release_n(struct BTreeNodeRC* rcer, u32 num, ...);

enum btree_e noderc_reinit_read(
	struct BTreeNodeRC* rcer, struct NodeView* out_view, u32 page_id);

enum btree_e noderc_reinit_as(
	struct BTreeNodeRC* rcer, struct NodeView* out_view, u32 page_id);

struct BTreeNode* nv_node(struct NodeView* view);
struct Page* nv_page(struct NodeView* view);
struct Pager* nv_pager(struct NodeView* view);

#endif
