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
	page_create(pager, &page, 1);

	pager_read_page(pager, page);

	memcpy(page->page_buffer, buffer, sizeof(buffer));
	// printf("%s\n", page->page_buffer);

	pager_write_page(pager, page);

	memset(page->page_buffer, 0x00, pager->page_size);

	pager_read_page(pager, page);

	// printf("%s\n", page->page_buffer);
	result = memcmp(page->page_buffer, buffer, sizeof(buffer)) == 0;

end:
	page_destroy(pager, page);

	pager_destroy(pager);
	remove("test_.db");

	return result;
}

int
pager_test_page_loads_caching()
{
	char buffer[] = "test_string";
	char page_filename[] = "test_caching_.db";
	int result = 0;
	struct Pager* pager;
	pager_cstd_new(&pager, page_filename);

	struct Page* page_one;
	pager_load(pager, &page_one, 1);

	struct Page* page_two;
	pager_load(pager, &page_two, 1);

	result = page_one->page_buffer == page_two->page_buffer;

end:
	remove(page_filename);

	// TODO: Free pages in cache?

	pager_destroy(pager);

	return result;
}