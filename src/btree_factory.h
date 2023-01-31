#ifndef BTREE_FACTORY_H_
#define BTREE_FACTORY_H_
#include "btint.h"
#include "btree_defs.h"
#include "noderc.h"
#include "pager.h"
#include "sql_defs.h"

struct BTreeView
{
	struct BTree* tree;
	struct BTreeNodeRC* rcer;
};

/**
 * For use when creating statically lived element
 */
struct Pager* btree_factory_pager_create(char const* filename);

/**
 * For use when creating statically lived element
 */
struct BTree* btree_factory_create_ex(
	struct Pager* pager,
	struct BTreeNodeRC* rcer,
	enum btree_type_e type,
	u32 page);

struct BTree* btree_factory_create_with_pager(
	struct Pager* pager, enum btree_type_e type, u32 page);

/**
 * For use when creating statically lived element
 */
struct BTree* btree_factory_create(char const* filename);

enum sql_e btree_factory_view_acquire(
	struct BTreeView* tv,
	struct Pager* pager,
	enum btree_type_e type,
	u32 page);
void btree_factory_view_release(struct BTreeView* tv);

#endif