#ifndef BTREE_OP_SCAN_H_
#define BTREE_OP_SCAN_H_

#include "btree_defs.h"

#include <stdbool.h>

enum op_scan_step_e
{
	OP_SCAN_STEP_UNINIT = 0,
	OP_SCAN_STEP_INIT,
	OP_SCAN_STEP_ITERATING,
	OP_SCAN_STEP_DONE
};

struct OpScan
{
	enum btree_e last_status;
	bool initialized;
	u32 data_size;
	struct Cursor* cursor;
	enum op_scan_step_e step;
};

enum btree_e btree_op_scan_acquire(struct BTree* tree, struct OpScan* op);

enum btree_e btree_op_scan_prepare(struct OpScan* op);

enum btree_e
btree_op_scan_current(struct OpScan* op, void* buffer, u32 buffer_size);

enum btree_e btree_op_scan_next(struct OpScan* op);

enum btree_e
btree_op_scan_update(struct OpScan* op, void* payload, u32 payload_size);

bool btree_op_scan_done(struct OpScan* op);

enum btree_e btree_op_scan_release(struct OpScan* op);

#endif