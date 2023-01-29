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
    src/btree_op_update.c
    src/btree_op_select.c
    src/btree_node_debug.c
    src/btree_cell.c
    src/btree_node_writer.c
    src/btree_node_reader.c
    src/btree_overflow.c
    src/serialization.c
    src/noderc.c
    src/ibtree_layout_schema_cmp.c
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
    src/btree_op_scan.c
    src/btree_op_update.c
    src/btree_op_select.c
    src/btree_node_debug.c
    src/btree_cell.c
    src/btree_node_writer.c
    src/btree_node_reader.c
    src/btree_overflow.c
    src/serialization.c
    src/noderc.c
    src/ibtree_layout_schema_cmp.c
    src/buffer_writer.c
    src/btree_factory.c
    src/sql_dt.c
    src/sql_ibtree.c
    src/sql_literalstr.c
    src/sql_value.c
    src/sql_parse@flex.c
    src/sql_parse.c
    src/sql_parsed.c
    src/sql_parsegen.c
    src/sql_record.c
    src/sql_string.c
    src/sql_table.c
    src/sqldb.c
    src/sqldb_meta_tbls.c
    src/sqldb_seq_tbl.c
    src/sqldb_table.c
    src/sql_utils.c
    bison/sql_lexer.c
    bison/sql_lexer_utils.c
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
    src/btree_op_update.c
    src/btree_op_select.c
    src/btree_node_debug.c
    src/btree_cell.c
    src/btree_node_writer.c
    src/btree_node_reader.c
    src/btree_overflow.c
    src/serialization.c
    src/ibtree_layout_schema_cmp.c
    src/buffer_writer.c
)

