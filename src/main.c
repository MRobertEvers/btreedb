#include "btree.h"

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
	btree_alloc(&tree);
	btree_init(tree);

	btree_insert(tree, 12, billy, sizeof(billy));
	btree_insert(tree, 1, charlie, sizeof(charlie));

	return 0;
}