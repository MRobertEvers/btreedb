#include "pager.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum pager_e
pager_page_alloc(struct Pager* pager, struct Page** r_page)
{
	*r_page = (struct Page*)malloc(sizeof(struct Page));
	return PAGER_OK;
}

enum pager_e
pager_page_dealloc(struct Page* page)
{
	free(page);
	return PAGER_OK;
}

enum pager_e
pager_page_init(struct Pager* pager, struct Page* page, int page_id)
{
	memset(page, 0x00, sizeof(*page));
	page->page_id = page_id;
	page->page_buffer = malloc(pager->page_size);
	memset(page->page_buffer, 0x00, pager->page_size);

	return PAGER_OK;
}

enum pager_e
pager_page_deinit(struct Pager* pager, struct Page* page)
{
	free(page->page_buffer);
	memset(page, 0x00, sizeof(*page));

	return PAGER_OK;
}

enum pager_e
pager_alloc(struct Pager** r_pager)
{
	*r_pager = (struct Pager*)malloc(sizeof(struct Pager));
	return PAGER_OK;
}
enum pager_e
pager_dealloc(struct Pager** pager)
{
	free(pager);
	return PAGER_OK;
}

enum pager_e
pager_init(struct Pager* r_pager, int pager_size)
{
	memset(r_pager, 0x00, sizeof(*r_pager));
	r_pager->page_size = pager_size;

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

	FILE* fp = fopen(pager->pager_name_str, "r+");
	if( !fp )
		fp = fopen(pager->pager_name_str, "wr+");

	if( fp )
	{
		pager->file = fp;
		return PAGER_OK;
	}
	else
	{
		return PAGER_OPEN_ERR;
	}
}

enum pager_e
pager_close(struct Pager* pager)
{
	if( fclose(pager->file) == 0 )
		return PAGER_OK;
	else
		return PAGER_UNK_ERR;
}

enum pager_e
pager_read_page(struct Pager* pager, struct Page* page)
{
	assert(page->page_id);

	int offset = pager->page_size * (page->page_id - 1);
	unsigned int pages_read;
	int seek_result;

	seek_result = fseek(pager->file, offset, SEEK_SET);
	if( seek_result != 0 )
		return PAGER_SEEK_ERR;

	pages_read = fread(page->page_buffer, pager->page_size, 1, pager->file);
	if( pages_read != 1 )
		memset(page->page_buffer, 0x00, pager->page_size);

	return PAGER_OK;
}

enum pager_e
pager_write_page(struct Pager* pager, struct Page* page)
{
	int offset = pager->page_size * (page->page_id - 1);
	int seek_result;
	int write_result;

	seek_result = fseek(pager->file, offset, SEEK_SET);
	if( seek_result != 0 )
		return PAGER_SEEK_ERR;

	write_result = fwrite(page->page_buffer, pager->page_size, 1, pager->file);
	if( write_result != 1 )
		return PAGER_WRITE_ERR;

	return PAGER_OK;
}