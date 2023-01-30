#ifndef SQLDB_SEQ_TBL_H_
#define SQLDB_SEQ_TBL_H_

#include "btree_defs.h"
#include "pager.h"
#include "sql_defs.h"
#include "sql_string.h"
#include "sqldb_defs.h"

struct SQLDBMetaTable sqldb_seq_tbl_create(struct Pager* pager);

enum sql_e sqldb_seq_tbl_next(
	struct SQLDB* db, struct SQLString const* sequence_name, int* out_next);
enum sql_e sqldb_seq_tbl_set(
	struct SQLDB* db, struct SQLString const* sequence_name, int seq);

#endif