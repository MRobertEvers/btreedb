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

struct KeyBytes
{
	byte* bytes;
	u32 nbytes;

	// Used for cmp key only.
	u32 lwnd;
	bool not_in_window;
	bool has_more;
};

static u32
consume_varsize_bytes(
	struct SchemaCompareContext* ctx, struct Comparison* cmp, u32 lwnd)
{
	byte* cmp_ptr = cmp->cmp_wnd;
	struct VarsizeKeyState* keystate = &ctx->varkeys[ctx->curr_key];
	if( !ctx->varkeys[ctx->curr_key].ready )
	{
		// Read as many key bytes as available.
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

	return lwnd;
}

static u32
configure_fixed_key(struct SchemaCompareContext* ctx, struct Comparison* cmp)
{
	ctx->varkeys[ctx->curr_key].key_size = ctx->rkeys[ctx->curr_key].rkey_size;
	ctx->varkeys[ctx->curr_key].ready = true;

	return 0;
}

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
			// Jump forward to the bytes in the window that are are passed the
			// offset.
			lwnd += (ctx->schema.key_offset - abs_lwnd);
		else
		{
			// There are no key bytes present.
			bytes.not_in_window = true;
			return bytes;
		}
	}

	// If we are not ready, then we have not received all the varsize size
	// bytes.
	switch( ctx->schema.key_definitions[ctx->curr_key].type )
	{
	case SCHEMA_KEY_TYPE_VARSIZE:
		// Yes lwnd = , not +=
		lwnd = consume_varsize_bytes(ctx, cmp, lwnd);
		break;
	case SCHEMA_KEY_TYPE_FIXED:
		configure_fixed_key(ctx, cmp);
		break;
	default:
		assert(0);
	}

	byte* cmp_ptr = cmp->cmp_wnd;
	struct VarsizeKeyState* keystate = &ctx->varkeys[ctx->curr_key];
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
		// There are no comparable bytes in the window.
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
	struct RKeyState* r_keystate = &ctx->rkeys[ctx->curr_key];

	// We received some bytes in the cmp window. We have to point
	// to the corresponding bytes in the key window.
	// Since we may have already compared bytes in that key,
	// we want to move passed those.
	u32 rwnd = cmp_keystate->consumed_size;

	byte* rptr = cmp->payload;
	bytes.bytes = rptr + r_keystate->rkey_offset + rwnd;
	bytes.nbytes = r_keystate->rkey_size - rwnd;

	return bytes;
}

/**
 * @brief Initialize the rkey offsets to contain the offsets of where the start
 * of each segment of comparable bytes
 *
 * @param ctx
 * @param cmp
 */
static void
initialize_rkey_offsets(
	struct SchemaCompareContext* ctx, struct Comparison* cmp)
{
	byte* rptr = cmp->payload;
	u32 offset = 0;
	if( ctx->type == PAYLOAD_COMPARE_TYPE_RECORD )
		offset += ctx->schema.key_offset;

	rptr += offset;

	u32 rkey_size = 0;
	for( int i = 0; i < ctx->schema.nkey_definitions; i++ )
	{
		struct RKeyState* keystate = &ctx->rkeys[i];

		memset(keystate, 0x00, sizeof(*keystate));
		if( ctx->schema.key_definitions[i].type == SCHEMA_KEY_TYPE_VARSIZE )
		{
			rkey_size = 0;
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
			rkey_size = ctx->schema.key_definitions[i].size;
			offset += rkey_size;
			rptr += rkey_size;
		}
	}

	ctx->nvarkeys = ctx->schema.nkey_definitions;
	ctx->initted = true;
}

static void
init_if_not(struct SchemaCompareContext* ctx, struct Comparison* cmp)
{
	if( !ctx->initted )
		initialize_rkey_offsets(ctx, cmp);
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
	int nbytes_to_cmp = 0;
	do
	{
		cmp_bytes = cmpbytes(ctx, &cmp);
		if( cmp_bytes.not_in_window )
		{
			// The only bytes available are not comparable key bytes.
			*cmp.out_key_size_remaining = 1;
			*cmp.out_bytes_compared = cmp_window_size;
			return 0;
		}

		struct VarsizeKeyState* cmp_keystate = &ctx->varkeys[ctx->curr_key];
		struct RKeyState* r_keystate = &ctx->rkeys[ctx->curr_key];
		key_bytes = keybytes(ctx, &cmp);

		nbytes_to_cmp = min(key_bytes.nbytes, cmp_bytes.nbytes);
		cmp_result = memcmp(cmp_bytes.bytes, key_bytes.bytes, nbytes_to_cmp);

		cmp_keystate->consumed_size += nbytes_to_cmp;

		if( cmp_keystate->consumed_size == r_keystate->rkey_size )
			ctx->curr_key += 1;

		// Check if the rkey is completely done.
		// Have we consumed all the rkey bytes and we are on the last key.
		bool rkey_remaining;
		if( cmp_keystate->consumed_size == r_keystate->rkey_size &&
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
	} while( cmp_bytes.has_more && nbytes_to_cmp != 0 );

	assert(nbytes_to_cmp != 0);

	return cmp_result;
}

void
schema_reset_compare(void* compare_context)
{
	// Pass
	struct SchemaCompareContext* ctx =
		(struct SchemaCompareContext*)compare_context;

	memset(ctx->varkeys, 0x00, sizeof(*ctx->varkeys) * ctx->nvarkeys);
	ctx->curr_key = 0;
}