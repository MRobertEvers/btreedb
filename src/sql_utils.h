#ifndef SQL_UTILS_H_
#define SQL_UTILS_H_

#include "btree_defs.h"
#include "pager_e.h"
#include "sql_defs.h"

enum sql_e sqlbt_err(enum btree_e err);
enum sql_e sqlpager_err(enum pager_e err);

#endif