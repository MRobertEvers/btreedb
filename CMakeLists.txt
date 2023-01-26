cmake_minimum_required(VERSION 3.10)

project(BtreeDb)

include_directories(src)
include_directories(bison)

# Now build our tools
add_executable(db
    src/main.c
    src/pager.c
    src/btree.c
    src/ibtree.c
    src/pager_ops_cstd.c
    src/page_cache.c
    src/btree_node.c
    src/btree_utils.c
    src/btree_defs.c
    src/btree_alg.c
    src/ibtree_alg.c
    src/btree_cursor.c
    src/btree_op_select.c
    src/btree_node_debug.c
    src/btree_cell.c
    src/btree_node_writer.c
    src/btree_node_reader.c
    src/btree_overflow.c
    src/serialization.c
    src/noderc.c
    src/schema.c
    src/buffer_writer.c
)

if(MSVC)
  target_compile_options(db PRIVATE /W4 /WX)
else()
  target_compile_options(db PRIVATE -Wall -Wextra -Wpedantic -Werror -Wno-unused-parameter -Wno-newline-eof -Wno-sign-compare)
endif()

# Now build our tools
add_executable(sql_main
    src/sql_main.c
    src/pager.c
    src/btree.c
    src/ibtree.c
    src/pager_ops_cstd.c
    src/page_cache.c
    src/btree_node.c
    src/btree_utils.c
    src/btree_defs.c
    src/btree_alg.c
    src/ibtree_alg.c
    src/btree_cursor.c
    src/btree_op_select.c
    src/btree_node_debug.c
    src/btree_cell.c
    src/btree_node_writer.c
    src/btree_node_reader.c
    src/btree_overflow.c
    src/serialization.c
    src/noderc.c
    src/schema.c
    src/buffer_writer.c
    bison/sql_lexer.c
)

add_executable(test
    src/test.c
    src/pager_test.c
    src/btree_test.c
    src/ibtree_test.c
    src/btree_utils_test.c
    src/btree_alg_test.c
    src/btree_overflow_test.c
    src/btree_cursor_test.c
    src/serialization_test.c
    src/schema_test.c
    src/noderc.c
    src/pager.c
    src/btree.c
    src/ibtree.c
    src/pager_ops_cstd.c
    src/page_cache.c
    src/btree_node.c
    src/btree_utils.c
    src/btree_defs.c
    src/btree_alg.c
    src/ibtree_alg.c
    src/btree_cursor.c
    src/btree_op_select.c
    src/btree_node_debug.c
    src/btree_cell.c
    src/btree_node_writer.c
    src/btree_node_reader.c
    src/btree_overflow.c
    src/serialization.c
    src/schema.c
    src/buffer_writer.c
)

