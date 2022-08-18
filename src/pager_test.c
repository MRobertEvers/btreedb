#include "pager_test.h"

#include "pager.h"
#include "pager_ops_cstd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
pager_test_read_write_cstd()
{
	char buffer[] = "test_string";
	int result = 0;
	struct Pager* pager;
	pager_cstd_new(&pager, "test_.db");

	struct Page* page;
	pager_page_alloc(pager, &page);
	pager_page_init(pager, page, 1);

	pager_read_page(pager, page);

	memcpy(page->page_buffer, buffer, sizeof(buffer));
	printf("%s\n", page->page_buffer);

	pager_write_page(pager, page);

	memset(page->page_buffer, 0x00, pager->page_size);

	pager_read_page(pager, page);

	printf("%s\n", page->page_buffer);
	result = memcmp(page->page_buffer, buffer, sizeof(buffer)) == 0;

end:
	pager_page_deinit(pager, page);
	pager_page_dealloc(page);

	pager_destroy(pager);
	remove("test_.db");

	return result;
}