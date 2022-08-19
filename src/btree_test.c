#include "btree_test.h"

#include "btree.h"
#include "pager_ops_cstd.h"

int
btree_test_insert_root_with_space()
{
	int result = 1;
	struct Pager* pager;
	pager_cstd_new(&pager, "test_.db");

	struct BTree* tree;
	btree_alloc(&tree);
	btree_init(tree, pager);

	char billy[] = "billy";
	char ruth[] = "ruth";
	btree_insert(tree, 12, billy, sizeof(billy));
	btree_insert(tree, 1, ruth, sizeof(ruth));

	if( tree->root->header->num_keys != 2 )
	{
		result = 0;
		goto end;
	}

	if( tree->root->keys[0].key != 1 || tree->root->keys[1].key != 12 )
	{
		result = 0;
		goto end;
	}

end:
	btree_deinit(tree);
	btree_dealloc(tree);

	return result;
}