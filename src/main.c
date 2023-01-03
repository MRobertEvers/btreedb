#include "btree.h"
#include "pager_ops_cstd.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
main()
{
	char billy[] = "billy";
	char charlie[] = "charlie";
	struct BTree* tree = NULL;

	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;
	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, "test.db", 0x1000);

	btree_alloc(&tree);
	btree_init(tree, pager);

	btree_insert(tree, 12, billy, sizeof(billy));
	btree_insert(tree, 1, charlie, sizeof(charlie));

	return 0;
}