#ifndef BTREE_NODE_DEBUG_H_
#define BTREE_NODE_DEBUG_H_

#include "btree_defs.h"

void dbg_print_node(struct BTreeNode* node);

void dbg_print_buffer(void const* buffer, unsigned int buffer_size);

#endif