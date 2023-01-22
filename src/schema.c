#include "schema.h"

#include "btree_node_debug.h"
#include "serialization.h"

#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static int
min(int left, int right)
{
	return left < right ? left : right;
}

struct Comparison
{
	void* cmp_wnd;
	u32 cmp_wnd_size;
	u32 cmp_total_size;
	u32 cmp_offset;
	void* payload;
	u32 payload_size;
	u32 bytes_compared;
	u32* out_bytes_compared;
	u32* out_key_size_remaining;
};

// u32
// varsize_feed(struct SchemaCompareContext* ctx, struct Comparison* cmp)
// {
// 	byte* ptr = cmp->cmp_wnd;
// 	ptr += cmp->cmp_offset;

// 	while( ctx->key_len_buf_len < 4 && cmp->cmp_offset < cmp->cmp_wnd_size )
// 	{
// 		ctx->key_len_buf[ctx->key_len_buf_len] = ptr[0];
// 		ptr += 1;
// 		ctx->key_len_buf_len += 1;
// 		cmp->cmp_offset += 1;
// 	}

// 	if( ctx->key_len_buf_len == 4 )
// 	{
// 		ser_read_32bit_le(&ctx->key_size, ctx->key_len_buf);
// 		return ctx->key_size;
// 	}
// 	else
// 	{
// 		// Return 1 if we don't know the size yet.
// 		return 1;
// 	}
// }

// u32
// keysize(struct SchemaCompareContext* ctx, struct Comparison* cmp)
// {
// 	if( ctx->key_len_buf_len == 4 )
// 		return ctx->key_size;

// 	switch( ctx->schema.key_type )
// 	{
// 	case SCHEMA_KEY_TYPE_FIXED:
// 		ctx->key_size = ctx->schema.key_size;
// 		//  Set this to 4 so we don't keep re-entering this switch.
// 		ctx->key_len_buf_len = 4;
// 		break;
// 	case SCHEMA_KEY_TYPE_VARSIZE:
// 		ctx->key_size = varsize_feed(ctx, cmp);
// 		break;
// 	default:
// 		assert(0);
// 		break;
// 	}

// 	return ctx->key_size;
// }

struct KeyBytes
{
	byte* bytes;
	u32 nbytes;

	// Used for cmp key only.
	u32 lwnd;
	bool not_in_window;
	bool has_more;
};

struct KeyBytes
cmpbytes(struct SchemaCompareContext* ctx, struct Comparison* cmp)
{
	// ctx
	struct KeyBytes bytes = {0};

	// 1 2 3 4 5 6 7 8
	//       |   |... (key window)
	//
	//   x (bytes compared)
	//      w (bytes compared + window size)
	u32 abs_lwnd = cmp->bytes_compared + cmp->cmp_offset;
	u32 lwnd = cmp->cmp_offset;

	if( abs_lwnd < ctx->schema.key_offset )
	{
		if( abs_lwnd + cmp->cmp_wnd_size >= ctx->schema.key_offset )
			lwnd += (ctx->schema.key_offset - abs_lwnd);
		else
		{
			bytes.not_in_window = true;
			return bytes;
		}
	}

	// We are now passed th[[]]
	byte* cmp_ptr = cmp->cmp_wnd;
	struct VarsizeKeyState* keystate = &ctx->varkeys[ctx->curr_key];
	if( !ctx->varkeys[ctx->curr_key].ready )
	{
		// Try to read key.
		while( lwnd < cmp->cmp_wnd_size &&
			   keystate->len_len < sizeof(keystate->len_bytes) )
		{
			keystate->len_bytes[keystate->len_len++] = cmp_ptr[lwnd];
			lwnd += 1;
		}

		if( keystate->len_len == sizeof(keystate->len_bytes) )
		{
			ser_read_32bit_le(&keystate->key_size, keystate->len_bytes);
			ctx->varkeys[ctx->curr_key].ready = true;
		}
	}

	if( ctx->varkeys[ctx->curr_key].ready )
	{
		bytes.bytes = cmp_ptr + lwnd;
		bytes.nbytes =
			min(cmp->cmp_wnd_size - lwnd,
				keystate->key_size - keystate->consumed_size);
		bytes.has_more = (lwnd + bytes.nbytes) < cmp->cmp_wnd_size;
	}
	else
	{
		bytes.not_in_window = true;
	}

	bytes.lwnd = lwnd;

	return bytes;
}

struct KeyBytes
keybytes(struct SchemaCompareContext* ctx, struct Comparison* cmp)
{
	struct KeyBytes bytes = {0};
	struct VarsizeKeyState* cmp_keystate = &ctx->varkeys[ctx->curr_key];

	// We received some bytes in the cmp window. We have to point
	// to the corresponding bytes in the key window.
	// Since we may have already compared bytes in that key,
	// we want to move passed those.
	u32 rwnd = cmp_keystate->consumed_size;

	byte* rptr = cmp->payload;
	bytes.bytes = rptr + cmp_keystate->rkey_offset + rwnd;
	bytes.nbytes = cmp_keystate->rkey_size - rwnd;

	return bytes;
}

enum adjust_offset_e
{
	ADJUST_OFFSET_BYTES_AVAILABLE,
	ADJUST_OFFSET_NEED_MORE_DATA,
};

// static int
// compare_fixed_size(
// 	struct SchemaCompareContext* compare_context, struct Comparison* cmp)
// {
// 	return 0;
// }

// static int
// compare_varsize(
// 	struct SchemaCompareContext* compare_context, struct Comparison* cmp)
// {
// 	byte* lptr = (byte*)cmp->cmp_wnd;
// 	byte* rptr = (byte*)cmp->payload;
// 	if( compare_context->type == PAYLOAD_COMPARE_TYPE_RECORD )
// 		rptr += compare_context->schema.key_offset;

// 	u32 rkey_size = 0;
// 	ser_read_32bit_le(&rkey_size, rptr);
// 	rptr += 4;

// 	if( cmp->bytes_compared + cmp->cmp_offset >=
// 		compare_context->schema.key_offset )
// 	{
// 		u32 key_offset = (cmp->bytes_compared + cmp->cmp_offset) -
// 						 compare_context->schema.key_offset;

// 		u32 cmp_size =
// 			min(rkey_size - key_offset, compare_context->key_size - key_offset);
// 		*cmp->out_bytes_compared = cmp_size;

// 		int cmp_result = memcmp(lptr, rptr, cmp_size);

// 		u32 rrem = rkey_size - key_offset - cmp_size;
// 		*cmp->out_key_size_remaining = rrem;
// 		u32 lrem = compare_context->key_size - key_offset - cmp_size;

// 		if( cmp_result != 0 )
// 			return cmp_result < 0 ? -1 : 1;
// 		if( rrem == 0 && lrem == 0 )
// 			return 0;
// 		else if( rrem == 0 && lrem != 0 )
// 			return 1;
// 		else
// 			return -1;
// 	}
// 	else
// 	{
// 		return 0;
// 	}
// }

static void
init_if_not(struct SchemaCompareContext* ctx, struct Comparison* cmp)
{
	if( !ctx->initted )
	{
		byte* rptr = cmp->payload;
		u32 offset = 0;
		if( ctx->type == PAYLOAD_COMPARE_TYPE_RECORD )
			offset += ctx->schema.key_offset;

		rptr += offset;
		for( int i = 0; i < ctx->schema.nkeytypes; i++ )
		{
			struct VarsizeKeyState* keystate = &ctx->varkeys[i];

			memset(keystate, 0x00, sizeof(*keystate));
			if( ctx->schema.key_types[i] == SCHEMA_KEY_TYPE_VARSIZE )
			{
				u32 rkey_size = 0;
				ser_read_32bit_le(&rkey_size, rptr);
				offset += 4;
				rptr += 4;

				keystate->rkey_offset = offset;
				keystate->rkey_size = rkey_size;
				offset += rkey_size;
				rptr += rkey_size;
			}
			else
			{
				assert(0);
			}
		}

		ctx->nvarkeys = ctx->schema.nkeytypes;
		ctx->initted = true;
	}
}

int
schema_compare(
	void* compare_context,
	void* cmp_window,
	u32 cmp_window_size,
	u32 cmp_total_size,
	void* payload,
	u32 payload_size,
	u32 bytes_compared,
	u32* out_bytes_compared,
	u32* out_key_size_remaining)
{
	struct SchemaCompareContext* ctx =
		(struct SchemaCompareContext*)compare_context;

	struct Comparison cmp = {
		.cmp_wnd = cmp_window,
		.cmp_wnd_size = cmp_window_size,
		.cmp_total_size = cmp_total_size,
		.cmp_offset = 0,
		.payload = payload,
		.payload_size = payload_size,
		.bytes_compared = bytes_compared,
		.out_bytes_compared = out_bytes_compared,
		.out_key_size_remaining = out_key_size_remaining};
	*cmp.out_bytes_compared = 0;

	struct KeyBytes cmp_bytes = {0};
	struct KeyBytes key_bytes = {0};
	init_if_not(ctx, &cmp);

	int cmp_result = 0;
	do
	{
		cmp_bytes = cmpbytes(ctx, &cmp);
		if( cmp_bytes.not_in_window )
		{
			*cmp.out_key_size_remaining = 1;
			*cmp.out_bytes_compared = cmp_window_size;
			return 0;
		}

		struct VarsizeKeyState* cmp_keystate = &ctx->varkeys[ctx->curr_key];
		key_bytes = keybytes(ctx, &cmp);

		int nbytes_to_cmp = min(key_bytes.nbytes, cmp_bytes.nbytes);
		cmp_result = memcmp(cmp_bytes.bytes, key_bytes.bytes, nbytes_to_cmp);

		cmp_keystate->consumed_size += nbytes_to_cmp;

		if( cmp_keystate->consumed_size == cmp_keystate->rkey_size )
			ctx->curr_key += 1;

		// Check if the rkey is completely done.
		// Have we consumed all the rkey bytes and we are on the last key.
		bool rkey_remaining;
		if( cmp_keystate->consumed_size == cmp_keystate->rkey_size &&
			ctx->curr_key == ctx->nvarkeys )
			rkey_remaining = false;
		else
			rkey_remaining = true;

		// Check if the cmp key is completely done.
		// Have we consumed all the cmp key bytes and we are on the last key.
		bool cmp_remaining;
		if( cmp_keystate->consumed_size == cmp_keystate->key_size &&
			ctx->curr_key == ctx->nvarkeys )
			cmp_remaining = false;
		else
			cmp_remaining = true;

		*cmp.out_bytes_compared += cmp_bytes.lwnd + nbytes_to_cmp;
		cmp.cmp_offset += cmp_bytes.lwnd + nbytes_to_cmp;
		*cmp.out_key_size_remaining = rkey_remaining ? 1 : 0;

		if( cmp_result != 0 )
			return cmp_result;

		if( rkey_remaining && cmp_remaining )
		{} // Do nothing... keep comparing.
		else if( rkey_remaining )
			// The key is longer than the stored bytes.
			return -1;
		else if( cmp_remaining )
			// The stored bytes are longer than the key
			return 1;

		// There are remaining bytes for both keys. Wait for caller to call
		// again.
	} while( cmp_bytes.has_more );

	return cmp_result;
}

void
schema_reset_compare(void* compare_context)
{
	// Pass
	struct SchemaCompareContext* ctx =
		(struct SchemaCompareContext*)compare_context;

	ctx->curr_key = 0;

	for( int i = 0; i < ctx->nvarkeys; i++ )
	{
		ctx->varkeys[i].len_len = 0;
		memset(
			ctx->varkeys[i].len_bytes, 0x00, sizeof(ctx->varkeys[i].len_bytes));
		ctx->varkeys[i].ready = false;
		ctx->varkeys[i].consumed_size = 0;
		ctx->varkeys[i].key_size = 0;
	}
}