#ifndef SQLDB_INTERPRET_H_
#define SQLDB_INTERPRET_H_

#include "sql_defs.h"
#include "sql_parse.h"
#include "sqldb_defs.h"

enum sql_e sqldb_interpret(struct SQLDB*, struct SQLParse*);

#endif