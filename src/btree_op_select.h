#ifndef BTREE_OP_SELECT_H_
#define BTREE_OP_SELECT_H_

#include "btree_defs.h"

#include <stdbool.h>

enum op_selection_step_e
{
	OP_SELECTION_STEP_UNINIT = 0,
	OP_SELECTION_STEP_INIT,
	OP_SELECTION_STEP_PREPARED,
	OP_SELECTION_STEP_NOT_FOUND,
	OP_SELECTION_STEP_DONE
};
struct OpSelection
{
	enum btree_e last_status;
	bool initialized;
	u32 data_size;
	struct Cursor* cursor;
	enum op_selection_step_e step;
	u32 sm_key_buf;
	byte* key;
	u32 key_size;
};

// Table table
enum btree_e btree_op_select_acquire_tbl(
	struct BTree* tree, struct OpSelection* op, u32 key, void* cmp_ctx);
// Index table
enum btree_e btree_op_select_acquire_index(
	struct BTree* tree,
	struct OpSelection* op,
	byte* key,
	u32 key_size,
	void* cmp_ctx);

enum btree_e btree_op_select_prepare(struct OpSelection* op);

enum btree_e
btree_op_select_commit(struct OpSelection* op, void* buffer, u32 buffer_size);

enum btree_e btree_op_select_release(struct OpSelection* op);

u32 op_select_size(struct OpSelection* op);

#endif