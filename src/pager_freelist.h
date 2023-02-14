#ifndef PAGER_FREELIST_H_
#define PAGER_FREELIST_H_

#include "btint.h"
#include "page_defs.h"

enum pager_e pager_freelist_pop(struct Pager* pager, int* out_free_page);
enum pager_e pager_freelist_push(struct Pager* pager, u32 page_number);

/**
 * @brief Add a page to the free list.
 *
 * @return enum pager_e
 */
enum pager_e pager_freelist_push_page(struct Pager*, struct Page*);

#endif