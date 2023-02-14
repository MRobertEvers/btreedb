#include "pager.h"

#include "page.h"
#include "page_cache.h"
#include "pagemeta.h"
#include "pager_freelist.h"
#include "pager_internal.h"
#include "serialization.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int
min(int left, int right)
{
	return left < right ? left : right;
}

enum pager_e
pager_alloc(struct Pager** r_pager)
{
	*r_pager = (struct Pager*)malloc(sizeof(struct Pager));
	return PAGER_OK;
}

enum pager_e
pager_dealloc(struct Pager* pager)
{
	free(pager);
	return PAGER_OK;
}

static enum pager_e
pager_init(
	struct Pager* pager,
	struct PagerOps* ops,
	struct PageCache* cache,
	int disk_page_size)
{
	memset(pager, 0x00, sizeof(*pager));
	pager->disk_page_size = disk_page_size;
	pager->page_size = disk_page_size - pagemeta_size();
	pager->ops = ops;

	pager->cache = cache;

	return PAGER_OK;
}

enum pager_e
pager_deinit(struct Pager* pager)
{
	return PAGER_OK;
}

enum pager_e
pager_open(struct Pager* pager, char const* pager_str)
{
	memcpy(
		pager->pager_name_str,
		pager_str,
		min(sizeof(pager->pager_name_str) - 1, strlen(pager_str)));

	pager->ops->open(&pager->file, pager->pager_name_str);
	int size = pager->ops->size(pager->file);
	pager->max_page = size / pager->disk_page_size;

	return PAGER_OK;
}

enum pager_e
pager_close(struct Pager* pager)
{
	return pager->ops->close(pager->file);
}

enum pager_e
pager_create(
	struct Pager** r_pager,
	struct PagerOps* ops,
	struct PageCache* cache,
	int disk_page_size)
{
	enum pager_e pager_result;
	pager_result = pager_alloc(r_pager);
	if( pager_result != PAGER_OK )
		goto err;

	pager_result = pager_init(*r_pager, ops, cache, disk_page_size);
	if( pager_result != PAGER_OK )
		goto err;

	return pager_result;
err:
	pager_dealloc(*r_pager);
	*r_pager = NULL;

	return pager_result;
}

enum pager_e
pager_destroy(struct Pager* pager)
{
	pager_close(pager);
	pager_deinit(pager);
	pager_dealloc(pager);
	return PAGER_OK;
}

enum pager_e
pager_read_page(
	struct Pager* pager, struct PageSelector* selector, struct Page* page)
{
	return pager_internal_cached_read(pager, selector, page);
}

static void
write_new_page_meta(struct Page* page, int page_number)
{
	struct PageMetadata meta = {
		.is_free = false, .next_free_page = 0, .page_number = page_number};

	pagemeta_write(page, &meta);
}

static enum pager_e
pager_disk_page(struct Pager* pager, struct Page* page)
{
	enum pager_e result = PAGER_OK;
	int next_page_id = 0;
	if( pager->max_page == 0 )
		page->page_id = 1;
	else
	{
		result = pager_freelist_pop(pager, &next_page_id);
		if( result != PAGER_OK && result != PAGER_ERR_NO_FREE_PAGE )
			goto end;

		if( result == PAGER_OK )
			page->page_id = next_page_id;
		else
			page->page_id = pager->max_page + 1;

		result = PAGER_OK;
	}

	write_new_page_meta(page, page->page_id);

end:
	return result;
}

static void
set_in_use(struct Page* page)
{
	struct PageMetadata old;
	pagemeta_read(&old, page);
	struct PageMetadata meta = {
		.is_free = false,
		.next_free_page = old.next_free_page,
		.page_number = page->page_id};

	pagemeta_write(page, &meta);
}

enum pager_e
pager_write_page(struct Pager* pager, struct Page* page)
{
	assert(pager->ops);
	enum pager_e result = PAGER_OK;

	if( page->page_id == PAGE_CREATE_NEW_PAGE )
	{
		// Get the page
		result = pager_disk_page(pager, page);
		if( result != PAGER_OK )
			goto end;
	}

	set_in_use(page);

	result = pager_internal_cached_write(pager, page);

end:
	return result;
}

enum pager_e
pager_extend(struct Pager* pager, u32* out_page_id)
{
	enum pager_e result = PAGER_OK;
	struct Page* page = NULL;

	result = page_create(pager, &page);
	if( result != PAGER_OK )
		goto end;

	page->page_id = pager->max_page + 1;

	result = pager_write_page(pager, page);
	if( result != PAGER_OK )
		goto end;

	*out_page_id = page->page_id;

end:
	if( page )
		page_destroy(pager, page);
	return result;
}

enum pager_e
pager_next_unused(struct Pager* pager, u32* out_page_id)
{
	*out_page_id = pager->max_page + 1;
	return PAGER_OK;
}

int
pager_disk_page_size_for(int mem_page_size)
{
	return mem_page_size + pagemeta_size();
}