#ifndef BTREE_ALG_TEST_H_
#define BTREE_ALG_TEST_H_

int btree_alg_test_split_nonleaf(void);
int btree_alg_test_split_leaf(void);
int btree_alg_test_split_as_parent_leaf(void);
int btree_alg_test_split_as_parent_nonleaf(void);
int btree_alg_rotate_test(void);
int btree_alg_rotate_leaf_test(void);
int btree_alg_merge_test(void);
int btree_alg_merge_nonleaf_test(void);
int btree_alg_merge_nonleaf_l_test(void);
int btree_alg_rebalance(void);

#endif