#ifndef PAGER_OPS_CSTD_H_
#define PAGER_OPS_CSTD_H_

#include "pager.h"
#include "pager_ops.h"

extern struct PagerOps CStdOps;

enum pager_e pager_cstd_new(struct Pager** r_pager, char* filename);

#endif