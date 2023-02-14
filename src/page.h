#ifndef PAGE_H_
#define PAGE_H_

#include "page_defs.h"

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
 * @brief Release page; Takes ownership of page
 *
 * @param pager
 * @param page
 * @return enum pager_e
 */
enum pager_e page_destroy(struct Pager* pager, struct Page* page);

#endif