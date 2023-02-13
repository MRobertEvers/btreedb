#ifndef PAGER_H_
#define PAGER_H_

#include "btint.h"
#include "page.h"
#include "page_cache.h"
#include "pager_ops.h"

#define PAGE_CREATE_NEW_PAGE 0

/**
 * @brief Creates a new non-loaded page object.
 *
 * !Attention! Use commit or destroy when done!
 *
 * @param pager
 * @param page_id
 * @param r_page
 * @return enum pager_e
 */
enum pager_e page_create(struct Pager* pager, struct Page** r_page);

/**
 * @brief Selects the page_id; DOES NOT LOAD THE PAGE.
 *
 * Use this for switch which page you want to read.
 *
 * @param page
 * @param r_page
 */
void pager_reselect(struct PageSelector* selector, int page_id);

/**
 * @brief Release page; Takes ownership of page
 *
 * @param pager
 * @param page
 * @return enum pager_e
 */
enum pager_e page_destroy(struct Pager* pager, struct Page* page);

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
 * TODO: Better way to assign page number?
 *
 * @return enum pager_e
 */
enum pager_e pager_write_page(struct Pager*, struct Page*);

/**
 * @brief Add a page to the free list.
 *
 * @return enum pager_e
 */
enum pager_e pager_free_page_id(struct Pager*, u32 page_number);
enum pager_e pager_free_page(struct Pager*, struct Page*);

enum pager_e pager_extend(struct Pager*, u32* out_page_id);
enum pager_e pager_next_unused(struct Pager*, u32* out_page_id);

int pager_disk_page_size_for(int mem_page_size);
#endif
