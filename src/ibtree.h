#ifndef IBTREE_H_
#define IBTREE_H_

#include "btint.h"
#include "btree_defs.h"
#include "buffer_writer.h"

/**
 * @brief
 *
 * @param tree
 * @param pager
 * @param root_page_id
 * @return enum btree_e
 */
enum btree_e ibtree_init(
	struct BTree* tree,
	struct Pager* pager,
	struct BTreeNodeRC* rcer,
	u32 root_page_id,
	btree_compare_fn compare,
	btree_compare_reset_fn reset_compare);

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
int ibtree_compare(
	void* compare_context,
	void* cmp_window,
	u32 cmp_window_size,
	void* right,
	u32 right_size,
	u32 bytes_compared,
	u32* out_bytes_compared,
	u32* key_size_remaining);

void ibtree_compare_reset(void* compare_context);

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
enum btree_e
ibtree_insert_ex(struct BTree*, void* data, int data_size, void* cmp_ctx);
enum btree_e ibtree_delete(struct BTree*, void* key, int key_size);
enum btree_e
ibtree_delete_ex(struct BTree*, void* key, int key_size, void* cmp_ctx);

#endif