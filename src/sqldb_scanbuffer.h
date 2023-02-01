#ifndef SQLDB_SCANBUFFER_H_
#define SQLDB_SCANBUFFER_H_

#include "btint.h"

struct SQLDBScanBuffer
{
	byte* buffer;
	u32 size;
};

void sqldb_scanbuffer_resize(struct SQLDBScanBuffer* buf, u32 min_size);
void sqldb_scanbuffer_free(struct SQLDBScanBuffer* buf);

#endif