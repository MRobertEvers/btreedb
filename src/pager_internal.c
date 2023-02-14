#include "pager_internal.h"

#include "page.h"
#include "page_cache.h"
#include "page_defs.h"
#include "pagemeta.h"
#include "pager_ops.h"

#include <assert.h>
#include <stdlib.h>

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
	enum pager_e pager_result = PAGER_OK;

	int offset = pager->disk_page_size * (selector->page_id - 1);
	int pages_read = 0;

	pager_result = pager->ops->read(
		pager->file,
		pagemeta_deadjust_buffer(page->page_buffer),
		offset,
		pager->disk_page_size,
		&pages_read);

	page->page_id = selector->page_id;

	return pager_result;
}

/**
 * @brief Read page from pager.
 *
 * @param selector
 * @param page An already allocated page.
 * @return enum pager_e
 */
enum pager_e
pager_internal_read(
	struct Pager* pager, struct PageSelector* selector, struct Page* page)
{
	return read_from_disk(pager, selector, page);
}

enum pager_e
pager_internal_cached_read(
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
		result = pager_internal_read(pager, selector, cached_page);
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
		pagemeta_memcpy_page(page, cached_page, pager);

		page_cache_release(pager->cache, cached_page);
	}

	page->page_id = page_id;
	page->status = result;

	return result;
}

/**
 * @brief Writes page to disk; if page is NEW, assign page number.
 *
 * @return enum pager_e
 */
enum pager_e
pager_internal_write(struct Pager* pager, struct Page* page)
{
	assert(pager->ops);
	int write_result;
	int offset;

	offset = pager->disk_page_size * (page->page_id - 1);

	pager->ops->write(
		pager->file,
		pagemeta_deadjust_buffer(page->page_buffer),
		offset,
		pager->disk_page_size,
		&write_result);

	pager->max_page =
		page->page_id > pager->max_page ? page->page_id : pager->max_page;

	return PAGER_OK;
}

enum pager_e
pager_internal_cached_write(struct Pager* pager, struct Page* page)
{
	assert(pager->ops);
	enum pager_e result = PAGER_OK;

	// TODO: Hack - all writes also write to disk.
	// Some reads are from cache... need to think this through.
	struct Page* cached_page = NULL;
	result = page_cache_acquire(pager->cache, page->page_id, &cached_page);
	if( result == PAGER_OK )
	{
		pagemeta_memcpy_page(cached_page, page, pager);
		page_cache_release(pager->cache, cached_page);
	}

	pager_internal_write(pager, page);

	// TODO: Error check.
	page->status = PAGER_OK;
	result = PAGER_OK;

	return result;
}