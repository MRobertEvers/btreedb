#ifndef PAGEMETA_H_
#define PAGEMETA_H_

#include "btint.h"
#include "page_defs.h"

#include <stdbool.h>

struct PageMetadata
{
	bool is_free;
	u32 next_free_page;
	u32 page_number;
};

int pagemeta_size(void);

/**
 * @brief Pager stores metadata at the beginning of each page.
 *
 * Shift the buffer by that amount.
 *
 * @param page_buffer
 * @return void*
 */
void* pagemeta_adjust_buffer(void* page_buffer);

void* pagemeta_deadjust_buffer(void* page_buffer);

void pagemeta_write(struct Page* page, struct PageMetadata* meta);

void pagemeta_read(struct PageMetadata* out_meta, struct Page* page);
void
pagemeta_memcpy_page(struct Page* dst, struct Page* src, struct Pager* pager);
void pagemeta_memset_page(struct Page* p, char val, struct Pager* pager);

#endif