#include "pager_test.h"

#include "page_cache.h"
#include "pagemeta.h"
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
	struct PageCache* cache = NULL;
	remove("test_.db");
	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, "test_.db", 0x1000);

	struct Page* page;
	page_create(pager, &page);

	struct PageSelector selector;
	pager_reselect(&selector, 1);
	pager_read_page(pager, &selector, page);

	memcpy(page->page_buffer, buffer, sizeof(buffer));
	// printf("%s\n", page->page_buffer);

	pager_write_page(pager, page);

	memset(page->page_buffer, 0x00, pager->page_size);

	pager_read_page(pager, &selector, page);

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
	remove(page_filename);
	int result = 0;
	struct Pager* pager;
	struct PageCache* cache = NULL;
	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, page_filename, 0x1000);

	struct Page* page_one;
	page_create(pager, &page_one);

	struct PageSelector selector;
	pager_reselect(&selector, 1);
	pager_read_page(pager, &selector, page_one);

	memcpy(page_one->page_buffer, buffer, sizeof(buffer));

	struct Page* page_two;
	page_create(pager, &page_two);

	pager_read_page(pager, &selector, page_two);

	result = memcmp(page_one->page_buffer, buffer, sizeof(buffer)) == 0;
	if( result != 0 )
		goto end;

	result =
		memcmp(
			page_one->page_buffer, page_two->page_buffer, pager->page_size) ==
		0;

end:
	remove(page_filename);

	// TODO: Free pages in cache?

	pager_destroy(pager);

	return result;
}

int
pager_test_free_page_list(void)
{
	char buffer[] = "test_string";
	char page_filename[] = "test_freelist.db";
	remove(page_filename);
	int result = 1;
	struct Pager* pager;
	struct PageCache* cache = NULL;
	struct PageMetadata meta = {0};
	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, page_filename, 0x1000);

	struct Page* page_one;
	struct Page* page_two;
	page_create(pager, &page_one);
	page_create(pager, &page_two);

	pager_write_page(pager, page_one);

	pagemeta_read(&meta, page_one);

	if( meta.is_free != false )
		goto fail;

	if( meta.page_number != 1 )
		goto fail;

	if( meta.next_free_page != 0 )
		goto fail;

	pager_write_page(pager, page_two);
	pagemeta_read(&meta, page_two);

	if( meta.is_free != false )
		goto fail;

	if( meta.page_number != 2 )
		goto fail;

	if( meta.next_free_page != 0 )
		goto fail;

	pager_free_page(pager, page_two);

	pager_read_page(pager, &(struct PageSelector){.page_id = 1}, page_one);
	pagemeta_read(&meta, page_one);

	if( meta.is_free != false )
		goto fail;

	if( meta.page_number != 1 )
		goto fail;

	if( meta.next_free_page != 2 )
		goto fail;

	pager_read_page(pager, &(struct PageSelector){.page_id = 2}, page_two);
	pagemeta_read(&meta, page_two);

	if( meta.is_free != true )
		goto fail;

	if( meta.page_number != 2 )
		goto fail;

	if( meta.next_free_page != 0 )
		goto fail;

	page_destroy(pager, page_two);
	page_create(pager, &page_two);

	memcpy(page_two->page_buffer, buffer, sizeof(buffer));

	if( page_two->page_id != 0 )
		goto fail;

	pager_write_page(pager, page_two);

	if( page_two->page_id != 2 )
		goto fail;

	pager_read_page(pager, &(struct PageSelector){.page_id = 1}, page_one);
	pagemeta_read(&meta, page_one);

	if( meta.is_free != false )
		goto fail;

	if( meta.page_number != 1 )
		goto fail;

	if( meta.next_free_page != 0 )
		goto fail;

end:
	remove(page_filename);
	pager_destroy(pager);

	return result;

fail:
	result = 0;
	goto end;
}