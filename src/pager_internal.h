#ifndef PAGER_INTERNAL_H_
#define PAGER_INTERNAL_H_

#include "page_defs.h"

/**
 * @brief Read page from pager.
 *
 * @param selector
 * @param page An already allocated page.
 * @return enum pager_e
 */
enum pager_e pager_internal_read(
	struct Pager*, struct PageSelector* selector, struct Page* page);

enum pager_e pager_internal_cached_read(
	struct Pager*, struct PageSelector* selector, struct Page* page);

/**
 * @brief Writes page to disk; if page is NEW, assign page number.
 *
 * @return enum pager_e
 */
enum pager_e pager_internal_write(struct Pager*, struct Page*);

enum pager_e pager_internal_cached_write(struct Pager*, struct Page*);

#endif