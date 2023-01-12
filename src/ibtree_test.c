#include "ibtree_test.h"

#include "btree.h"
#include "btree_defs.h"
#include "btree_node.h"
#include "btree_node_reader.h"
#include "ibtree.h"
#include "pager.h"
#include "pager_ops_cstd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
ibtree_test_insert_shallow(void)
{
	char const* db_name = "ibtree_insert_shallow.db";
	int result = 1;
	struct BTreeNode* node = 0;
	struct PageSelector selector;
	struct Pager* pager;
	struct PageCache* cache = NULL;
	struct InsertionIndex index = {0};
	struct Page* test_page = NULL;
	struct BTreeNode* test_node = NULL;

	remove(db_name);
	page_cache_create(&cache, 3);
	// 1 byte of payload can fit on the first page.
	u32 page_size = btree_min_page_size() + 1 * 4;
	pager_cstd_create(&pager, cache, db_name, page_size);

	struct BTree* tree;
	btree_alloc(&tree);
	enum btree_e btresult = ibtree_init(tree, pager, 1, &ibtree_compare);
	if( btresult != BTREE_OK )
	{
		result = 0;
		goto end;
	}

	char flurb[] = "BLURING BLARGER BOLD";
	ibtree_insert(tree, flurb, sizeof(flurb));

	char churb[] = "BMRBING BLARGER BOLD";
	ibtree_insert(tree, churb, sizeof(churb));

	page_create(pager, &test_page);
	pager_reselect(&selector, 1);
	pager_read_page(pager, &selector, test_page);
	btree_node_create_from_page(&test_node, test_page);

	char alpha_buf[sizeof(flurb)] = {0};

	btree_node_read_ex(
		tree,
		test_node,
		tree->pager,
		flurb,
		sizeof(flurb),
		alpha_buf,
		sizeof(alpha_buf));

	if( memcmp(flurb, alpha_buf, sizeof(flurb)) != 0 )
	{
		result = 0;
		goto end;
	}

end:
	if( test_page )
		page_destroy(pager, test_page);
	if( test_node )
		btree_node_destroy(test_node);
	return result;
}