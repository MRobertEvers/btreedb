#ifndef IBTREE_H_
#define IBTREE_H_

#include "btint.h"
#include "btree_defs.h"
#include "buffer_writer.h"

int ibtree_payload_writer(void* data, void* cell, struct BufferWriter* writer);

/**
 * @brief TODO: cmp_chunk_number is ugly
 *
 * @param left
 * @param left_size
 * @param right
 * @param right_size
 * @param bytes_compared The number of bytes already compared.
 * @return enum btree_e
 */
enum btree_e ibtree_compare(
	void* left,
	u32 left_size,
	void* right,
	u32 right_size,
	u32 bytes_compared,
	u32* out_bytes_compared);

/**
 * @brief
 *
 * For indexes, the payload should be the index_key+(page# | primary_key)
 * The left child page is stored after that.
 *
 * @param data
 * @param data_size
 * @return enum btree_e
 */
enum btree_e ibtree_insert(struct BTree*, void* data, int data_size);
enum btree_e ibtree_delete(struct BTree*, void* data, int data_size);

#endif