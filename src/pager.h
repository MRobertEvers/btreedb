#ifndef PAGER_H_
#define PAGER_H_

#include "pager_ops.h"

struct Pager
{
	char pager_name_str[32];
	struct PagerOps* ops;
	int page_size;
	void* file;
};

struct Page
{
	int page_id;
	char loaded;

	void* page_buffer;
};

enum pager_e pager_page_alloc(struct Pager*, struct Page**);
enum pager_e pager_page_dealloc(struct Page*);
enum pager_e pager_page_init(struct Pager*, struct Page*, int);
enum pager_e pager_page_deinit(struct Page*);

enum pager_e pager_alloc(struct Pager**);
enum pager_e pager_dealloc(struct Pager*);
enum pager_e pager_init(struct Pager*, struct PagerOps* ops, int);
enum pager_e pager_deinit(struct Pager* pager);
enum pager_e pager_open(struct Pager*, char*);
enum pager_e pager_close(struct Pager*);
enum pager_e pager_destroy(struct Pager*);
enum pager_e pager_read_page(struct Pager*, struct Page*);
enum pager_e pager_write_page(struct Pager*, struct Page*);

#endif