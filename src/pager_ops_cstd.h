#ifndef PAGER_OPS_CSTD_H_
#define PAGER_OPS_CSTD_H_

#include "page_cache.h"
#include "pager.h"
#include "pager_ops.h"

extern struct PagerOps CStdOps;

enum pager_e pager_cstd_create(
	struct Pager** r_pager,
	struct PageCache* cache,
	char const* filename,
	unsigned int page_size);

#endif