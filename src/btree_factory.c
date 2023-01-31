#include "btree_factory.h"

#include "btree.h"
#include "btree_utils.h"
#include "ibtree.h"
#include "ibtree_layout_schema.h"
#include "ibtree_layout_schema_cmp.h"
#include "ibtree_layout_schema_ctx.h"
#include "noderc.h"
#include "pager.h"
#include "pager_ops_cstd.h"

#include <assert.h>
#include <stdlib.h>

struct Pager*
btree_factory_pager_create(char const* filename)
{
	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	enum pager_e pres = PAGER_OK;

	pres = page_cache_create(&cache, 10);
	pres = pager_cstd_create(&pager, cache, filename, 0x1000);

	assert(btpage_err(pres) == BTREE_OK);

	return pager;
}

struct BTree*
btree_factory_create_ex(struct Pager* pager, enum btree_type_e type, u32 page)
{
	struct BTree* tree = NULL;
	struct BTreeNodeRC* rcer = NULL;

	enum btree_e bres = BTREE_OK;

	rcer = (struct BTreeNodeRC*)malloc(sizeof(struct BTreeNodeRC));

	noderc_init(rcer, pager);

	bres = btree_alloc(&tree);
	switch( type )
	{
	case BTREE_TBL:
		bres = btree_init(tree, pager, rcer, page);
		break;
	case BTREE_INDEX:

		bres = ibtree_init(
			tree, pager, rcer, page, &ibtls_compare, &ibtls_reset_compare);
		break;
	}

	if (bres != BTREE_OK)
	{
		free(rcer);
		btree_dealloc(tree);
		tree = NULL;
	}

	return tree;
}

struct BTree*
btree_factory_create(char const* filename)
{
	return btree_factory_create_ex(
		btree_factory_pager_create(filename), BTREE_INDEX, 1);
}

void
btree_factory_destroy(struct BTree* tree)
{
	page_cache_destroy(tree->pager->cache);
	pager_destroy(tree->pager);
	free(tree->rcer);

	btree_dealloc(tree);
}