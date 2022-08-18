
#include "pager_test.h"

#include <stdio.h>

int
main()
{
	int result = 0;
	result = pager_test_read_write_cstd();
	printf("read/write page: %d\n", result);
	return 0;
}