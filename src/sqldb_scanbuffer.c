#include "sqldb_scanbuffer.h"

#include <stdlib.h>
#include <string.h>

void
sqldb_scanbuffer_resize(struct SQLDBScanBuffer* buf, u32 min_size)
{
	if( buf->size < min_size )
	{
		if( buf->buffer )
			free(buf->buffer);

		buf->buffer = (byte*)malloc(min_size);
		buf->size = min_size;
	}

	memset(buf->buffer, 0x00, buf->size);
}

void
sqldb_scanbuffer_free(struct SQLDBScanBuffer* buf)
{
	if( buf->buffer )
		free(buf->buffer);
}