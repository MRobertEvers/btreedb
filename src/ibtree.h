#ifndef IBTREE_H_
#define IBTREE_H_

#include "btint.h"
#include "btree_defs.h"

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