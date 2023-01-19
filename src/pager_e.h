#ifndef PAGER_E_H_
#define PAGER_E_H_

enum pager_e
{
	PAGER_OK,
	// It is not known whether this page exists on disk.
	PAGER_ERR_PAGE_PERSISTENCE_UNKNOWN,
	PAGER_ERR_CACHE_MISS,
	PAGER_ERR_NO_MEM,
	PAGER_ERR_NIF, // Not in file
	PAGER_UNK_ERR,
	PAGER_OPEN_ERR,
	PAGER_SEEK_ERR,
	PAGER_READ_ERR,
	PAGER_WRITE_ERR,
};

#endif
