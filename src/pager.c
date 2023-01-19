#include "pager.h"

#include "page_cache.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static enum pager_e
pager_page_alloc(struct Pager* pager, struct Page** r_page)
{
	*r_page = (struct Page*)malloc(sizeof(struct Page));
	return PAGER_OK;
}

static enum pager_e
pager_page_dealloc(struct Page* page)
{
	free(page);
	return PAGER_OK;
}

static enum pager_e
pager_page_init(struct Pager* pager, struct Page* page, int page_id)
{
	memset(page, 0x00, sizeof(*page));
	page->page_id = page_id;
	page->status = PAGER_ERR_PAGE_PERSISTENCE_UNKNOWN;
	page->page_size = pager->page_size;
	page->page_buffer = malloc(pager->page_size);
	memset(page->page_buffer, 0x00, pager->page_size);
	// char* temp = ((char*)page->page_buffer);
	// for( int i = 0; i < pager->page_size - 1; i++ )
	// 	temp[i] = 'a' + (i % (('z' - 'a') + 1));

	return PAGER_OK;
}

static enum pager_e
pager_page_deinit(struct Page* page)
{
	free(page->page_buffer);
	memset(page, 0x00, sizeof(*page));

	return PAGER_OK;
}

enum pager_e
page_create(struct Pager* pager, struct Page** r_page)
{
	pager_page_alloc(pager, r_page);
	pager_page_init(pager, *r_page, PAGE_CREATE_NEW_PAGE);

	return PAGER_OK;
}

void
pager_reselect(struct PageSelector* selector, int page_id)
{
	selector->page_id = page_id;
}

enum pager_e
page_destroy(struct Pager* pager, struct Page* page)
{
	pager_page_deinit(page);
	pager_page_dealloc(page);

	return PAGER_OK;
}

/**
 * @brief Commit new pages; Takes ownership of page
 *
 * Page MUST be written to disk prior to commit.
 *
 * @param pager
 * @param page
 * @return enum pager_e
 */
static enum pager_e
page_commit(struct Pager* pager, struct Page* page)
{
	assert(page->page_id != 0);

	struct Page* evicted_page = NULL;
	page_cache_insert(pager->cache, page, &evicted_page);

	if( evicted_page != NULL )
		page_destroy(pager, evicted_page);

	return PAGER_OK;
}

static enum pager_e
read_from_disk(
	struct Pager* pager, struct PageSelector* selector, struct Page* page)
{
	assert(pager->ops != NULL);
	assert(selector->page_id != PAGE_CREATE_NEW_PAGE);

	int offset = pager->page_size * (selector->page_id - 1);
	int pages_read;
	enum pager_e pager_result;

	pager_result = pager->ops->read(
		pager->file, page->page_buffer, offset, pager->page_size, &pages_read);

	page->page_id = selector->page_id;

	return pager_result;
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

enum pager_e
pager_init(
	struct Pager* pager,
	struct PagerOps* ops,
	struct PageCache* cache,
	int page_size)
{
	memset(pager, 0x00, sizeof(*pager));
	pager->page_size = page_size;
	pager->ops = ops;

	pager->cache = cache;

	return PAGER_OK;
}

enum pager_e
pager_deinit(struct Pager* pager)
{
	return PAGER_OK;
}

static int
min(int left, int right)
{
	return left < right ? left : right;
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
	pager->max_page = size / pager->page_size;

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
	int page_size)
{
	enum pager_e pager_result;
	pager_result = pager_alloc(r_pager);
	if( pager_result != PAGER_OK )
		goto err;

	pager_result = pager_init(*r_pager, ops, cache, page_size);
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
	assert(selector->page_id != PAGE_CREATE_NEW_PAGE);

	int page_id = selector->page_id;
	struct Page* cached_page = NULL;
	enum pager_e result =
		page_cache_acquire(pager->cache, page_id, &cached_page);
	if( result == PAGER_ERR_CACHE_MISS )
	{
		page_create(pager, &cached_page);
		result = read_from_disk(pager, selector, cached_page);
		if( result == PAGER_READ_ERR )
		{
			page_destroy(pager, cached_page);
			result = PAGER_ERR_NIF;
		}
		else
		{
			page_commit(pager, cached_page);
			result = PAGER_OK;
		}
	}

	if( result == PAGER_OK )
	{
		memcpy(page->page_buffer, cached_page->page_buffer, pager->page_size);

		page_cache_release(pager->cache, cached_page);
	}

	page->page_id = page_id;
	page->status = result;

	return result;
}

enum pager_e
pager_write_page(struct Pager* pager, struct Page* page)
{
	assert(pager->ops);
	int write_result;
	int offset;

	if( page->page_id == PAGE_CREATE_NEW_PAGE )
	{
		page->page_id = pager->max_page + 1;
	}

	// TODO: Hack - all writes also write to disk.
	// Some reads are from cache... need to think this through.
	struct Page* cached_page;
	enum pager_e cache_result =
		page_cache_acquire(pager->cache, page->page_id, &cached_page);
	if( cache_result == PAGER_OK )
	{
		memcpy(cached_page->page_buffer, page->page_buffer, pager->page_size);
		page_cache_release(pager->cache, cached_page);
	}

	offset = pager->page_size * (page->page_id - 1);

	pager->ops->write(
		pager->file,
		page->page_buffer,
		offset,
		pager->page_size,
		&write_result);

	pager->max_page =
		page->page_id > pager->max_page ? page->page_id : pager->max_page;

	// TODO: Error check.
	page->status = PAGER_OK;

	return PAGER_OK;
}
