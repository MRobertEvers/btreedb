#ifndef SQLDB_DEFS_H_
#define SQLDB_DEFS_H_

#include "btree_defs.h"
#include "sql_table.h"

struct SQLDBMetaTable
{
	struct BTree* tree;
	struct SQLTable* table;
};

struct SQLDB
{
	struct SQLDBMetaTable tb_tables;
	struct SQLDBMetaTable tb_sequences;
};

#endif