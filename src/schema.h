#ifndef SCHEMA_H_
#define SCHEMA_H_

#include "btint.h"

#include <stdbool.h>

enum schema_key_type_e
{
	SCHEMA_KEY_TYPE_INVALID = 0,
	SCHEMA_KEY_TYPE_FIXED,
	SCHEMA_KEY_TYPE_VARSIZE,
};

enum payload_compare_type_e
{
	// We are comparing two records
	PAYLOAD_COMPARE_TYPE_RECORD,
	// We are comparing records with a key. I.e. We just have the.
	PAYLOAD_COMPARE_TYPE_KEY
};

/**
 * @brief Right now this only supports keys that are a fixed offset away from
 * the start of the record.
 *
 */
struct Schema
{
	u32 key_offset;

	enum schema_key_type_e key_types[4];
	u8 nkeytypes;
};

struct VarsizeKeyState
{
	byte len_bytes[sizeof(u32)];
	u8 len_len;
	u32 key_size;
	// We keep track of the total size of this key already read
	// because we need to find the same offset in the key buffer.
	u32 consumed_size;

	// Offset of this key in the key buffer.
	u32 rkey_size;
	u32 rkey_offset;
	bool ready;
};

struct SchemaCompareContext
{
	bool initted;
	enum payload_compare_type_e type;
	struct Schema schema;
	struct VarsizeKeyState varkeys[4];
	u8 nvarkeys;

	u8 curr_key;
};

int schema_compare(
	void* compare_context,
	void* cmp_window,
	u32 cmp_window_size,
	u32 cmp_total_size,
	void* right,
	u32 right_size,
	u32 bytes_compared,
	u32* out_bytes_compared,
	u32* out_key_size_remaining);

void schema_reset_compare(void* compare_context);

#endif