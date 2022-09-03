#ifndef PAGER_H_
#define PAGER_H_

#include "page.h"
#include "page_cache.h"
#include "pager_ops.h"

#define PAGE_CREATE_NEW_PAGE 0

struct Pager
{
	char pager_name_str[32];
	struct PagerOps* ops;
	int page_size;
	int max_page;
	void* file;

	struct PageCache* cache;
};

/**
 * @brief Does not use cache
 *
 * !Attention! Use commit or destroy when done!
 *
 * @param pager
 * @param page_id
 * @param r_page
 * @return enum pager_e
 */
enum pager_e
page_create(struct Pager* pager, int page_id, struct Page** r_page);

/**
 * @brief Release page; Takes ownership of page
 *
 * @param pager
 * @param page
 * @return enum pager_e
 */
enum pager_e page_destroy(struct Pager* pager, struct Page* page);

/**
 * @brief Commit new pages; Takes ownership of page
 *
 * Page MUST be written to disk prior to commit.
 *
 * @param pager
 * @param page
 * @return enum pager_e
 */
enum pager_e page_commit(struct Pager* pager, struct Page* page);

enum pager_e pager_open(struct Pager*, char*);
enum pager_e pager_close(struct Pager*);

enum pager_e pager_create(
	struct Pager** r_pager,
	struct PagerOps* ops,
	struct PageCache* cache,
	int page_size);
enum pager_e pager_destroy(struct Pager*);

enum pager_e pager_read_page(struct Pager*, struct Page*);

/**
 * @brief Writes page to disk; if page is NEW, assign page number.
 *
 * TODO: Better way to assign page number?
 *
 * @return enum pager_e
 */
enum pager_e pager_write_page(struct Pager*, struct Page*);

#endif