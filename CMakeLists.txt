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
)

add_executable(test
    src/test.c
    src/pager.c
    src/btree.c
    src/binary_search.c
    src/pager_ops_cstd.c
    src/pager_test.c
)
