#ifndef IBTREE_LAYOUT_SCHEMA_CTX_H_
#define IBTREE_LAYOUT_SCHEMA_CTX_H_

#include "btint.h"
#include "ibtree_layout_schema.h"

enum payload_compare_type_e
{
	PAYLOAD_COMPARE_TYPE_INVALID = 0,
	// We are comparing two records
	PAYLOAD_COMPARE_TYPE_RECORD,
	// We are comparing records with a key. I.e. We just have the key bytes.
	PAYLOAD_COMPARE_TYPE_KEY
};

/**
 * @brief Used to store information about windowed streaming varkeys.
 *
 * Since the compare key bytes come in windows, this must store relevant state
 * across windows.
 */
struct VarsizeKeyState
{
	byte len_bytes[sizeof(u32)];
	u8 len_len;
	u32 key_size;
	bool ready;
	// We keep track of the total size of this key already read
	// because we need to find the same offset in the key buffer.
	u32 consumed_size;
};

/**
 * @brief Lookup information for comparable offsets in the rkey.
 *
 * Reminder! The rkey must be entirely in memory.
 */
struct RKeyState
{
	// Offset of this key in the key buffer.
	u32 rkey_size;
	u32 rkey_offset;
};

struct IBTLSCompareContext
{
	bool initted;
	enum payload_compare_type_e type;
	struct IBTreeLayoutSchema schema;
	struct VarsizeKeyState varkeys[4];
	struct RKeyState rkeys[4];
	u8 nvarkeys;

	u8 curr_key;
};

#endif