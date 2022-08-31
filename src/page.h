#ifndef PAGE_H_
#define PAGE_H_

struct Page
{
	int page_id;
	char loaded;

	void* page_buffer;
};

#endif