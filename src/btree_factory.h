#ifndef BTREE_FACTORY_H_
#define BTREE_FACTORY_H_
#include "btint.h"
#include "btree_defs.h"
#include "pager.h"

struct Pager* btree_factory_pager_create(char const* filename);

struct BTree*
btree_factory_create_ex(struct Pager* pager, enum btree_type_e type, u32 page);

struct BTree* btree_factory_create(char const* filename);
void btree_factory_destroy(struct BTree* tree);

#endif