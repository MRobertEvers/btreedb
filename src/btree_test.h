#ifndef BTREE_TEST_H
#define BTREE_TEST_H

int btree_test_insert(void);
int btree_test_insert_root_with_space(void);
int btree_test_split_root_node(void);
int btree_test_delete(void);
int btree_test_free_heap_calcs(void);
int btree_test_deep_tree(void);

int btree_test_delete_last(void);
int btree_test_delete_merge_root(void);

#endif