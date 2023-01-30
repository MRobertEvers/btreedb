#ifndef BTREE_OP_UPDATE_H_
#define BTREE_OP_UPDATE_H_

#include "btint.h"
#include "btree_defs.h"

#include <stdbool.h>

enum op_update_step_e
{
	OP_UPDATE_STEP_UNINIT = 0,
	OP_UPDATE_STEP_INIT,
	OP_UPDATE_STEP_PREPARED,
	OP_UPDATE_STEP_DONE
};
struct OpUpdate
{
	enum btree_e last_status;
	bool initialized;
	bool not_found;
	u32 data_size;
	struct Cursor* cursor;
	enum op_update_step_e step;
	u32 sm_key_buf;
	byte* key;
	u32 key_size;
};

enum btree_e btree_op_update_acquire_tbl(
	struct BTree* tree, struct OpUpdate* op, u32 key, void* cmp_ctx);

enum btree_e btree_op_update_acquire_index(
	struct BTree* tree,
	struct OpUpdate* op,
	byte* key,
	u32 key_size,
	void* cmp_ctx);

enum btree_e btree_op_update_prepare(struct OpUpdate* op);

enum btree_e
btree_op_update_preview(struct OpUpdate* op, void* buffer, u32 buffer_size);

enum btree_e
btree_op_update_commit(struct OpUpdate* op, byte* payload, u32 payload_size);

void btree_op_update_release(struct OpUpdate* op);

u32 op_update_size(struct OpUpdate* op);

#endif