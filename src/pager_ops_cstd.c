#include "pager_ops_cstd.h"

#include "pager_ops.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static enum pager_e
open(void** file, char const* filename)
{
	FILE* fp = fopen(filename, "rb+");
	if( !fp )
		fp = fopen(filename, "wb+");

	if( fp )
	{
		*file = fp;
		return PAGER_OK;
	}
	else
	{
		return PAGER_OPEN_ERR;
	}
}

static int
size(void* file)
{
	fseek(file, 0L, SEEK_END);
	return ftell(file);
}

static enum pager_e
close(void* file)
{
	if( fclose(file) == 0 )
		return PAGER_OK;
	else
		return PAGER_UNK_ERR;
}

static enum pager_e
read(void* file, void* buffer, int offset, int read_size, int* bytes_read)
{
	int result;

	result = fseek(file, offset, SEEK_SET);
	if( result != 0 )
		return PAGER_SEEK_ERR;

	result = fread(buffer, read_size, 1, file);
	if( result != 1 )
		return PAGER_READ_ERR;

	return PAGER_OK;
}

static enum pager_e
write(void* file, void* buffer, int offset, int write_size, int* bytes_written)
{
	int seek_result;
	unsigned int write_result;

	seek_result = fseek(file, offset, SEEK_SET);
	if( seek_result != 0 )
		return PAGER_SEEK_ERR;

	write_result = fwrite(buffer, write_size, 1, file);
	if( write_result != 1 )
		return PAGER_WRITE_ERR;

	return PAGER_OK;
}

struct PagerOps CStdOps = {
	.open = &open,	 //
	.close = &close, //
	.read = &read,	 //
	.write = &write, //
	.size = &size	 //
};

enum pager_e
pager_cstd_new(struct Pager** r_pager, char* filename)
{
	enum pager_e pager_result;

	// 4Kb
	pager_result = pager_create(r_pager, &CStdOps, 0x1000);
	if( pager_result != PAGER_OK )
		return pager_result;

	pager_result = pager_open(*r_pager, filename);
	if( pager_result != PAGER_OK )
		return pager_result;

	return PAGER_OK;
}