#ifndef BTREE_OVERFLOW_H_
#define BTREE_OVERFLOW_H_

#include "pager.h"

unsigned int btree_overflow_max_write_size(struct Pager* pager);

struct BTreeOverflowReadResult
{
	unsigned int next_page_id;
	unsigned int bytes_read;
};
enum btree_e btree_overflow_read(
	struct Pager* pager,
	unsigned int page_id,
	void* buffer,
	unsigned int buffer_size,
	struct BTreeOverflowReadResult* result);

struct BTreeOverflowWriteResult
{
	unsigned int page_id;
};
enum btree_e btree_overflow_write(
	struct Pager* pager,
	void* data,
	unsigned int data_size,
	unsigned int follow_page_id,
	struct BTreeOverflowWriteResult* result);

#endif