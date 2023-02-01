#ifndef SQLDB_SCAN_H_
#define SQLDB_SCAN_H_

#include "sql_defs.h"
#include "sql_record.h"
#include "sqldb_defs.h"

#include <stdbool.h>

struct SQLDBScan
{
	struct SQLDB* db;
	struct SQLString* table_name;
	struct SQLRecord* current_record;

	void* internal;
};

enum sql_e
sqldb_scan_acquire(struct SQLDB*, struct SQLString*, struct SQLDBScan*);
enum sql_e sqldb_scan_next(struct SQLDBScan*);
enum sql_e sqldb_scan_update(struct SQLDBScan*, struct SQLRecord*);
bool sqldb_scan_done(struct SQLDBScan*);
struct SQLRecord* sqldb_scan_record(struct SQLDBScan*);
void sqldb_scan_release(struct SQLDBScan*);

#endif