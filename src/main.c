#include "btree.h"
#include "btree_defs.h"
#include "btree_op_select.h"
#include "buffer_writer.h"
#include "ibtree.h"
#include "noderc.h"
#include "pager_ops_cstd.h"
#include "serialization.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct MyRecordType
{
	byte email[100];
	u32 email_size;
	byte name[100];
	u32 name_size;
	u32 age;
};

static int
min(int left, int right)
{
	return left < right ? left : right;
}

u32
serialize_my_record(struct MyRecordType* rc, byte* buffer, u32 buffer_size)
{
	byte* ptr = buffer;

	ser_write_32bit_le(ptr, rc->email_size);
	ptr += 4;
	memcpy(ptr, rc->email, rc->email_size);
	ptr += rc->email_size;

	ser_write_32bit_le(ptr, rc->name_size);
	ptr += 4;
	memcpy(ptr, rc->name, rc->name_size);
	ptr += rc->name_size;

	ser_write_32bit_le(ptr, rc->age);
	ptr += 4;

	return ptr - buffer;
}

u32
deserialize_my_record(struct MyRecordType* rc, byte* buffer, u32 buffer_size)
{
	struct BufferReader rdr = {0};
	buffer_reader_init(&rdr, buffer, buffer_size, NULL);

	char temp[sizeof(rc->email_size)] = {0};
	buffer_reader_read(&rdr, temp, sizeof(temp));
	ser_read_32bit_le(&rc->email_size, temp);
	buffer_reader_read(&rdr, rc->email, rc->email_size);

	buffer_reader_read(&rdr, temp, sizeof(temp));
	ser_read_32bit_le(&rc->name_size, temp);
	buffer_reader_read(&rdr, rc->name, rc->name_size);

	buffer_reader_read(&rdr, temp, sizeof(temp));
	ser_read_32bit_le(&rc->age, temp);

	return rdr.bytes_read;
}

struct CompareMyRecordContext
{
	byte key_size_buf[4];
	u32 key_size_len;

	byte key_buf[sizeof(((struct MyRecordType*)0)->email)];
	u32 key_len;
};

static int
compare_rr(
	void* compare_context,
	void* cmp_window,
	u32 cmp_window_size,
	u32 cmp_total_size,
	void* right,
	u32 right_size,
	u32 bytes_compared,
	u32* out_bytes_compared,
	u32* out_key_size_remaining)
{
	struct CompareMyRecordContext* ctx =
		(struct CompareMyRecordContext*)compare_context;

	if( *out_key_size_remaining == 0 )
	{
		u32 right_key_size = 0;
		ser_read_32bit_le(&right_key_size, right);
		*out_key_size_remaining = right_key_size;
	}

	// Key is right at the front
	u32 window_offset = 0;
	u32 wnd_key_size = 0;

	byte* ptr = cmp_window;

	while( ctx->key_size_len < 4 && window_offset < cmp_window_size )
	{
		ctx->key_size_buf[ctx->key_size_len] = ptr[0];
		ptr += 1;
		ctx->key_size_len += 1;
		window_offset += 1;
	}

	ser_read_32bit_le(&wnd_key_size, ctx->key_size_buf);

	while( ctx->key_len < wnd_key_size && window_offset < cmp_window_size )
	{
		ctx->key_buf[ctx->key_len] = ptr[0];
		ptr += 1;
		ctx->key_len += 1;
		window_offset += 1;
	}

	*out_key_size_remaining = ctx->key_len;
	*out_bytes_compared = window_offset;

	// TODO: Error if we run out of bytes
	// TODO: Also could eagerly compare bytes but that is harder to keep track
	// of. Just load all the bytes into mem and do the cmp all at once.
	if( ctx->key_len == wnd_key_size )
	{
		u32 right_key_size = 0;
		ser_read_32bit_le(&right_key_size, right);
		byte* rptr = (byte*)right;
		rptr += 4;
		u32 rem = *out_key_size_remaining;

		u32 cmp_bytes = min(right_key_size, wnd_key_size);
		int cmp = memcmp(ctx->key_buf, rptr, cmp_bytes);
		*out_key_size_remaining -= cmp_bytes;
		if( cmp != 0 )
			return cmp < 0 ? -1 : 1;
		rem = *out_key_size_remaining;
		if( rem == 0 && cmp_bytes == wnd_key_size )
			return 0;
		else if( rem == 0 && cmp_bytes != wnd_key_size )
			return 1;
		else
			return -1;
	}
	else
	{
		return 0;
	}
}

static void
reset_compare(void* compare_context)
{
	memset(compare_context, 0x00, sizeof(struct CompareMyRecordContext));
}

int
main()
{
	struct BTree* tree = NULL;
	struct BTreeNodeRC rcer = {0};
	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;
	page_cache_create(&cache, 10);
	pager_cstd_create(&pager, cache, "test.db", 0x1000);
	noderc_init(&rcer, pager);

	btree_alloc(&tree);
	ibtree_init(tree, pager, &rcer, 1, &compare_rr, &reset_compare);

	char alice[] = "alice";
	char alice_email[] = "alice@gmail.com";
	char billy[] = "billy";
	char billy_email[] = "billy@gmail.com";
	char candace[] = "candace";
	char candance_email[] = "candace@gmail.com";
	byte buffer[0x500] = {0};
	struct CompareMyRecordContext ctx = {0};
	struct MyRecordType r = {0};
	u32 len;
	enum btree_e result;

	memcpy(r.name, billy, sizeof(billy));
	r.name_size = sizeof(billy);
	memcpy(r.email, billy_email, sizeof(billy_email));
	r.email_size = sizeof(billy_email);
	r.age = 42;
	len = serialize_my_record(&r, buffer, sizeof(buffer));

	result = ibtree_insert_ex(tree, buffer, len, &ctx);
	if( result != BTREE_OK )
	{
		printf("Bad insert billy %d\n", result);
		return -1;
	}

	memcpy(r.name, alice, sizeof(alice));
	r.name_size = sizeof(alice);
	memcpy(r.email, alice_email, sizeof(alice_email));
	r.email_size = sizeof(alice_email);
	r.age = 22;
	len = serialize_my_record(&r, buffer, sizeof(buffer));

	result = ibtree_insert_ex(tree, buffer, len, &ctx);
	if( result != BTREE_OK )
	{
		printf("Bad insert alice %d\n", result);
		return -1;
	}

	memcpy(r.name, candace, sizeof(candace));
	r.name_size = sizeof(candace);
	memcpy(r.email, candance_email, sizeof(candance_email));
	r.email_size = sizeof(candance_email);
	r.age = 18;
	len = serialize_my_record(&r, buffer, sizeof(buffer));

	result = ibtree_insert_ex(tree, buffer, len, &ctx);
	if( result != BTREE_OK )
	{
		printf("Bad insert candy %d\n", result);
		return -1;
	}

	byte read_buf[0x500];

	byte key_buf[sizeof(candance_email) + sizeof(u32)] = {0};
	memcpy(key_buf + 4, candance_email, sizeof(candance_email));
	ser_write_32bit_le(key_buf, sizeof(candance_email));
	struct OpSelection op = {0};
	result = btree_op_select_acquire_index(
		tree, &op, (byte*)key_buf, sizeof(key_buf), &ctx);
	if( result != BTREE_OK )
	{
		printf("Bad acquire select %d\n", result);
		return -1;
	}

	result = btree_op_select_prepare(&op);
	if( result != BTREE_OK )
	{
		printf("Bad prepare select %d\n", result);
		return -1;
	}

	result = btree_op_select_commit(&op, read_buf, op_select_size(&op));
	if( result != BTREE_OK )
	{
		printf("Bad commit select %d\n", result);
		return -1;
	}
	struct MyRecordType out = {0};
	deserialize_my_record(&out, read_buf, op_select_size(&op));

	result = btree_op_select_release(&op);
	if( result != BTREE_OK )
	{
		printf("Bad release %d\n", result);
		return -1;
	}

	return 0;
}
