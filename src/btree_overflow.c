#include "btree_overflow.h"

#include "btree_defs.h"
#include "btree_utils.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

unsigned int
btree_overflow_max_write_size(struct Pager* pager)
{
	return pager->page_size - sizeof(unsigned int) * 2;
}

enum btree_e
btree_overflow_read(
	struct Pager* pager,
	unsigned int page_id,
	void* buffer,
	unsigned int buffer_size,
	struct BTreeOverflowReadResult* result)
{
	enum pager_e page_result = PAGER_OK;
	struct Page* page = NULL;
	unsigned int bytes_on_page = 0;
	unsigned int next_page_id = 0;
	enum btree_e read_result = BTREE_OK;

	result->next_page_id = 0;
	result->bytes_read = 0;

	page_create(pager, &page);

	struct PageSelector select = {0};
	pager_reselect(&select, page_id);
	read_result = btpage_err(pager_read_page(pager, &select, page));
	if( read_result != BTREE_OK )
		goto end;

	// dbg_print_buffer(page->page_buffer, page->page_size);

	char* payload_buffer = (char*)page->page_buffer;
	memcpy(&next_page_id, payload_buffer, sizeof(next_page_id));

	payload_buffer += sizeof(bytes_on_page);
	memcpy(&bytes_on_page, payload_buffer, sizeof(bytes_on_page));
	assert(bytes_on_page < pager->page_size);

	if( bytes_on_page > buffer_size )
	{
		read_result = BTREE_ERR_NODE_NOT_ENOUGH_SPACE;
		goto end;
	}

	payload_buffer += sizeof(next_page_id);
	memcpy(buffer, payload_buffer, bytes_on_page);

	result->next_page_id = next_page_id;
	result->bytes_read = bytes_on_page;

end:
	page_destroy(pager, page);

	return read_result;
}

enum btree_e
btree_overflow_write(
	struct Pager* pager,
	void* data,
	unsigned int data_size,
	unsigned int follow_page_id,
	struct BTreeOverflowWriteResult* out)
{
	enum btree_e result = BTREE_OK;
	struct Page* page = NULL;

	if( data_size > btree_overflow_max_write_size(pager) )
		return BTREE_ERR_NODE_NOT_ENOUGH_SPACE;

	page_create(pager, &page);

	char* payload_buffer = (char*)page->page_buffer;
	memcpy(payload_buffer, &follow_page_id, sizeof(follow_page_id));
	payload_buffer += sizeof(follow_page_id);

	memcpy(payload_buffer, &data_size, sizeof(data_size));
	payload_buffer += sizeof(data_size);

	memcpy(payload_buffer, data, data_size);

	// dbg_print_buffer(page->page_buffer, page->page_size);

	result = btpage_err(pager_write_page(pager, page));
	if( result != BTREE_OK )
		goto end;

	out->page_id = page->page_id;

end:
	page_destroy(pager, page);

	return result;
}