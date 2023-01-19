#include "page_cache.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void
assert_cache_correct(struct PageCache* cache)
{
	for( int i = 1; i < cache->size; i++ )
	{
		if( cache->pages[i - 1].page_id >= cache->pages[i].page_id )
		{
			assert(cache->pages[i - 1].page_id < cache->pages[i].page_id);
		}
	}
}

/**
 * @brief Returns insertion location.
 *
 * @param cache
 * @param page_id
 * @param found
 * @return int
 */
static int
find_in_cache(struct PageCache* cache, u32 page_id, char* found)
{
	int left = 0;
	int right = cache->size - 1;
	int mid = 0;

	while( left <= right )
	{
		mid = (right - left) / 2 + left;

		unsigned int cache_page_id = cache->pages[mid].page_id;
		if( cache_page_id == page_id )
		{
			if( found )
				*found = 1;
			return mid;
		}
		else if( cache_page_id < page_id )
		{
			left = mid + 1;
		}
		else
		{
			right = mid - 1;
		}
	}

	return left;
}

static enum pager_e
page_cache_init(struct PageCache* cache, int capacity)
{
	memset(cache, 0x00, sizeof(struct PageCache));

	cache->capacity = capacity;
	cache->pages =
		(struct PageCacheKey*)malloc(sizeof(struct PageCacheKey) * capacity);

	memset(cache->pages, 0x00, sizeof(struct PageCacheKey) * capacity);
	return PAGER_OK;
}

static enum pager_e
page_cache_deinit(struct PageCache* cache)
{
	free(cache->pages);
	return PAGER_OK;
}

enum pager_e
page_cache_create(struct PageCache** r_cache, int capacity)
{
	*r_cache = (struct PageCache*)malloc(sizeof(struct PageCache));
	return page_cache_init(*r_cache, capacity);
}

enum pager_e
page_cache_destroy(struct PageCache* cache)
{
	page_cache_deinit(cache);
	free(cache);
	return PAGER_OK;
}

enum pager_e
page_cache_acquire(
	struct PageCache* cache, int page_number, struct Page** r_page)
{
	char page_found = 0;
	int cache_index = find_in_cache(cache, page_number, &page_found);

	if( page_found )
	{
		struct PageCacheKey* pck = &cache->pages[cache_index];

		pck->ref += 1;
		*r_page = pck->page;

		return PAGER_OK;
	}
	else
	{
		return PAGER_ERR_CACHE_MISS;
	}
}

enum pager_e
page_cache_release(struct PageCache* cache, struct Page* page)
{
	char page_found = 0;
	int cache_index = find_in_cache(cache, page->page_id, &page_found);

	if( page_found )
	{
		struct PageCacheKey* pck = &cache->pages[cache_index];

		pck->ref -= pck->ref != 0 ? 1 : 0;

		return PAGER_OK;
	}
	else
	{
		return PAGER_OK;
	}
}

static void
cache_evict(struct PageCache* cache, struct Page** r_evicted_page)
{
	assert(cache->size != 0);
	int evict_index = -1;

	// TODO: Selection algorithm.
	for( int i = 0; i < cache->size; i++ )
	{
		if( cache->pages[i].ref == 0 )
		{
			evict_index = i;
			break;
		}
	}

	assert(evict_index != -1);

	*r_evicted_page = cache->pages[evict_index].page;

	memmove(
		&cache->pages[evict_index],
		&cache->pages[evict_index + 1],
		sizeof(cache->pages[0]) * (cache->capacity - 1 - (evict_index)));
	memset(&cache->pages[cache->capacity - 1], 0x00, sizeof(cache->pages[0]));

	cache->size -= 1;
}

enum pager_e
page_cache_insert(
	struct PageCache* cache, struct Page* page, struct Page** r_evicted_page)
{
	assert(r_evicted_page);
	assert(page->page_id != 0);

	// We need to evict to make room for this page.
	if( cache->size == cache->capacity )
		cache_evict(cache, r_evicted_page);

	char page_found = 0;
	int cache_index = find_in_cache(cache, page->page_id, &page_found);
	assert(page_found == 0);

	memmove(
		&cache->pages[cache_index + 1],
		&cache->pages[cache_index],
		sizeof(cache->pages[0]) * ((cache->capacity - 1) - cache_index));

	struct PageCacheKey* pck = &cache->pages[cache_index];
	pck->page = page;
	pck->page_id = page->page_id;
	pck->ref = 1;

	assert_cache_correct(cache);

	cache->size += 1;
	return PAGER_OK;
}