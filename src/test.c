
#include "btree_alg_test.h"
#include "btree_overflow_test.h"
#include "btree_test.h"
#include "btree_utils_test.h"
#include "pager_test.h"
#include "serialization_test.h"

#include <stdio.h>

int
main()
{
	int result = 0;
	result = pager_test_read_write_cstd();
	printf("read/write page: %d\n", result);
	result = pager_test_page_loads_caching();
	printf("pager shared pages from cache: %d\n", result);

	result = btree_alg_test_split_nonleaf();
	printf("alg split non-leaf: %d\n", result);
	result = btree_alg_test_split_leaf();
	printf("alg split leaf: %d\n", result);
	result = btree_alg_test_split_as_parent_nonleaf();
	printf("alg split as parent non-leaf: %d\n", result);
	result = btree_alg_test_split_as_parent_leaf();
	printf("alg split as parent leaf: %d\n", result);

	result = btree_test_insert();
	printf("insert page: %d\n", result);
	result = btree_test_insert_root_with_space();
	printf("insert page root with space: %d\n", result);
	result = btree_test_delete();
	printf("delete key: %d\n", result);
	result = btree_test_split_root_node();
	printf("split root node: %d\n", result);
	result = btree_test_free_heap_calcs();
	printf("free heap calcs: %d\n", result);
	result = btree_utils_test_bin_search_keys();
	printf("bin search keys: %d\n", result);
	result = btree_test_deep_tree();
	printf("deep tree test: %d\n", result);
	result = btree_overflow_test_overflow_rw();
	printf("overflow test: %d\n", result);
	result = serialization_test();
	printf("serialization test: %d\n", result);

	return 0;
}