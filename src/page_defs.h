#ifndef PAGE_DEFS_H_
#define PAGE_DEFS_H_

#include "btint.h"
#include "pager_e.h"

#define PAGE_CREATE_NEW_PAGE 0

struct PageSelector
{
	u32 page_id;
};

struct Page
{
	u32 page_id;
	u32 page_size;
	enum pager_e status;

	void* page_buffer;
};

struct Pager
{
	char pager_name_str[32];
	struct PagerOps* ops;
	// Size of page as it's allocated on disk.
	u32 disk_page_size;
	// Size of page useable by client modules.
	u32 page_size;
	u32 max_page;
	void* file;

	struct PageCache* cache;
};

#endif
