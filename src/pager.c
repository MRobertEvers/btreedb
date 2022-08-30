#include "pager.h"

#include <assert.h>
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
	page->page_buffer = malloc(pager->page_size);
	memset(page->page_buffer, 0x00, pager->page_size);

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
page_create(struct Pager* pager, struct Page** r_page, int page_number)
{
	pager_page_alloc(pager, r_page);
	pager_page_init(pager, *r_page, page_number);

	// TODO: Errors
	return PAGER_OK;
}

enum pager_e
page_destroy(struct Pager* pager, struct Page* page)
{
	pager_page_deinit(page);
	pager_page_dealloc(page);

	return PAGER_OK;
}

int
find_in_cache(struct Pager* pager, int page_id)
{
	int left = 0;
	int right = pager->page_cache_size - 1;
	int mid = 0;

	while( left <= right )
	{
		mid = (right - left) / 2 + left;

		if( pager->page_cache[mid].page_id == page_id )
		{
			return mid;
		}
		else if( pager->page_cache[mid].page_id < page_id )
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

// enum pager_e
// insert_into_cache(struct Pager* pager, struct Page* page)
// {}

enum pager_e
page_load_into_cache(struct Pager* pager, int page_number, struct Page** r_page)
{
	// TODO: Evict?
	assert(pager->page_cache_size < pager->page_cache_capacity);

	int insert_location = find_in_cache(pager, page_number);

	memmove(
		&pager->page_cache[insert_location + 1],
		&pager->page_cache[insert_location],
		sizeof(pager->page_cache[0]) *
			(pager->page_cache_capacity - insert_location - 1));
	struct PageCacheKey* pck = &pager->page_cache[insert_location];

	pck->page_id = page_number;
	pager_page_alloc(pager, &pck->page);
	pager_page_init(pager, pck->page, page_number);
	pager_read_page(pager, pck->page);

	pager->page_cache_size += 1;

	*r_page = pck->page;

	return PAGER_OK;
}

enum pager_e
pager_load(struct Pager* pager, struct Page** r_page, int page_number)
{
	struct PageCacheKey* pck = NULL;
	int cache_index = 0;
	cache_index = find_in_cache(pager, page_number);
	if( cache_index < pager->page_cache_size )
	{
		pck = &pager->page_cache[cache_index];
	}

	if( pck != NULL )
	{
		*r_page = pck->page;
	}
	else
	{
		page_load_into_cache(pager, page_number, r_page);
	}

	// TODO: Errors
	return PAGER_OK;
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
pager_init(struct Pager* pager, struct PagerOps* ops, int page_size)
{
	memset(pager, 0x00, sizeof(*pager));
	pager->page_size = page_size;
	pager->ops = ops;

	pager->page_cache_size = 0;
	pager->page_cache_capacity = 5;
	pager->page_cache = (struct PageCacheKey*)malloc(
		pager->page_cache_capacity * sizeof(struct PageCacheKey));
	memset(
		pager->page_cache,
		0x00,
		pager->page_cache_capacity * sizeof(struct PageCacheKey));

	return PAGER_OK;
}

enum pager_e
pager_deinit(struct Pager* pager)
{
	for( int i = 0; i < pager->page_cache_size; i++ )
	{
		page_destroy(pager, pager->page_cache[i].page);
	}

	free(pager->page_cache);
	pager->page_cache = NULL;
	pager->page_cache_size = 0;

	return PAGER_OK;
}

static int
min(int left, int right)
{
	return left < right ? left : right;
}
enum pager_e
pager_open(struct Pager* pager, char* pager_str)
{
	memcpy(
		pager->pager_name_str,
		pager_str,
		min(sizeof(pager->pager_name_str) - 1, strlen(pager_str)));

	enum pager_e opened = pager->ops->open(&pager->file, pager->pager_name_str);
	int size = pager->ops->size(pager->file);
	pager->max_page = size / 0x1000;

	return PAGER_OK;
}

enum pager_e
pager_close(struct Pager* pager)
{
	return pager->ops->close(pager->file);
}

enum pager_e
pager_create(struct Pager** r_pager, struct PagerOps* ops, int page_size)
{
	enum pager_e pager_result;
	pager_result = pager_alloc(r_pager);
	if( pager_result != PAGER_OK )
		return pager_result;

	// 4Kb
	pager_result = pager_init(*r_pager, ops, page_size);
	if( pager_result != PAGER_OK )
		return pager_result;

	return PAGER_OK;
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
pager_read_page(struct Pager* pager, struct Page* page)
{
	assert(pager->ops);
	assert(page->page_id);

	int offset = pager->page_size * (page->page_id - 1);
	int pages_read;
	enum pager_e pager_result;

	if( page->page_id > pager->max_page )
	{
		// Hack
		pager->ops->write(
			pager->file, "", page->page_id * pager->page_size, 1, &pages_read);
	}

	// TODO: Wait for result.
	page->loaded = 1;

	pager_result = pager->ops->read(
		pager->file, page->page_buffer, offset, pager->page_size, &pages_read);
	return pager_result;
}

enum pager_e
pager_write_page(struct Pager* pager, struct Page* page)
{
	assert(pager->ops);
	int offset = pager->page_size * (page->page_id - 1);
	int write_result;

	pager->ops->write(
		pager->file,
		page->page_buffer,
		offset,
		pager->page_size,
		&write_result);

	pager->max_page = pager->ops->size(pager->file) / 0x1000;

	return PAGER_OK;
}