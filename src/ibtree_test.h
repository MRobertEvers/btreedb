#ifndef IBTREE_TEST_H_
#define IBTREE_TEST_H_

int ibtree_test_insert_shallow(void);
int ibtree_test_insert_split_root(void);

int ibtree_test_deep_tree(void);
int ibta_rotate_test(void);
int ibta_merge_test(void);
int ibta_rebalance_test(void);
int ibta_rebalance_nonleaf_test(void);
int ibta_rebalance_root_fit(void);
int ibta_rebalance_root_nofit(void);
int ibta_cmp_ctx_test(void);
#endif