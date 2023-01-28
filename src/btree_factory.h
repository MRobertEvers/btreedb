#ifndef BTREE_FACTORY_H_
#define BTREE_FACTORY_H_

struct BTree* btree_factory_create(char const* filename);
void btree_factory_destroy(struct BTree* tree);

#endif