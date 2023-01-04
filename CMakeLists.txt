cmake_minimum_required(VERSION 3.10)

project(BtreeDb)

include_directories(src)

# Now build our tools
add_executable(db
    src/main.c
    src/pager.c
    src/btree.c
    src/binary_search.c
    src/pager_ops_cstd.c
    src/page_cache.c
    src/btree_node.c
    src/btree_utils.c
    src/btree_alg.c
    src/btree_cursor.c
    src/btree_node_debug.c
)

add_executable(test
    src/test.c
    src/pager_test.c
    src/btree_test.c
    src/btree_utils_test.c
    src/btree_alg_test.c
    src/pager.c
    src/btree.c
    src/binary_search.c
    src/pager_ops_cstd.c
    src/page_cache.c
    src/btree_node.c
    src/btree_utils.c
    src/btree_alg.c
    src/btree_cursor.c
    src/btree_node_debug.c
)
