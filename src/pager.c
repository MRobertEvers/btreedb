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
pager_open(struct Pager* pager, char* pager_str)
{
	memcpy(
		pager->pager_name_str,
		pager_str,
		min(sizeof(pager->pager_name_str) - 1, strlen(pager_str)));

	return pager->ops->open(&pager->file, pager->pager_name_str);
}

enum pager_e
pager_close(struct Pager* pager)
{
	return pager->ops->close(pager->file);
}

enum pager_e
pager_destroy(struct Pager* pager)
{
	pager_close(pager);
	pager_deinit(pager);
	pager_dealloc(pager);
}

enum pager_e
pager_read_page(struct Pager* pager, struct Page* page)
{
	assert(pager->ops);
	assert(page->page_id);

	int offset = pager->page_size * (page->page_id - 1);
	int pages_read;
	enum pager_e pager_result;

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

	return pager->ops->write(
		pager->file,
		page->page_buffer,
		offset,
		pager->page_size,
		&write_result);
}