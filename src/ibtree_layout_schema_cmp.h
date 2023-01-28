#ifndef IBTREE_LAYOUT_SCHEMA_CMP_H_
#define IBTREE_LAYOUT_SCHEMA_CMP_H_

#include "btint.h"

#include <stdbool.h>

/**
 * schema
 *
 * The functions provided here are used as a definition of comparison during
 * btree insertion or seek.
 *
 * The functions operate on the notion of a 'cmp key' and an 'rkey', the compare
 * key and the right key.
 *
 * The rkey must reside entirely in a single contiguous memory array. The cmp
 * key does not.
 *
 * The cmp key is passed to these functions in 'windows' where a window may
 * contain 0 or more comparable bytes. Comparable bytes are the bytes that are
 * defined as part of the schema. For example, strings of variable lengths
 * contain comparable bytes, but their lengths are not. Since strings should be
 * varsize for the btree, that means that a string buffer contains   the size
 * of the string at the beginning and then the string itself. An example of such
 * string is [0x04, 0x00, 0x00, 0x00, 'w', 'i', 'l', 'l']. The length bytes of
 * the string are not comparable bytes, whereas 'will' is.
 *
 * A schema allows the key bytes to begin at a certain offset, but must then be
 * otherwise contiguous. For the rkey, the key bytes must be in contiguous
 * memory, for the cmp key, the key bytes must be in consecutive windows with no
 * gaps.
 *
 * The 'key bytes' of the cmp key and the rkey are the start of the bytes after
 * any offset. e.g.
 *
 * [ // Header Info Bytes // , 0x04, 0x00, 0x00, 0x00, 'w', 'i', 'l', 'l' ]
 *                             ^-- key offset
 *
 * During record insertion, there may be a header of information at the start of
 * a record that should not participate in comparison. When this is the keys,
 * i.e. the rkey value contains non-comparable bytes at the start of its memory
 * region, this is PAYLOAD_COMPARE_TYPE_RECORD. However, during lookup, we may
 * not necessarily have that header information, rather we might just have the
 * key bytes. This is called PAYLOAD_COMPARE_TYPE_KEY. It is assumed that the
 * cmp key always contains the header information (because that is how it will
 * be stored in the btree).
 *
 * Ex.
 * # During Insertion: PAYLOAD_COMPARE_TYPE_RECORD
 * [ // Header Info Bytes // , 0x04, 0x00, 0x00, 0x00, 'w', 'i', 'l', 'l' ]
 *
 * # During Lookup: PAYLOAD_COMPARE_TYPE_KEY
 * [ 0x04, 0x00, 0x00, 0x00, 'w', 'i', 'l', 'l' ]
 */

/**
 * @brief A btree compare function.
 *
 * @param compare_context
 * @param cmp_window
 * @param cmp_window_size
 * @param cmp_total_size
 * @param right
 * @param right_size
 * @param bytes_compared
 * @param out_bytes_compared
 * @param out_key_size_remaining
 * @return int
 */
int ibtls_compare(
	void* compare_context,
	void* cmp_window,
	u32 cmp_window_size,
	u32 cmp_total_size,
	void* right,
	u32 right_size,
	u32 bytes_compared,
	u32* out_bytes_compared,
	u32* out_key_size_remaining);

/**
 * @brief Reset the state of the compare context for the next cmp key.
 *
 * @param compare_context
 */
void ibtls_reset_compare(void* compare_context);

#endif