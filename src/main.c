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
main()
{
	struct BTree* tree = NULL;
	struct BTreeNodeRC rcer = {0};
	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	remove("test.db");
	page_cache_create(&cache, 10);
	u32 page_size = btree_min_page_size() + 1 * 4;
	pager_cstd_create(&pager, cache, "test.db", page_size);
	noderc_init(&rcer, pager);

	btree_alloc(&tree);
	ibtree_init(tree, pager, &rcer, 1, &ibtls_compare, &ibtls_reset_compare);

	struct IBTreeLayoutSchema schema = {
		.key_offset = 4,
	};

	schema.nkey_definitions = 1;
	struct IBTLSKeyDef key_one_def = {.type = IBTLSK_TYPE_VARSIZE, .size = 0};
	schema.key_definitions[0] = key_one_def;

	struct IBTLSCompareContext ctx = {
		.schema = schema,
		.curr_key = 0,
		.initted = false,
		.type = PAYLOAD_COMPARE_TYPE_RECORD};

	char alice[] = "alice";
	char alice_email[] = "alice@gmail.com";
	char billy[] = "billy";
	char billy_email[] = "billy@gmail.com";
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

	ctx.initted = false;
	ctx.type = PAYLOAD_COMPARE_TYPE_KEY;
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
