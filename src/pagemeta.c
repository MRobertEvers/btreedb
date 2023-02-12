#include "pagemeta.h"

#include "serialization.h"

#include <stdlib.h>
#include <string.h>

int
pagemeta_size(void)
{
	return 1	// is_free
		   + 4	// Next free page
		   + 4	// Page #
		   + 3; // Don't really need to "reserve" space, but I am anyway.
}

void*
pagemeta_adjust_buffer(void* page_buffer)
{
	char* buf = (char*)page_buffer;
	buf += pagemeta_size();

	return (void*)buf;
}

void*
pagemeta_deadjust_buffer(void* page_buffer)
{
	char* buf = (char*)page_buffer;
	buf -= pagemeta_size();

	return (void*)buf;
}

void
pagemeta_write(struct Page* page, struct PageMetadata* meta)
{
	byte* buffer = pagemeta_deadjust_buffer(page->page_buffer);

	// Is Free
	// 0xCC - free, 0xAF - in use
	buffer[0] = meta->is_free ? 0xCC : 0xAF;

	// Next Free Page
	ser_write_32bit_le(buffer + 1, meta->next_free_page);

	// Page number
	ser_write_32bit_le(buffer + 5, meta->page_number);
}

void
pagemeta_read(struct PageMetadata* out_meta, struct Page* page)
{
	memset(out_meta, 0x00, sizeof(*out_meta));

	byte* buffer = pagemeta_deadjust_buffer(page->page_buffer);

	// TODO: Check for corruption.
	out_meta->is_free = buffer[0] != 0xAF;

	ser_read_32bit_le(&out_meta->next_free_page, buffer + 1);
	ser_read_32bit_le(&out_meta->page_number, buffer + 5);
}

void
pagemeta_memcpy_page(struct Page* dst, struct Page* src, struct Pager* pager)
{
	// assert(dst->page_id == src->page_id);
	memcpy(
		pagemeta_deadjust_buffer(dst->page_buffer),
		pagemeta_deadjust_buffer(src->page_buffer),
		pager->disk_page_size);
}

void
pagemeta_memset_page(struct Page* p, char val, struct Pager* pager)
{
	memset(
		pagemeta_deadjust_buffer(p->page_buffer), val, pager->disk_page_size);
}
