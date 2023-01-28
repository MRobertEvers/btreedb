#ifndef IBTREE_LAYOUT_SCHEMA_H_
#define IBTREE_LAYOUT_SCHEMA_H_

#include "btint.h"

// IBTreeLayoutSchemaKey type
enum ibtlsk_type_e
{
	IBTLSK_TYPE_INVALID = 0,
	IBTLSK_TYPE_FIXED,
	IBTLSK_TYPE_VARSIZE,
};

struct IBTLSKeyDef
{
	enum ibtlsk_type_e type;
	// Only used if SCHEMA_KEY_TYPE_FIXED
	u32 size;
};

/**
 * @brief Stores information about the key bytes offset and then the types of
 * keys.
 *
 */
struct IBTreeLayoutSchema
{
	u32 key_offset;

	struct IBTLSKeyDef key_definitions[8];
	u8 nkey_definitions;
};

#endif