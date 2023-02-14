#include "pager_freelist.h"

#include "page.h"
#include "pagemeta.h"
#include "pager_internal.h"

#include <assert.h>
#include <stdlib.h>

/**
 * Read the free page list.
 *
 * @param pager
 * @return enum pager_e
 */
enum pager_e
pager_freelist_pop(struct Pager* pager, int* out_free_page)
{
	enum pager_e result = PAGER_OK;

	struct PageSelector selector = {.page_id = 1};
	struct Page* root_page = NULL;
	struct Page* page = NULL;
	struct PageMetadata root_meta = {0};
	struct PageMetadata meta = {0};
	int next_free_page = 0;

	result = page_create(pager, &root_page);
	if( result != PAGER_OK )
		goto end;

	result = page_create(pager, &page);
	if( result != PAGER_OK )
		goto end;

	result = pager_internal_cached_read(pager, &selector, root_page);
	if( result != PAGER_OK )
		goto end;

	pagemeta_read(&root_meta, root_page);

	if( root_meta.next_free_page == 0 )
	{
		result = PAGER_ERR_NO_FREE_PAGE;
		goto end;
	}

	pager_reselect(&selector, root_meta.next_free_page);
	result = pager_internal_cached_read(pager, &selector, page);
	if( result != PAGER_OK )
		goto end;

	pagemeta_read(&meta, page);

	next_free_page = root_meta.next_free_page;
	*out_free_page = next_free_page;

	root_meta.next_free_page = meta.next_free_page;
	pagemeta_write(root_page, &root_meta);

	result = pager_internal_cached_write(pager, root_page);
	if( result != PAGER_OK )
		goto end;

end:
	page_destroy(pager, root_page);
	page_destroy(pager, page);
	return result;
}

enum pager_e
pager_freelist_push(struct Pager* pager, u32 page_number)
{
	enum pager_e result = PAGER_OK;
	struct PageSelector selector = {.page_id = page_number};
	struct Page* page = NULL;
	result = page_create(pager, &page);
	if( result != PAGER_OK )
		goto end;

	result = pager_internal_cached_read(pager, &selector, page);
	if( result != PAGER_OK )
		goto end;

	result = pager_freelist_push_page(pager, page);
	if( result != PAGER_OK )
		goto end;

end:
	page_destroy(pager, page);

	return result;
}

/**
 * The head of the free list is stored on the page 1.
 *
 * TODO: We can write to the page itself immediately,
 * but for concurrent algorithms, we may need to defer
 * updating the root page. We can traverse the list
 * of free pages without worry as there are no references
 * to those pages, but the root page must be synchronized.
 *
 * @param pager
 * @param page
 * @return enum pager_e
 */
enum pager_e
pager_freelist_push_page(struct Pager* pager, struct Page* page)
{
	assert(page->page_id != PAGE_CREATE_NEW_PAGE);
	enum pager_e result = PAGER_OK;

	struct PageSelector selector = {.page_id = 1};
	struct Page* root_page = NULL;
	struct PageMetadata meta = {0};

	result = page_create(pager, &root_page);
	if( result != PAGER_OK )
		goto end;

	result = pager_internal_cached_read(pager, &selector, root_page);
	if( result != PAGER_OK )
		goto end;

	pagemeta_read(&meta, root_page);

	int old_free_head_id = meta.next_free_page;
	meta.next_free_page = page->page_id;

	pagemeta_write(root_page, &meta);

	result = pager_internal_cached_write(pager, root_page);
	if( result != PAGER_OK )
		goto end;

	pagemeta_read(&meta, page);

	meta.is_free = true;
	meta.next_free_page = old_free_head_id;

	pagemeta_write(page, &meta);

	result = pager_internal_cached_write(pager, page);
	if( result != PAGER_OK )
		goto end;

end:
	page_destroy(pager, root_page);
	return result;
}
