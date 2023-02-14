#include "page.h"

#include "pagemeta.h"

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
	page->status = PAGER_ERR_PAGE_PERSISTENCE_UNKNOWN;
	page->page_size = pager->page_size;

	void* page_buffer = malloc(pager->disk_page_size);
	page->page_buffer = pagemeta_adjust_buffer(page_buffer);

	pagemeta_memset_page(page, 0x00, pager);

	return PAGER_OK;
}

static enum pager_e
pager_page_deinit(struct Page* page)
{
	free(pagemeta_deadjust_buffer(page->page_buffer));
	memset(page, 0x00, sizeof(*page));

	return PAGER_OK;
}

void
pager_reselect(struct PageSelector* selector, int page_id)
{
	selector->page_id = page_id;
}

enum pager_e
page_create(struct Pager* pager, struct Page** r_page)
{
	pager_page_alloc(pager, r_page);
	pager_page_init(pager, *r_page, PAGE_CREATE_NEW_PAGE);

	return PAGER_OK;
}

enum pager_e
page_destroy(struct Pager* pager, struct Page* page)
{
	pager_page_deinit(page);
	pager_page_dealloc(page);

	return PAGER_OK;
}