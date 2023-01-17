#include "btree_overflow.h"

#include "btree_defs.h"
#include "btree_utils.h"
#include "serialization.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

u32
btree_overflow_max_write_size(struct Pager* pager)
{
	return pager->page_size - sizeof(u32) * 2;
}

enum btree_e
btree_overflow_peek(
	struct Pager* pager,
	struct Page* page,
	u32 page_id,
	byte** out_payload,
	struct BTreeOverflowReadResult* out)
{
	u32 bytes_on_page = 0;
	u32 next_page_id = 0;
	enum btree_e read_result = BTREE_OK;
	struct PageSelector selector = {0};

	out->next_page_id = 0;
	out->payload_bytes = 0;

	pager_reselect(&selector, page_id);
	read_result = btpage_err(pager_read_page(pager, &selector, page));
	if( read_result != BTREE_OK )
		goto end;

	// dbg_print_buffer(page->page_buffer, page->page_size);

	byte* payload_buffer = (byte*)page->page_buffer;
	// TODO: Serialization
	memcpy(&next_page_id, payload_buffer, sizeof(next_page_id));

	payload_buffer += sizeof(bytes_on_page);
	memcpy(&bytes_on_page, payload_buffer, sizeof(bytes_on_page));
	assert(bytes_on_page < pager->page_size);

	payload_buffer += sizeof(next_page_id);
	*out_payload = payload_buffer;

	out->next_page_id = next_page_id;
	out->payload_bytes = bytes_on_page;

end:
	return read_result;
}

enum btree_e
btree_overflow_read(
	struct Pager* pager,
	u32 page_id,
	void* buffer,
	u32 buffer_size,
	struct BTreeOverflowReadResult* out)
{
	struct Page* page = NULL;
	enum btree_e read_result = BTREE_OK;
	byte* payload = NULL;

	out->next_page_id = 0;
	out->payload_bytes = 0;

	read_result = btpage_err(page_create(pager, &page));
	if( read_result != BTREE_OK )
		goto end;

	read_result = btree_overflow_peek(pager, page, page_id, &payload, out);
	if( read_result != BTREE_OK )
		goto end;

	if( out->payload_bytes > buffer_size )
	{
		read_result = BTREE_ERR_NODE_NOT_ENOUGH_SPACE;
		goto end;
	}

	memcpy(buffer, payload, out->payload_bytes);

end:
	if( page )
		page_destroy(pager, page);

	return read_result;
}

enum btree_e
btree_overflow_write(
	struct Pager* pager,
	void* data,
	u32 data_size,
	u32 follow_page_id,
	struct BTreeOverflowWriteResult* out)
{
	enum btree_e result = BTREE_OK;
	struct Page* page = NULL;

	if( data_size > btree_overflow_max_write_size(pager) )
		return BTREE_ERR_NODE_NOT_ENOUGH_SPACE;

	result = btpage_err(page_create(pager, &page));
	if( result != BTREE_OK )
		goto end;

	byte* payload_buffer = (byte*)page->page_buffer;
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
	if( page )
		page_destroy(pager, page);

	return result;
}