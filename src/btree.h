#ifndef BTREE_H_
#define BTREE_H_

#include "btint.h"
#include "btree_defs.h"
#include "pager.h"

u32 btree_min_page_size(void);

enum btree_e btree_alloc(struct BTree**);
enum btree_e btree_dealloc(struct BTree*);
enum btree_e btree_init(
	struct BTree* tree,
	struct Pager* pager,
	struct BTreeNodeRC* rcer,
	u32 root_page_id);
enum btree_e btree_deinit(struct BTree* tree);

enum btree_e btree_insert(struct BTree*, int key, void* data, int data_size);

/**
 * @brief
 * STEP 1 Find leaf L containing (key,pointer) entry to delete
 * STEP 2 Remove entry from L
 * STEP 2a If L meets the "half full" criteria, then we're done.
 * STEP 2b Otherwise, L has too few data entries.
 * STEP 3 If L's right sibling can spare an entry, then move smallest entry in
 * right sibling to L STEP 3a Else, if L's left sibling can spare an entry then
 * move largest entry in left sibling to L STEP 3b Else, merge L and a sibling
 * STEP 4 If merging, then recursively deletes the entry (pointing toL or
 * sibling) from the parent. STEP 5 Merge could propagate to root, decreasing
 * height
 * @param key
 * @return enum btree_e
 */
enum btree_e btree_delete(struct BTree*, int key);

#endif