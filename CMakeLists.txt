cmake_minimum_required(VERSION 3.10)

project(BtreeDb)

include_directories(src)

# Now build our tools
add_executable(db
    src/main.c
    src/pager.c
    src/btree.c
    src/binary_search.c
)
