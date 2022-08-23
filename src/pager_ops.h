#ifndef PAGER_OPS_H_
#define PAGER_OPS_H_

enum pager_e
{
	PAGER_OK,
	PAGER_UNK_ERR,
	PAGER_OPEN_ERR,
	PAGER_SEEK_ERR,
	PAGER_READ_ERR,
	PAGER_WRITE_ERR,
};

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