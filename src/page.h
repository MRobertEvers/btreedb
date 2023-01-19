#ifndef PAGE_H_
#define PAGE_H_

#include "btint.h"
#include "pager_e.h"

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

#endif
