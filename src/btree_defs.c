#include "btree_defs.h"

u32
btree_underflow_lim(struct BTree* tree)
{
	return tree->underflow;
}

u32
btree_underflow_lim_set(struct BTree* tree, u32 underflow)
{
	tree->underflow = underflow;
	return underflow;
}