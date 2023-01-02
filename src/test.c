
#include "btree_test.h"
#include "pager_test.h"

#include <stdio.h>

int
main()
{
	int result = 0;
	result = pager_test_read_write_cstd();
	printf("read/write page: %d\n", result);
	result = pager_test_page_loads_caching();
	printf("pager shared pages from cache: %d\n", result);

	result = btree_test_insert();
	printf("insert page: %d\n", result);
	result = btree_test_insert_root_with_space();
	printf("insert page: %d\n", result);
	result = btree_test_split_root_node();
	printf("split root node: %d\n", result);
	result = btree_test_free_heap_calcs();
	printf("free heap calcs: %d\n", result);

	return 0;
}