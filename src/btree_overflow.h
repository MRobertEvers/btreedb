#ifndef BTREE_OVERFLOW_H_
#define BTREE_OVERFLOW_H_

#include "btint.h"
#include "btree_defs.h"
#include "pager.h"

struct BTreeOverflowPageHeader
{
	u32 next_page_id;
	u32 payload_size;
};

/**
 * @brief Get the max amount of payload bytes writable to an overflow page.
 *
 * @param pager
 * @return u32
 */
u32 btree_overflow_max_write_size(struct Pager* pager);

struct BTreeOverflowReadResult
{
	u32 next_page_id;
	u32 payload_bytes;
};

/**
 * @brief Outpayload may be null
 *
 * @param pager
 * @param page
 * @param page_id
 * @param out
 * @param out_payload
 * @return enum btree_e
 */
enum btree_e btree_overflow_peek(
	struct Pager* pager,
	struct Page* page,
	u32 page_id,
	struct BTreeOverflowReadResult* out,
	byte** out_payload);

enum btree_e btree_overflow_read(
	struct Pager* pager,
	u32 page_id,
	void* buffer,
	u32 buffer_size,
	struct BTreeOverflowReadResult* result);

struct BTreeOverflowWriteResult
{
	u32 page_id;
};
enum btree_e btree_overflow_write(
	struct Pager* pager,
	void* data,
	u32 data_size,
	u32 follow_page_id,
	struct BTreeOverflowWriteResult* result);

#endif