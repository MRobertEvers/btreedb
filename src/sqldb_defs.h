#ifndef SQLDB_DEFS_H_
#define SQLDB_DEFS_H_

#include "btree_defs.h"

struct SQLDBMetaTables
{
	struct BTree* tb_table_definitions;
	struct BTree* tb_sequences;
};

struct SQLDB
{
	struct SQLDBMetaTables meta;
};

#endif