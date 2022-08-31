#ifndef PAGER_OPS_H_
#define PAGER_OPS_H_

#include "pager_e.h"

struct PagerOps
{
	enum pager_e (*open)(void** file, char const* filename);
	enum pager_e (*read)(
		void* file, void* buffer, int offset, int read_size, int* bytes_read);
	enum pager_e (*write)(
		void* file,
		void* buffer,
		int offset,
		int write_size,
		int* bytes_written);
	enum pager_e (*close)(void*);
	int (*size)(void*);
};

#endif