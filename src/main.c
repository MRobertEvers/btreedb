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
	struct BTree* tree;

	struct Pager* pager;
	pager_cstd_new(&pager, "test.db");

	btree_alloc(&tree);
	btree_init(tree, pager);

	btree_insert(tree, 12, billy, sizeof(billy));
	btree_insert(tree, 1, charlie, sizeof(charlie));

	return 0;
}