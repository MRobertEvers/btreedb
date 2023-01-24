#ifndef PAGE_CACHE_H_
#define PAGE_CACHE_H_

#include "page.h"
#include "pager_e.h"

struct PageCacheKey
{
	int page_id;
	struct Page* page;

	int ref;
};

struct PageCache
{
	struct PageCacheKey* pages;
	int size;
	int capacity;
};

// Page Cache does not load pages
// the pager manages the page cache
// the page cache is a structure for holding pages

// 1. ask pager for a page
// 2. cache miss
// 3. pager creates Page struct
//   - load page
//   - insert into cache
//   - evict on no space; crash if no eviction

enum pager_e page_cache_create(struct PageCache**, int);
enum pager_e page_cache_destroy(struct PageCache*);

enum pager_e page_cache_acquire(
	struct PageCache* cache, int page_number, struct Page** r_page);
enum pager_e page_cache_release(struct PageCache* cache, struct Page* page);

/**
 * @brief Inserts page into cache and acquires
 *
 * @param cache
 * @param page
 * @param r_evicted_page Evicted page if there is one.
 * @return enum pager_e
 */
enum pager_e page_cache_insert(
	struct PageCache* cache, struct Page* page, struct Page** r_evicted_page);

#endif
