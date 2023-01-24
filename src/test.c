
#include "btree_alg_test.h"
#include "btree_cursor_test.h"
#include "btree_overflow_test.h"
#include "btree_test.h"
#include "btree_utils_test.h"
#include "ibtree_test.h"
#include "pager_test.h"
#include "schema_test.h"
#include "serialization_test.h"

#include <stdio.h>

int
main()
{
	int result = 0;
	// result = pager_test_read_write_cstd();
	// printf("read/write page: %d\n", result);
	// result = pager_test_page_loads_caching();
	// printf("pager shared pages from cache: %d\n", result);

	// result = btree_alg_test_split_nonleaf();
	// printf("alg split non-leaf: %d\n", result);
	// result = btree_alg_test_split_leaf();
	// printf("alg split leaf: %d\n", result);
	// result = btree_alg_test_split_as_parent_nonleaf();
	// printf("alg split as parent non-leaf: %d\n", result);
	// result = btree_alg_test_split_as_parent_leaf();
	// printf("alg split as parent leaf: %d\n", result);

	// result = btree_test_insert();
	// printf("insert page: %d\n", result);
	// result = btree_test_insert_root_with_space();
	// printf("insert page root with space: %d\n", result);
	// result = btree_test_delete();
	// printf("delete key: %d\n", result);
	// result = btree_test_split_root_node();
	// printf("split root node: %d\n", result);
	// result = btree_test_free_heap_calcs();
	// printf("free heap calcs: %d\n", result);
	// result = btree_utils_test_bin_search_keys();
	// printf("bin search keys: %d\n", result);
	// result = btree_test_deep_tree();
	// printf("deep tree test: %d\n", result);
	// result = btree_overflow_test_overflow_rw();
	// printf("overflow test: %d\n", result);
	// result = serialization_test();
	// printf("serialization test: %d\n", result);

	// result = ibtree_test_insert_shallow();
	// printf("ibtree insert shallow: %d\n", result);
	// result = ibtree_test_insert_split_root();
	// printf("ibtree insert split_root: %d\n", result);

	// result = ibtree_test_deep_tree();
	// printf("ibtree deep test: %d\n", result);
	// result = ibta_rotate_test();
	// printf("rotate: %d\n", result);
	// result = ibta_merge_test();
	// printf("merge: %d\n", result);
	// result = ibta_rebalance_test();
	// printf("rebalance: %d\n", result);
	// result = ibta_rebalance_nonleaf_test();
	// printf("rebalance nonleaf: %d\n", result);
	// result = btree_alg_rotate_test();
	// printf("bt rotate nonleaf: %d\n", result);
	// result = btree_alg_rotate_leaf_test();
	// printf("bt rotate leaf: %d\n", result);
	// result = btree_alg_merge_test();
	// printf("bt merge: %d\n", result);
	// result = btree_alg_merge_nonleaf_test();
	// printf("bt merge nonleaf: %d\n", result);
	// result = btree_alg_merge_nonleaf_l_test();
	// printf("bt merge nonleaf l: %d\n", result);
	// result = btree_alg_rebalance();
	// printf("bt rebalance: %d\n", result);
	// result = ibta_rebalance_root_fit();
	// printf("ibt rebalance root fit: %d\n", result);
	// result = ibta_rebalance_root_nofit();
	// printf("ibt rebalance root nofit: %d\n", result);
	// result = bta_rebalance_root_fit();
	// printf("bt rebalance root fit: %d\n", result);
	// result = bta_rebalance_root_nofit();
	// printf("bt rebalance root nofit: %d\n", result);
	// result = ibta_cmp_ctx_test();
	// printf("compare with ctx: %d\n", result);
	// result = schema_comparer_test();
	// printf("schema comparer test: %d\n", result);
	// result = schema_compare_test();
	// printf("schema compare test: %d\n", result);
	// result = cursor_iter_test();
	// printf("cursor iter test: %d\n", result);
	result = cursor_iter_test2();
	printf("cursor iter test: %d\n", result);

	return 0;
}