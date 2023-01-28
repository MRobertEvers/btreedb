#include "btree_factory.h"

#include "btree.h"
#include "ibtree.h"
#include "ibtree_layout_schema.h"
#include "ibtree_layout_schema_cmp.h"
#include "ibtree_layout_schema_ctx.h"
#include "noderc.h"
#include "pager.h"
#include "pager_ops_cstd.h"

#include <stdlib.h>

struct BTree*
btree_factory_create(char const* filename)
{
	struct BTree* tree = NULL;
	struct BTreeNodeRC* rcer = NULL;
	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	enum pager_e pres = PAGER_OK;
	enum btree_e bres = BTREE_OK;

	rcer = (struct BTreeNodeRC*)malloc(sizeof(struct BTreeNodeRC));

	pres = page_cache_create(&cache, 10);
	pres = pager_cstd_create(&pager, cache, filename, 0x1000);

	noderc_init(rcer, pager);

	bres = btree_alloc(&tree);
	bres =
		ibtree_init(tree, pager, rcer, 1, &ibtls_compare, &ibtls_reset_compare);

	return tree;
}

void
btree_factory_destroy(struct BTree* tree)
{
	page_cache_destroy(tree->pager->cache);
	pager_destroy(tree->pager);
	free(tree->rcer);

	btree_dealloc(tree);
}