#ifndef PAGE_H_
#define PAGE_H_

#include "pager_e.h"

struct PageSelector
{
	int page_id;
};

struct Page
{
	int page_id;
	unsigned int page_size;
	enum pager_e status;

	void* page_buffer;
};

#endif