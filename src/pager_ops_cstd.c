#include "pager_ops_cstd.h"

#include "pager_ops.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// This is because of some crazy os caching I can't seem to control...
// Posix only.
// #include <unistd.h>
// #include <fcntl.h>

static enum pager_e
co_open(void** file, char const* filename)
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
co_size(void* file)
{
	fseek(file, 0L, SEEK_END);
	return ftell(file);
}

static enum pager_e
co_close(void* file)
{
	if( fclose(file) == 0 )
		return PAGER_OK;
	else
		return PAGER_UNK_ERR;
}

static enum pager_e
co_read(void* file, void* buffer, int offset, int read_size, int* bytes_read)
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
co_write(
	void* file, void* buffer, int offset, int write_size, int* bytes_written)
{
	int seek_result;
	unsigned int write_result;
	assert(offset < 0x10000);

	seek_result = fseek(file, offset, SEEK_SET);
	if( seek_result != 0 )
		return PAGER_SEEK_ERR;

	write_result = fwrite(buffer, write_size, 1, file);
	if( write_result != 1 )
		return PAGER_WRITE_ERR;

	// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fsync.2.html
	// From fsync docs.
	// 	For applications that require tighter guarantees about the integrity of
	//  their data, Mac OS X provides the F_FULLFSYNC fcntl.  The F_FULLFSYNC
	//  fcntl asks the drive to flush all buffered data to permanent storage.
	//  Applications, such as databases, that require a strict ordering of
	//  writes should use F_FULLFSYNC to ensure that their data is written in
	//  the order they expect.  Please see fcntl(2) for more detail.
	// fsync(file);

	// https://developer.apple.com/library/archive/documentation/System/Conceptual/ManPages_iPhoneOS/man2/fcntl.2.html#//apple_ref/doc/man/2/fcntl
	// int ffullsync_result = 0;
	// ffullsync_result = fcntl(fileno(file), F_FULLFSYNC);
	// printf("fcntl %i", ffullsync_result);

	return PAGER_OK;
}

struct PagerOps CStdOps = {
	.open = &co_open,	//
	.close = &co_close, //
	.read = &co_read,	//
	.write = &co_write, //
	.size = &co_size	//
};

enum pager_e
pager_cstd_create(
	struct Pager** r_pager,
	struct PageCache* cache,
	char const* filename,
	unsigned int page_size)
{
	enum pager_e pager_result;

	// 4Kb
	pager_result = pager_create(r_pager, &CStdOps, cache, page_size);
	if( pager_result != PAGER_OK )
		return pager_result;

	pager_result = pager_open(*r_pager, filename);
	if( pager_result != PAGER_OK )
		return pager_result;

	return PAGER_OK;
}