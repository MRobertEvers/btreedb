#ifndef SQLDB_H_
#define SQLDB_H_

#include "sql_defs.h"

struct SQLDB
{};

enum sql_e sqldb_create(struct SQLDB** out_sqldb);

// TODO: Not sure if this belongs here.
// TODO: RecordRC
// enum sql_e sqldb_prepare_record(struct SQLDB* sqldb, );

#endif