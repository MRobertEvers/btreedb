#ifndef PAGER_H_
#define PAGER_H_

#include "btint.h"
#include "page_cache.h"
#include "page_defs.h"
#include "pager_ops.h"

enum pager_e pager_open(struct Pager*, char const*);
enum pager_e pager_close(struct Pager*);

enum pager_e pager_create(
	struct Pager** r_pager,
	struct PagerOps* ops,
	struct PageCache* cache,
	int disk_page_size);
enum pager_e pager_destroy(struct Pager*);

/**
 * @brief Read page from pager.
 *
 * @param selector
 * @param page An already allocated page.
 * @return enum pager_e
 */
enum pager_e pager_read_page(
	struct Pager*, struct PageSelector* selector, struct Page* page);

/**
 * @brief Writes page to disk; if page is NEW, assign page number.
 *
 * @return enum pager_e
 */
enum pager_e pager_write_page(struct Pager*, struct Page*);

enum pager_e pager_extend(struct Pager*, u32* out_page_id);
enum pager_e pager_next_unused(struct Pager*, u32* out_page_id);

int pager_disk_page_size_for(int mem_page_size);
#endif
