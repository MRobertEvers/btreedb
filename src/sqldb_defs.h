#ifndef SQLDB_DEFS_H_
#define SQLDB_DEFS_H_

#include "btree_defs.h"

struct SQLDB
{
	struct BTree* tb_table_definitions;
	struct BTree* tb_sequences;
};

#endif