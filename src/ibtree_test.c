#include "ibtree_test.h"

#include "btree.h"
#include "btree_defs.h"
#include "ibtree.h"
#include "pager.h"
#include "pager_ops_cstd.h"

#include <stdio.h>
#include <stdlib.h>

void
ibtree_test_insert_shallow(void)
{
	char const* db_name = "ibtree_insert_shallow.db";
	int result = 1;
	struct BTreeNode* node = 0;
	struct PageSelector selector;
	struct Pager* pager;
	struct PageCache* cache = NULL;
	struct InsertionIndex index = {0};

	remove(db_name);
	page_cache_create(&cache, 3);
	// 1 byte of payload can fit on the first page.
	u32 page_size = btree_min_page_size() + 1 * 4;
	pager_cstd_create(&pager, cache, db_name, page_size);

	struct BTree* tree;
	btree_alloc(&tree);
	enum btree_e btresult = btree_init(tree, pager, 1);
	if( btresult != BTREE_OK )
	{
		result = 0;
		return;
	}

	tree->compare = &ibtree_compare;

	char flurb[] = "BLURING BLARGER BOLD";
	ibtree_insert(tree, flurb, sizeof(flurb));

	char churb[] = "BMRBING BLARGER BOLD";
	ibtree_insert(tree, churb, sizeof(churb));

	// end:
	// return result;
}