

#include "schema_test.h"

#include "btree.h"
#include "btree_defs.h"
#include "btree_op_select.h"
#include "buffer_writer.h"
#include "ibtree.h"
#include "ibtree_layout_schema.h"
#include "ibtree_layout_schema_cmp.h"
#include "ibtree_layout_schema_ctx.h"
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

u32
serialize_my_record(struct MyRecordType* rc, byte* buffer, u32 buffer_size)
{
	byte* ptr = buffer;

	ser_write_32bit_le(ptr, rc->age);
	ptr += 4;

	ser_write_32bit_le(ptr, rc->email_size);
	ptr += 4;
	memcpy(ptr, rc->email, rc->email_size);
	ptr += rc->email_size;

	ser_write_32bit_le(ptr, rc->name_size);
	ptr += 4;
	memcpy(ptr, rc->name, rc->name_size);
	ptr += rc->name_size;

	return ptr - buffer;
}

u32
deserialize_my_record(struct MyRecordType* rc, byte* buffer, u32 buffer_size)
{
	struct BufferReader rdr = {0};
	buffer_reader_init(&rdr, buffer, buffer_size, NULL);

	char temp[sizeof(rc->email_size)] = {0};
	buffer_reader_read(&rdr, temp, sizeof(temp));
	ser_read_32bit_le(&rc->age, temp);

	buffer_reader_read(&rdr, temp, sizeof(temp));
	ser_read_32bit_le(&rc->email_size, temp);
	buffer_reader_read(&rdr, rc->email, rc->email_size);

	buffer_reader_read(&rdr, temp, sizeof(temp));
	ser_read_32bit_le(&rc->name_size, temp);
	buffer_reader_read(&rdr, rc->name, rc->name_size);

	return rdr.bytes_read;
}

int
schema_compare_test(void)
{
	char const db_name[] = "schema_test.db";
	struct BTree* tree = NULL;
	struct BTreeNodeRC rcer = {0};
	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	remove(db_name);
	page_cache_create(&cache, 10);
	u32 page_size = btree_min_page_size() + 1 * 4;
	pager_cstd_create(&pager, cache, db_name, page_size);
	noderc_init(&rcer, pager);

	btree_alloc(&tree);
	ibtree_init(tree, pager, &rcer, 1, &ibtls_compare, &ibtls_reset_compare);

	struct IBTreeLayoutSchema schema = {
		.key_offset = 4,
	};

	schema.nkey_definitions = 2;
	struct IBTLSKeyDef key_one_def = {.type = IBTLSK_TYPE_VARSIZE, .size = 0};
	schema.key_definitions[0] = key_one_def;
	schema.key_definitions[1] = key_one_def;

	struct IBTLSCompareContext ctx = {
		.schema = schema,
		.curr_key = 0,
		.initted = false,
		.type = PAYLOAD_COMPARE_TYPE_RECORD};

	char alice[] = "alice";
	char alice_email[] = "alice@gmail.com";
	char billy[] = "billy";
	// Alice and billy share an email.
	char billy_email[] = "alice@gmail.com";
	char candace[] = "candace";
	char candance_email[] = "candace@gmail.com";
	byte buffer[0x500] = {0};
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
		return 0;
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
		return 0;
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
		return 0;
	}

	byte read_buf[0x500];

	ctx.initted = false;
	ctx.type = PAYLOAD_COMPARE_TYPE_KEY;
	byte key_buf[sizeof(candance_email) + 2 * sizeof(u32) + sizeof(candace)] = {
		0};
	ser_write_32bit_le(key_buf, sizeof(candance_email));
	memcpy(key_buf + 4, candance_email, sizeof(candance_email));
	ser_write_32bit_le(key_buf + 4 + sizeof(candance_email), sizeof(candace));
	memcpy(key_buf + 4 + sizeof(candance_email) + 4, candace, sizeof(candace));
	struct OpSelection op = {0};
	result = btree_op_select_acquire_index(
		tree, &op, (byte*)key_buf, sizeof(key_buf), &ctx);
	if( result != BTREE_OK )
	{
		// printf("Bad acquire select %d\n", result);
		goto fail;
	}

	result = btree_op_select_prepare(&op);
	if( result != BTREE_OK )
	{
		// printf("Bad prepare select %d\n", result);
		goto fail;
	}

	result = btree_op_select_commit(&op, read_buf, op_select_size(&op));
	if( result != BTREE_OK )
	{
		// printf("Bad commit select %d\n", result);
		goto fail;
	}
	struct MyRecordType out = {0};
	deserialize_my_record(&out, read_buf, op_select_size(&op));

	result = btree_op_select_release(&op);
	if( result != BTREE_OK )
	{
		// printf("Bad release %d\n", result);
		goto fail;
	}

	if( memcmp(out.email, candance_email, out.email_size) != 0 )
		result = 0;

end:
	remove(db_name);

	return result;

fail:
	result = 0;
	goto end;
}

int
schema_comparer_test(void)
{
	char const db_name[] = "schema_cmper_test.db";
	struct BTree* tree = NULL;
	struct BTreeNodeRC rcer = {0};
	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	remove(db_name);
	page_cache_create(&cache, 10);
	u32 page_size = btree_min_page_size() + 1 * 4;
	pager_cstd_create(&pager, cache, db_name, page_size);
	noderc_init(&rcer, pager);

	btree_alloc(&tree);
	ibtree_init(tree, pager, &rcer, 1, &ibtls_compare, &ibtls_reset_compare);

	struct IBTreeLayoutSchema schema = {
		.key_offset = 4,
	};

	schema.nkey_definitions = 2;
	struct IBTLSKeyDef key_one_def = {.type = IBTLSK_TYPE_VARSIZE, .size = 0};
	schema.key_definitions[0] = key_one_def;
	schema.key_definitions[1] = key_one_def;

	struct IBTLSCompareContext ctx = {
		.schema = schema,
		.curr_key = 0,
		.initted = false,
		.type = PAYLOAD_COMPARE_TYPE_RECORD};

	char alice[] = "alice";
	char alice_email[] = "alice@gmail.com";
	char billy[] = "billy";
	// Alice and billy share an email.
	char billy_email[] = "alice@gmail.com";
	int result = 1;

	u32 r1len;
	byte r1buffer[0x500] = {0};
	struct MyRecordType r1 = {0};
	memcpy(r1.name, billy, sizeof(billy));
	r1.name_size = sizeof(billy);
	memcpy(r1.email, billy_email, sizeof(billy_email));
	r1.email_size = sizeof(billy_email);
	r1.age = 42;
	r1len = serialize_my_record(&r1, r1buffer, sizeof(r1buffer));

	u32 r2len;
	byte r2buffer[0x500] = {0};
	struct MyRecordType r2 = {0};
	memcpy(r2.name, alice, sizeof(alice));
	r2.name_size = sizeof(alice);
	memcpy(r2.email, alice_email, sizeof(alice_email));
	r2.email_size = sizeof(alice_email);
	r2.age = 22;
	r2len = serialize_my_record(&r2, r2buffer, sizeof(r2buffer));

	byte* window = r1buffer;
	u32 ntotal_bytes_compared = 0;
	u32 nbytes_compared = 0;
	u32 nkey_bytes_remaining = 0;
	u32 window_size = 1;
	ibtls_reset_compare(&ctx);
	int compare_result = ibtls_compare(
		&ctx,
		window,
		window_size,
		r1len,
		r2buffer,
		r2len,
		ntotal_bytes_compared,
		&nbytes_compared,
		&nkey_bytes_remaining);
	ntotal_bytes_compared += nbytes_compared;
	nbytes_compared = 0;
	if( compare_result != 0 || nkey_bytes_remaining == 0 )
		goto fail;

	window_size = 17;
	compare_result = ibtls_compare(
		&ctx,
		window + ntotal_bytes_compared,
		window_size,
		r1len,
		r2buffer,
		r2len,
		ntotal_bytes_compared,
		&nbytes_compared,
		&nkey_bytes_remaining);
	ntotal_bytes_compared += nbytes_compared;
	nbytes_compared = 0;

	if( compare_result != 0 || nkey_bytes_remaining == 0 )
		goto fail;

	window_size = 8;
	compare_result = ibtls_compare(
		&ctx,
		window + ntotal_bytes_compared,
		window_size,
		r1len,
		r2buffer,
		r2len,
		ntotal_bytes_compared,
		&nbytes_compared,
		&nkey_bytes_remaining);
	ntotal_bytes_compared += nbytes_compared;
	nbytes_compared = 0;

	if( compare_result != 0 || nkey_bytes_remaining == 0 )
		goto fail;

	window_size = r1len - ntotal_bytes_compared;
	compare_result = ibtls_compare(
		&ctx,
		window + ntotal_bytes_compared,
		window_size,
		r1len,
		r2buffer,
		r2len,
		ntotal_bytes_compared,
		&nbytes_compared,
		&nkey_bytes_remaining);
	ntotal_bytes_compared += nbytes_compared;
	nbytes_compared = 0;

	// r2 < r1
	if( compare_result != 1 || nkey_bytes_remaining != 0 )
		goto fail;

end:
	remove(db_name);

	return result;

fail:
	result = 0;
	goto end;
}