#ifndef PAGER_H_
#define PAGER_H_

#include "pager_ops.h"

#define PAGE_CREATE_NEW_PAGE 0

struct PageCacheKey
{
	int page_id;
	struct Page* page;

	int ref;
};

struct Pager
{
	char pager_name_str[32];
	struct PagerOps* ops;
	int page_size;
	int max_page;
	void* file;

	struct PageCacheKey* page_cache;
	int page_cache_size;
	int page_cache_capacity;
};

struct Page
{
	int page_id;
	char loaded;

	void* page_buffer;
};

enum pager_e page_create(struct Pager*, struct Page**, int);
enum pager_e page_destroy(struct Pager*, struct Page*);

enum pager_e pager_load(struct Pager*, struct Page**, int);

enum pager_e pager_open(struct Pager*, char*);
enum pager_e pager_close(struct Pager*);
enum pager_e
pager_create(struct Pager** r_pager, struct PagerOps* ops, int page_size);
enum pager_e pager_destroy(struct Pager*);

enum pager_e pager_read_page(struct Pager*, struct Page*);
enum pager_e pager_write_page(struct Pager*, struct Page*);

#endif