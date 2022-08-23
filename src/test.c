
#include "btree_test.h"
#include "pager_test.h"

#include <stdio.h>

int
main()
{
	int result = 0;
	// result = pager_test_read_write_cstd();
	// printf("read/write page: %d\n", result);
	// result = btree_test_insert_root_with_space();
	// printf("insert page: %d\n", result);
	result = btree_test_split_root_node();
	printf("split root node: %d\n", result);

	return 0;
}