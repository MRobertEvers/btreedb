#include "btree_alg_test.h"

#include "btree.h"
#include "btree_alg.h"
#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_node_reader.h"
#include "btree_node_writer.h"
#include "btree_utils.h"
#include "noderc.h"
#include "page_cache.h"
#include "pager_ops_cstd.h"

#include <stdio.h>
#include <string.h>

int
btree_alg_test_split_nonleaf(void)
{
	char const* db_name = "btree_test_alg_split.db";
	int result = 1;
	struct Pager* pager;

	struct PageSelector selector = {0};
	struct PageCache* cache = NULL;

	struct BTreeCellInline cell = {0};

	remove(db_name);

	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, db_name, 0x1000);

	struct BTreeNode* node = {0};
	struct Page* page = NULL;
	struct InsertionIndex inserter = {.mode = KLIM_END};
	struct BTreeNodeRC rcer = {0};
	noderc_init(&rcer, pager);

	page_create(pager, &page);

	btree_node_create_as_page_number(&node, 1, page);
	node->header->is_leaf = 0;

	unsigned int fake_child_page_id_as_key;

	cell.inline_size = sizeof(fake_child_page_id_as_key);
	cell.payload = &fake_child_page_id_as_key;

	fake_child_page_id_as_key = 3;
	btree_node_insert_inline(
		node, &inserter, fake_child_page_id_as_key, (void*)&cell);
	fake_child_page_id_as_key = 4;
	btree_node_insert_inline(
		node, &inserter, fake_child_page_id_as_key, (void*)&cell);
	fake_child_page_id_as_key = 5;
	btree_node_insert_inline(
		node, &inserter, fake_child_page_id_as_key, (void*)&cell);
	fake_child_page_id_as_key = 6;
	node->header->right_child = fake_child_page_id_as_key;

	struct SplitPage split_result = {0};
	bta_split_node(node, &rcer, &split_result);

	struct BTreeNode* other_node = {0};
	struct Page* other_page = NULL;
	page_create(pager, &other_page);

	selector.page_id = split_result.left_page_id;
	pager_read_page(pager, &selector, other_page);

	btree_node_create_from_page(&other_node, other_page);

	if( other_node->page_number != 2 || node->page_number != 1 )
	{
		result = 0;
		goto end;
	}

	if( other_node->header->num_keys != 1 || node->header->num_keys != 1 )
	{
		result = 0;
		goto end;
	}

	if( other_node->header->right_child != 4 || node->header->right_child != 6 )
	{
		result = 0;
		goto end;
	}

end:
	btree_node_destroy(node);
	btree_node_destroy(other_node);
	page_destroy(pager, page);
	page_destroy(pager, other_page);
	page_cache_destroy(cache);
	pager_destroy(pager);
	remove(db_name);
	return result;
}

int
btree_alg_test_split_leaf(void)
{
	char const* db_name = "bta_test_split_leaf.db";
	int result = 1;
	struct Pager* pager;

	struct PageSelector selector = {0};
	struct PageCache* cache = NULL;
	struct BTreeCellInline cell = {0};

	remove(db_name);

	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, db_name, 0x1000);

	struct BTreeNode* node = {0};
	struct Page* page = NULL;
	struct InsertionIndex inserter = {.mode = KLIM_END};

	page_create(pager, &page);

	btree_node_create_as_page_number(&node, 1, page);
	node->header->is_leaf = 1;

	unsigned int fake_child_page_id_as_key;

	cell.inline_size = sizeof(fake_child_page_id_as_key);
	cell.payload = &fake_child_page_id_as_key;

	fake_child_page_id_as_key = 3;
	btree_node_insert_inline(
		node, &inserter, fake_child_page_id_as_key, (void*)&cell);
	fake_child_page_id_as_key = 4;
	btree_node_insert_inline(
		node, &inserter, fake_child_page_id_as_key, (void*)&cell);
	fake_child_page_id_as_key = 5;
	btree_node_insert_inline(
		node, &inserter, fake_child_page_id_as_key, (void*)&cell);
	fake_child_page_id_as_key = 6;
	btree_node_insert_inline(
		node, &inserter, fake_child_page_id_as_key, (void*)&cell);

	struct SplitPage split_result = {0};
	struct BTreeNodeRC rcer = {0};
	noderc_init(&rcer, pager);
	bta_split_node(node, &rcer, &split_result);

	struct BTreeNode* other_node = {0};
	struct Page* other_page = NULL;
	page_create(pager, &other_page);

	selector.page_id = split_result.left_page_id;
	pager_read_page(pager, &selector, other_page);

	btree_node_create_from_page(&other_node, other_page);

	if( other_node->page_number != 2 || node->page_number != 1 )
	{
		result = 0;
		goto end;
	}

	if( other_node->header->num_keys != 2 || node->header->num_keys != 2 )
	{
		result = 0;
		goto end;
	}

	if( other_node->header->right_child != 0 || node->header->right_child != 0 )
	{
		result = 0;
		goto end;
	}

end:
	btree_node_destroy(node);
	btree_node_destroy(other_node);
	page_destroy(pager, page);
	page_destroy(pager, other_page);
	page_cache_destroy(cache);
	pager_destroy(pager);
	remove(db_name);
	return result;
}

int
btree_alg_test_split_as_parent_nonleaf(void)
{
	char const* db_name = "bta_split_p_nonleaf.db";
	int result = 1;
	struct Pager* pager;

	struct PageSelector selector = {0};
	struct PageCache* cache = NULL;

	struct BTreeCellInline cell = {0};

	remove(db_name);

	struct BTreeNodeRC rcer = {0};

	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, db_name, 0x1000);
	noderc_init(&rcer, pager);

	struct NodeView nv = {0};
	struct InsertionIndex inserter = {.mode = KLIM_END};

	noderc_acquire(&rcer, &nv);
	noderc_reinit_as(&rcer, &nv, 1);
	nv.node.header->is_leaf = 0;

	struct BTreeNode* node = nv_node(&nv);

	unsigned int fake_child_page_id_as_key;

	cell.inline_size = sizeof(fake_child_page_id_as_key);
	cell.payload = &fake_child_page_id_as_key;

	fake_child_page_id_as_key = 3;
	btree_node_insert_inline(
		node, &inserter, fake_child_page_id_as_key, (void*)&cell);
	fake_child_page_id_as_key = 4;
	btree_node_insert_inline(
		node, &inserter, fake_child_page_id_as_key, (void*)&cell);
	fake_child_page_id_as_key = 5;
	btree_node_insert_inline(
		node, &inserter, fake_child_page_id_as_key, (void*)&cell);
	fake_child_page_id_as_key = 6;
	node->header->right_child = fake_child_page_id_as_key;

	pager_write_page(pager, node->page);

	struct SplitPageAsParent split_result = {0};

	bta_split_node_as_parent(nv_node(&nv), &rcer, &split_result);

	struct BTreeNode* left_node = {0};
	struct BTreeNode* right_node = {0};
	struct Page* left_page = NULL;
	struct Page* right_page = NULL;
	page_create(pager, &left_page);
	page_create(pager, &right_page);

	selector.page_id = split_result.left_child_page_id;
	pager_read_page(pager, &selector, left_page);

	selector.page_id = split_result.right_child_page_id;
	pager_read_page(pager, &selector, right_page);

	btree_node_create_from_page(&left_node, left_page);
	btree_node_create_from_page(&right_node, right_page);

	if( left_node->page_number != 2 || node->page_number != 1 ||
		right_node->page_number != 3 )
	{
		result = 0;
		goto end;
	}

	if( node->header->num_keys != 1 || left_node->header->num_keys != 1 ||
		right_node->header->num_keys != 1 )
	{
		result = 0;
		goto end;
	}

	if( node->header->right_child != right_node->page_number )
	{
		result = 0;
		goto end;
	}

end:
	btree_node_destroy(left_node);
	btree_node_destroy(right_node);
	page_destroy(pager, left_page);
	page_destroy(pager, right_page);
	page_cache_destroy(cache);
	pager_destroy(pager);
	remove(db_name);
	return result;
}

int
btree_alg_test_split_as_parent_leaf(void)
{
	char const* db_name = "bta_split_p_leaf.db";
	int result = 1;
	struct Pager* pager;

	struct PageSelector selector = {0};
	struct PageCache* cache = NULL;

	struct BTreeCellInline cell = {0};

	remove(db_name);

	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, db_name, 0x1000);

	struct BTreeNode* node = {0};
	struct Page* page = NULL;
	struct InsertionIndex inserter = {.mode = KLIM_END};
	struct BTreeNodeRC rcer = {0};
	noderc_init(&rcer, pager);
	page_create(pager, &page);

	btree_node_create_as_page_number(&node, 1, page);
	node->header->is_leaf = 1;

	unsigned int fake_child_page_id_as_key;

	cell.inline_size = sizeof(fake_child_page_id_as_key);
	cell.payload = &fake_child_page_id_as_key;

	fake_child_page_id_as_key = 3;
	btree_node_insert_inline(node, &inserter, fake_child_page_id_as_key, &cell);
	fake_child_page_id_as_key = 4;
	btree_node_insert_inline(node, &inserter, fake_child_page_id_as_key, &cell);
	fake_child_page_id_as_key = 5;
	btree_node_insert_inline(node, &inserter, fake_child_page_id_as_key, &cell);
	fake_child_page_id_as_key = 6;
	btree_node_insert_inline(node, &inserter, fake_child_page_id_as_key, &cell);
	pager_write_page(pager, node->page);

	struct SplitPageAsParent split_result = {0};
	bta_split_node_as_parent(node, &rcer, &split_result);

	struct BTreeNode* left_node = {0};
	struct BTreeNode* right_node = {0};
	struct Page* left_page = NULL;
	struct Page* right_page = NULL;
	page_create(pager, &left_page);
	page_create(pager, &right_page);

	selector.page_id = split_result.left_child_page_id;
	pager_read_page(pager, &selector, left_page);

	selector.page_id = split_result.right_child_page_id;
	pager_read_page(pager, &selector, right_page);

	btree_node_create_from_page(&left_node, left_page);
	btree_node_create_from_page(&right_node, right_page);

	if( left_node->page_number != 2 || node->page_number != 1 ||
		right_node->page_number != 3 )
	{
		result = 0;
		goto end;
	}

	if( node->header->num_keys != 1 || left_node->header->num_keys != 2 ||
		right_node->header->num_keys != 2 )
	{
		result = 0;
		goto end;
	}

	if( node->header->right_child != right_node->page_number )
	{
		result = 0;
		goto end;
	}

end:
	btree_node_destroy(node);
	btree_node_destroy(left_node);
	btree_node_destroy(right_node);
	page_destroy(pager, page);
	page_destroy(pager, left_page);
	page_destroy(pager, right_page);
	page_cache_destroy(cache);
	pager_destroy(pager);
	remove(db_name);
	return result;
}

int
btree_alg_rotate_test(void)
{
	char const* db_name = "btree_alg_rotate_test.db";
	remove(db_name);

	int result = 1;
	struct BTreeNode test_node = {0};
	struct BTreeNode* parent_node = NULL;
	struct BTreeNode* left_node = NULL;
	struct BTreeNode* right_node = NULL;
	struct BTreeNode* l1_node = NULL;
	struct BTreeNode* l2_node = NULL;
	struct BTreeNode* l3_node = NULL;
	struct BTreeNode* l4_node = NULL;
	struct BTreeNode* l5_node = NULL;
	struct Page* parent_page = NULL;
	struct Page* left_page = NULL;
	struct Page* right_page = NULL;
	struct Page* l1_page = NULL;
	struct Page* l2_page = NULL;
	struct Page* l3_page = NULL;
	struct Page* l4_page = NULL;
	struct Page* l5_page = NULL;
	struct Page* test_page = NULL;

	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	page_cache_create(&cache, 11);
	pager_cstd_create(&pager, cache, db_name, 0x1000);
	struct BTreeNodeRC rcer;
	noderc_init(&rcer, pager);
	struct BTree* tree;
	btree_alloc(&tree);
	if( btree_init(tree, pager, &rcer, 2) != BTREE_OK )
	{
		result = 0;
		goto end;
	}

	page_create(pager, &test_page);
	page_create(pager, &parent_page);
	page_create(pager, &left_page);
	page_create(pager, &right_page);
	page_create(pager, &l1_page);
	page_create(pager, &l2_page);
	page_create(pager, &l3_page);
	page_create(pager, &l4_page);
	page_create(pager, &l5_page);

	btree_node_create_as_page_number(&parent_node, 2, parent_page);
	btree_node_create_as_page_number(&left_node, 3, left_page);
	btree_node_create_as_page_number(&right_node, 4, right_page);
	btree_node_create_as_page_number(&l1_node, 5, l1_page);
	btree_node_create_as_page_number(&l2_node, 6, l2_page);
	btree_node_create_as_page_number(&l3_node, 7, l3_page);
	btree_node_create_as_page_number(&l4_node, 8, l4_page);
	btree_node_create_as_page_number(&l5_node, 9, l5_page);

	// This test is an impossibility, but it covers what we want.
	// We've given the cells children but also label them as leaves.
	l1_node->header->is_leaf = 1;
	l2_node->header->is_leaf = 1;
	l3_node->header->is_leaf = 1;
	l4_node->header->is_leaf = 1;
	l5_node->header->is_leaf = 1;

	u32 left_node_right_most_child = l3_node->page_number;
	u32 right_node_right_most_child = l5_node->page_number;
	left_node->header->right_child = left_node_right_most_child;
	right_node->header->right_child = right_node_right_most_child;

	parent_node->header->right_child = right_node->page_number;

	pager_write_page(pager, parent_page);
	pager_write_page(pager, left_page);
	pager_write_page(pager, right_page);
	char A[] = "A";
	char B[] = "B";
	char C[] = "C";
	char D[] = "D";
	char E[] = "E";

	//         3
	// 1,   2,   *  |     4, *
	// 1  | 2 |  3  |   4 | 5
	struct InsertionIndex insert_index = {0};
	insert_index.index = 0;
	insert_index.mode = KLIM_END;
	u32 child_page = left_node->page_number;
	btree_node_write_ex(
		parent_node,
		pager,
		&insert_index,
		3,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);

	child_page = l1_node->page_number;
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		1,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	child_page = l2_node->page_number;
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		2,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	left_node->header->right_child = l3_node->page_number;

	btree_node_write_ex(
		l1_node, pager, &insert_index, 1, 0, A, sizeof(A), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l2_node, pager, &insert_index, 2, 0, B, sizeof(B), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l3_node, pager, &insert_index, 3, 0, C, sizeof(C), WRITER_EX_MODE_RAW);

	child_page = l4_node->page_number;
	btree_node_write_ex(
		right_node,
		pager,
		&insert_index,
		4,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	right_node->header->right_child = l5_node->page_number;
	btree_node_write_ex(
		l4_node, pager, &insert_index, 4, 0, D, sizeof(D), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l5_node, pager, &insert_index, 5, 0, E, sizeof(E), WRITER_EX_MODE_RAW);
	char found;
	struct Cursor* cursor = cursor_create(tree);
	// Traverse to leaf, then pop to parent to get to nonleaf.
	cursor_traverse_to(cursor, 6, &found);
	cursor_pop(cursor, NULL);

	bta_rotate(cursor, BTA_REBALANCE_MODE_ROTATE_RIGHT);
	cursor_destroy(cursor);

	u32 tests[] = {1, 2, 3, 4, 5};
	char* vals[] = {A, B, C, D, E};
	char buf[100] = {0};
	for( int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++ )
	{
		u32 test_key = tests[i];
		char* testi = vals[i];
		memset(buf, 0x00, sizeof(buf));
		cursor = cursor_create(tree);
		cursor_traverse_to(cursor, test_key, &found);

		if( !found )
			goto fail;

		btree_node_init_from_read(
			&test_node, test_page, pager, cursor->current_page_id);
		cursor_destroy(cursor);

		btree_node_read(&test_node, tree->pager, test_key, buf, sizeof(buf));

		if( memcmp(buf, testi, strlen(testi) + 1) != 0 )
			goto fail;
	}

	/**
	 * @brief ROTATE BACK
	 *
	 */

	cursor = cursor_create(tree);
	// Traverse to leaf, then pop to parent to get to nonleaf.
	cursor_traverse_to(cursor, 1, &found);
	cursor_pop(cursor, NULL);

	bta_rotate(cursor, BTA_REBALANCE_MODE_ROTATE_LEFT);
	cursor_destroy(cursor);

	for( int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++ )
	{
		u32 test_key = tests[i];
		char* testi = vals[i];
		memset(buf, 0x00, sizeof(buf));
		cursor = cursor_create(tree);
		cursor_traverse_to(cursor, test_key, &found);

		if( !found )
			goto fail;

		btree_node_init_from_read(
			&test_node, test_page, pager, cursor->current_page_id);
		cursor_destroy(cursor);

		btree_node_read(&test_node, tree->pager, test_key, buf, sizeof(buf));

		if( memcmp(buf, testi, strlen(testi) + 1) != 0 )
			goto fail;
	}

end:
	if( test_page )
		page_destroy(pager, test_page);
	remove(db_name);

	return result;
fail:
	result = 0;
	goto end;
}

int
btree_alg_rotate_leaf_test(void)
{
	char const* db_name = "bt_alg_rote_leaf_t.db";
	remove(db_name);

	int result = 1;
	struct BTreeNode test_node = {0};
	struct BTreeNode* parent_node = NULL;
	struct BTreeNode* left_node = NULL;
	struct BTreeNode* right_node = NULL;
	struct BTreeNode* l1_node = NULL;
	struct BTreeNode* l2_node = NULL;
	struct BTreeNode* l3_node = NULL;
	struct BTreeNode* l4_node = NULL;
	struct BTreeNode* l5_node = NULL;
	struct Page* parent_page = NULL;
	struct Page* left_page = NULL;
	struct Page* right_page = NULL;
	struct Page* l1_page = NULL;
	struct Page* l2_page = NULL;
	struct Page* l3_page = NULL;
	struct Page* l4_page = NULL;
	struct Page* l5_page = NULL;
	struct Page* test_page = NULL;

	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	page_cache_create(&cache, 11);
	pager_cstd_create(&pager, cache, db_name, 0x1000);
	struct BTreeNodeRC rcer;
	noderc_init(&rcer, pager);
	struct BTree* tree;
	btree_alloc(&tree);
	if( btree_init(tree, pager, &rcer, 2) != BTREE_OK )
	{
		result = 0;
		goto end;
	}

	page_create(pager, &test_page);
	page_create(pager, &parent_page);
	page_create(pager, &left_page);
	page_create(pager, &right_page);
	page_create(pager, &l1_page);
	page_create(pager, &l2_page);
	page_create(pager, &l3_page);
	page_create(pager, &l4_page);
	page_create(pager, &l5_page);

	btree_node_create_as_page_number(&parent_node, 2, parent_page);
	btree_node_create_as_page_number(&left_node, 3, left_page);
	btree_node_create_as_page_number(&right_node, 4, right_page);
	btree_node_create_as_page_number(&l1_node, 5, l1_page);
	btree_node_create_as_page_number(&l2_node, 6, l2_page);
	btree_node_create_as_page_number(&l3_node, 7, l3_page);
	btree_node_create_as_page_number(&l4_node, 8, l4_page);
	btree_node_create_as_page_number(&l5_node, 9, l5_page);

	// This test is an impossibility, but it covers what we want.
	// We've given the cells children but also label them as leaves.
	l1_node->header->is_leaf = 1;
	l2_node->header->is_leaf = 1;
	l3_node->header->is_leaf = 1;
	l4_node->header->is_leaf = 1;
	l5_node->header->is_leaf = 1;

	u32 left_node_right_most_child = l3_node->page_number;
	u32 right_node_right_most_child = l5_node->page_number;
	left_node->header->right_child = left_node_right_most_child;
	right_node->header->right_child = right_node_right_most_child;

	parent_node->header->right_child = right_node->page_number;

	pager_write_page(pager, parent_page);
	pager_write_page(pager, left_page);
	pager_write_page(pager, right_page);
	char A[] = "A";
	char B[] = "B";
	char C[] = "C";
	char D[] = "D";
	char E[] = "E";
	char F[] = "F";

	//         3
	// 1,   2,   *  |     5, *
	// 1  | 2 |  3  |   4, 5 | 6
	struct InsertionIndex insert_index = {0};
	insert_index.index = 0;
	insert_index.mode = KLIM_END;
	u32 child_page = left_node->page_number;
	btree_node_write_ex(
		parent_node,
		pager,
		&insert_index,
		3,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);

	child_page = l1_node->page_number;
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		1,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	child_page = l2_node->page_number;
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		2,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	left_node->header->right_child = l3_node->page_number;

	btree_node_write_ex(
		l1_node, pager, &insert_index, 1, 0, A, sizeof(A), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l2_node, pager, &insert_index, 2, 0, B, sizeof(B), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l3_node, pager, &insert_index, 3, 0, C, sizeof(C), WRITER_EX_MODE_RAW);

	child_page = l4_node->page_number;
	btree_node_write_ex(
		right_node,
		pager,
		&insert_index,
		5,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	right_node->header->right_child = l5_node->page_number;
	btree_node_write_ex(
		l4_node, pager, &insert_index, 4, 0, D, sizeof(D), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l4_node, pager, &insert_index, 5, 0, E, sizeof(E), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l5_node, pager, &insert_index, 6, 0, F, sizeof(F), WRITER_EX_MODE_RAW);
	char found;
	struct Cursor* cursor = cursor_create(tree);
	// Traverse to leaf, then pop to parent to get to nonleaf.
	cursor_traverse_to(cursor, 6, &found);

	bta_rotate(cursor, BTA_REBALANCE_MODE_ROTATE_RIGHT);
	cursor_destroy(cursor);

	u32 tests[] = {1, 2, 3, 4, 5, 6};
	char* vals[] = {A, B, C, D, E, F};
	char buf[100] = {0};
	for( int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++ )
	{
		u32 test_key = tests[i];
		char* testi = vals[i];
		memset(buf, 0x00, sizeof(buf));
		cursor = cursor_create(tree);
		cursor_traverse_to(cursor, test_key, &found);

		if( !found )
			goto fail;

		btree_node_init_from_read(
			&test_node, test_page, pager, cursor->current_page_id);
		cursor_destroy(cursor);

		btree_node_read(&test_node, tree->pager, test_key, buf, sizeof(buf));

		if( memcmp(buf, testi, strlen(testi) + 1) != 0 )
			goto fail;
	}

	/**
	 * @brief ROTATE BACK
	 *
	 */

	cursor = cursor_create(tree);
	// Traverse to leaf, then pop to parent to get to nonleaf.
	cursor_traverse_to(cursor, 4, &found);

	bta_rotate(cursor, BTA_REBALANCE_MODE_ROTATE_LEFT);
	cursor_destroy(cursor);

	for( int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++ )
	{
		u32 test_key = tests[i];
		char* testi = vals[i];
		memset(buf, 0x00, sizeof(buf));
		cursor = cursor_create(tree);
		cursor_traverse_to(cursor, test_key, &found);

		if( !found )
			goto fail;

		btree_node_init_from_read(
			&test_node, test_page, pager, cursor->current_page_id);
		cursor_destroy(cursor);

		btree_node_read(&test_node, tree->pager, test_key, buf, sizeof(buf));

		if( memcmp(buf, testi, strlen(testi) + 1) != 0 )
			goto fail;
	}

end:
	if( test_page )
		page_destroy(pager, test_page);
	remove(db_name);

	return result;
fail:
	result = 0;
	goto end;
}

int
btree_alg_merge_test(void)
{
	char const* db_name = "bt_alg_merge_t.db";
	remove(db_name);

	int result = 1;
	struct BTreeNode test_node = {0};
	struct BTreeNode* parent_node = NULL;
	struct BTreeNode* left_node = NULL;
	struct BTreeNode* right_node = NULL;
	struct BTreeNode* l1_node = NULL;
	struct BTreeNode* l2_node = NULL;
	struct BTreeNode* l3_node = NULL;
	struct BTreeNode* l4_node = NULL;
	struct BTreeNode* l5_node = NULL;
	struct BTreeNode* l6_node = NULL;
	struct Page* parent_page = NULL;
	struct Page* left_page = NULL;
	struct Page* right_page = NULL;
	struct Page* l1_page = NULL;
	struct Page* l2_page = NULL;
	struct Page* l3_page = NULL;
	struct Page* l4_page = NULL;
	struct Page* l5_page = NULL;
	struct Page* l6_page = NULL;
	struct Page* test_page = NULL;

	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	page_cache_create(&cache, 11);
	pager_cstd_create(&pager, cache, db_name, 0x1000);
	struct BTreeNodeRC rcer;
	noderc_init(&rcer, pager);
	struct BTree* tree;
	btree_alloc(&tree);
	if( btree_init(tree, pager, &rcer, 2) != BTREE_OK )
	{
		result = 0;
		goto end;
	}

	page_create(pager, &test_page);
	page_create(pager, &parent_page);
	page_create(pager, &left_page);
	page_create(pager, &right_page);
	page_create(pager, &l1_page);
	page_create(pager, &l2_page);
	page_create(pager, &l3_page);
	page_create(pager, &l4_page);
	page_create(pager, &l5_page);
	page_create(pager, &l6_page);

	btree_node_create_as_page_number(&parent_node, 2, parent_page);
	btree_node_create_as_page_number(&left_node, 3, left_page);
	btree_node_create_as_page_number(&right_node, 4, right_page);
	btree_node_create_as_page_number(&l1_node, 5, l1_page);
	btree_node_create_as_page_number(&l2_node, 6, l2_page);
	btree_node_create_as_page_number(&l3_node, 7, l3_page);
	btree_node_create_as_page_number(&l4_node, 8, l4_page);
	btree_node_create_as_page_number(&l5_node, 9, l5_page);
	btree_node_create_as_page_number(&l6_node, 10, l6_page);

	// This test is an impossibility, but it covers what we want.
	// We've given the cells children but also label them as leaves.
	l1_node->header->is_leaf = 1;
	l2_node->header->is_leaf = 1;
	l3_node->header->is_leaf = 1;
	l4_node->header->is_leaf = 1;
	l5_node->header->is_leaf = 1;
	l6_node->header->is_leaf = 1;

	u32 left_node_right_most_child = l3_node->page_number;
	u32 right_node_right_most_child = l5_node->page_number;
	left_node->header->right_child = left_node_right_most_child;
	right_node->header->right_child = right_node_right_most_child;

	parent_node->header->right_child = right_node->page_number;

	pager_write_page(pager, parent_page);
	pager_write_page(pager, left_page);
	pager_write_page(pager, right_page);
	char A[] = "A";
	char B[] = "B";
	char C[] = "C";
	char D[] = "D";
	char E[] = "E";
	char F[] = "F";
	char G[] = "G";

	//         3
	// 1,   2,   *  |     5, 6, *
	// 1  | 2 |  3  |   4, 5 | 6 | 7
	struct InsertionIndex insert_index = {0};
	insert_index.index = 0;
	insert_index.mode = KLIM_END;
	u32 child_page = left_node->page_number;
	btree_node_write_ex(
		parent_node,
		pager,
		&insert_index,
		3,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);

	child_page = l1_node->page_number;
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		1,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	child_page = l2_node->page_number;
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		2,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	left_node->header->right_child = l3_node->page_number;

	btree_node_write_ex(
		l1_node, pager, &insert_index, 1, 0, A, sizeof(A), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l2_node, pager, &insert_index, 2, 0, B, sizeof(B), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l3_node, pager, &insert_index, 3, 0, C, sizeof(C), WRITER_EX_MODE_RAW);

	child_page = l4_node->page_number;
	btree_node_write_ex(
		right_node,
		pager,
		&insert_index,
		5,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	child_page = l5_node->page_number;
	btree_node_write_ex(
		right_node,
		pager,
		&insert_index,
		6,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	right_node->header->right_child = l6_node->page_number;
	pager_write_page(pager, left_page);
	pager_write_page(pager, right_page);

	btree_node_write_ex(
		l4_node, pager, &insert_index, 4, 0, D, sizeof(D), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l4_node, pager, &insert_index, 5, 0, E, sizeof(E), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l5_node, pager, &insert_index, 6, 0, F, sizeof(F), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l6_node, pager, &insert_index, 7, 0, G, sizeof(G), WRITER_EX_MODE_RAW);
	char found;
	struct Cursor* cursor = cursor_create(tree);
	// Traverse to leaf, then pop to parent to get to nonleaf.
	cursor_traverse_to(cursor, 6, &found);

	bta_merge(cursor, BTA_REBALANCE_MODE_MERGE_RIGHT);
	cursor_destroy(cursor);

	u32 tests[] = {1, 2, 3, 4, 5, 6, 7};
	char* vals[] = {A, B, C, D, E, F, G};
	char buf[100] = {0};
	for( int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++ )
	{
		u32 test_key = tests[i];
		char* testi = vals[i];
		memset(buf, 0x00, sizeof(buf));
		cursor = cursor_create(tree);
		cursor_traverse_to(cursor, test_key, &found);

		if( !found )
			goto fail;

		btree_node_init_from_read(
			&test_node, test_page, pager, cursor->current_page_id);
		cursor_destroy(cursor);

		btree_node_read(&test_node, tree->pager, test_key, buf, sizeof(buf));

		if( memcmp(buf, testi, strlen(testi) + 1) != 0 )
			goto fail;
	}

	/**
	 * @brief ROTATE BACK
	 *
	 */

	cursor = cursor_create(tree);
	// Traverse to leaf, then pop to parent to get to nonleaf.
	cursor_traverse_to(cursor, 2, &found);

	bta_merge(cursor, BTA_REBALANCE_MODE_MERGE_LEFT);
	cursor_destroy(cursor);

	for( int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++ )
	{
		u32 test_key = tests[i];
		char* testi = vals[i];
		memset(buf, 0x00, sizeof(buf));
		cursor = cursor_create(tree);
		cursor_traverse_to(cursor, test_key, &found);

		if( !found )
			goto fail;

		btree_node_init_from_read(
			&test_node, test_page, pager, cursor->current_page_id);
		cursor_destroy(cursor);

		btree_node_read(&test_node, tree->pager, test_key, buf, sizeof(buf));

		if( memcmp(buf, testi, strlen(testi) + 1) != 0 )
			goto fail;
	}

end:
	if( test_page )
		page_destroy(pager, test_page);
	remove(db_name);

	return result;
fail:
	result = 0;
	goto end;
}

int
btree_alg_merge_nonleaf_test(void)
{
	char const* db_name = "bt_alg_merge_nl_t.db";
	remove(db_name);

	int result = 1;
	struct BTreeNode test_node = {0};
	struct BTreeNode* parent_node = NULL;
	struct BTreeNode* left_node = NULL;
	struct BTreeNode* middle_node = NULL;
	struct BTreeNode* right_node = NULL;
	struct BTreeNode* l1_node = NULL;
	struct BTreeNode* l2_node = NULL;
	struct BTreeNode* l3_node = NULL;
	struct BTreeNode* l4_node = NULL;
	struct BTreeNode* l5_node = NULL;
	struct BTreeNode* l6_node = NULL;
	struct BTreeNode* l7_node = NULL;
	struct Page* parent_page = NULL;
	struct Page* left_page = NULL;
	struct Page* middle_page = NULL;
	struct Page* right_page = NULL;
	struct Page* l1_page = NULL;
	struct Page* l2_page = NULL;
	struct Page* l3_page = NULL;
	struct Page* l4_page = NULL;
	struct Page* l5_page = NULL;
	struct Page* l6_page = NULL;
	struct Page* l7_page = NULL;
	struct Page* test_page = NULL;

	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	page_cache_create(&cache, 11);
	pager_cstd_create(&pager, cache, db_name, 0x1000);
	struct BTreeNodeRC rcer;
	noderc_init(&rcer, pager);
	struct BTree* tree;
	btree_alloc(&tree);
	if( btree_init(tree, pager, &rcer, 2) != BTREE_OK )
	{
		result = 0;
		goto end;
	}

	page_create(pager, &test_page);
	page_create(pager, &parent_page);
	page_create(pager, &middle_page);
	page_create(pager, &left_page);
	page_create(pager, &right_page);
	page_create(pager, &l1_page);
	page_create(pager, &l2_page);
	page_create(pager, &l3_page);
	page_create(pager, &l4_page);
	page_create(pager, &l5_page);
	page_create(pager, &l6_page);
	page_create(pager, &l7_page);

	btree_node_create_as_page_number(&parent_node, 2, parent_page);
	btree_node_create_as_page_number(&left_node, 3, left_page);
	btree_node_create_as_page_number(&middle_node, 4, middle_page);
	btree_node_create_as_page_number(&right_node, 5, right_page);
	btree_node_create_as_page_number(&l1_node, 6, l1_page);
	btree_node_create_as_page_number(&l2_node, 7, l2_page);
	btree_node_create_as_page_number(&l3_node, 8, l3_page);
	btree_node_create_as_page_number(&l4_node, 9, l4_page);
	btree_node_create_as_page_number(&l5_node, 10, l5_page);
	btree_node_create_as_page_number(&l6_node, 11, l6_page);
	btree_node_create_as_page_number(&l7_node, 12, l7_page);

	// This test is an impossibility, but it covers what we want.
	// We've given the cells children but also label them as leaves.
	l1_node->header->is_leaf = 1;
	l2_node->header->is_leaf = 1;
	l3_node->header->is_leaf = 1;
	l4_node->header->is_leaf = 1;
	l5_node->header->is_leaf = 1;
	l6_node->header->is_leaf = 1;
	l7_node->header->is_leaf = 1;

	u32 left_node_right_most_child = l3_node->page_number;
	u32 right_node_right_most_child = l5_node->page_number;
	left_node->header->right_child = left_node_right_most_child;
	right_node->header->right_child = right_node_right_most_child;

	parent_node->header->right_child = right_node->page_number;

	pager_write_page(pager, parent_page);
	pager_write_page(pager, left_page);
	pager_write_page(pager, right_page);
	char A[] = "A";
	char B[] = "B";
	char C[] = "C";
	char D[] = "D";
	char E[] = "E";
	char F[] = "F";
	char G[] = "G";

	//         3,  5, *
	// 1,   2,   *  |     4, *   |   6, *
	// 1  | 2 |  3  |   4 | 5    |  6 | 7
	struct InsertionIndex insert_index = {0};
	insert_index.index = 0;
	insert_index.mode = KLIM_END;
	u32 child_page = left_node->page_number;
	btree_node_write_ex(
		parent_node,
		pager,
		&insert_index,
		3,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	child_page = middle_node->page_number;
	btree_node_write_ex(
		parent_node,
		pager,
		&insert_index,
		5,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	parent_node->header->right_child = right_node->page_number;

	child_page = l1_node->page_number;
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		1,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	child_page = l2_node->page_number;
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		2,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	left_node->header->right_child = l3_node->page_number;

	btree_node_write_ex(
		l1_node, pager, &insert_index, 1, 0, A, sizeof(A), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l2_node, pager, &insert_index, 2, 0, B, sizeof(B), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l3_node, pager, &insert_index, 3, 0, C, sizeof(C), WRITER_EX_MODE_RAW);

	child_page = l4_node->page_number;
	btree_node_write_ex(
		middle_node,
		pager,
		&insert_index,
		4,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	middle_node->header->right_child = l5_node->page_number;
	btree_node_write_ex(
		l4_node, pager, &insert_index, 4, 0, D, sizeof(D), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l5_node, pager, &insert_index, 5, 0, E, sizeof(E), WRITER_EX_MODE_RAW);

	child_page = l6_node->page_number;
	btree_node_write_ex(
		right_node,
		pager,
		&insert_index,
		6,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	right_node->header->right_child = l7_node->page_number;

	btree_node_write_ex(
		l6_node, pager, &insert_index, 6, 0, F, sizeof(F), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l7_node, pager, &insert_index, 7, 0, G, sizeof(G), WRITER_EX_MODE_RAW);

	pager_write_page(pager, parent_page);
	pager_write_page(pager, left_page);
	pager_write_page(pager, middle_page);
	pager_write_page(pager, right_page);
	char found;
	struct Cursor* cursor = cursor_create(tree);
	// Traverse to leaf, then pop to parent to get to nonleaf.
	cursor_traverse_to(cursor, 7, &found);
	cursor_pop(cursor, NULL);

	bta_merge(cursor, BTA_REBALANCE_MODE_MERGE_LEFT);
	cursor_destroy(cursor);

	u32 tests[] = {1, 2, 3, 4, 5, 6, 7};
	char* vals[] = {A, B, C, D, E, F, G};
	char buf[100] = {0};
	for( int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++ )
	{
		u32 test_key = tests[i];
		char* testi = vals[i];
		memset(buf, 0x00, sizeof(buf));
		cursor = cursor_create(tree);
		cursor_traverse_to(cursor, test_key, &found);

		if( !found )
			goto fail;

		btree_node_init_from_read(
			&test_node, test_page, pager, cursor->current_page_id);
		cursor_destroy(cursor);

		btree_node_read(&test_node, tree->pager, test_key, buf, sizeof(buf));

		if( memcmp(buf, testi, strlen(testi) + 1) != 0 )
			goto fail;
	}

end:
	if( test_page )
		page_destroy(pager, test_page);
	remove(db_name);

	return result;
fail:
	result = 0;
	goto end;
}

int
btree_alg_merge_nonleaf_l_test(void)
{
	char const* db_name = "bt_alg_meg_nl_l_t.db";
	remove(db_name);

	int result = 1;
	struct BTreeNode test_node = {0};
	struct BTreeNode* parent_node = NULL;
	struct BTreeNode* left_node = NULL;
	struct BTreeNode* middle_node = NULL;
	struct BTreeNode* right_node = NULL;
	struct BTreeNode* l1_node = NULL;
	struct BTreeNode* l2_node = NULL;
	struct BTreeNode* l3_node = NULL;
	struct BTreeNode* l4_node = NULL;
	struct BTreeNode* l5_node = NULL;
	struct BTreeNode* l6_node = NULL;
	struct BTreeNode* l7_node = NULL;
	struct Page* parent_page = NULL;
	struct Page* left_page = NULL;
	struct Page* middle_page = NULL;
	struct Page* right_page = NULL;
	struct Page* l1_page = NULL;
	struct Page* l2_page = NULL;
	struct Page* l3_page = NULL;
	struct Page* l4_page = NULL;
	struct Page* l5_page = NULL;
	struct Page* l6_page = NULL;
	struct Page* l7_page = NULL;
	struct Page* test_page = NULL;

	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	page_cache_create(&cache, 11);
	pager_cstd_create(&pager, cache, db_name, 0x1000);
	struct BTreeNodeRC rcer;
	noderc_init(&rcer, pager);
	struct BTree* tree;
	btree_alloc(&tree);
	if( btree_init(tree, pager, &rcer, 2) != BTREE_OK )
	{
		result = 0;
		goto end;
	}

	page_create(pager, &test_page);
	page_create(pager, &parent_page);
	page_create(pager, &middle_page);
	page_create(pager, &left_page);
	page_create(pager, &right_page);
	page_create(pager, &l1_page);
	page_create(pager, &l2_page);
	page_create(pager, &l3_page);
	page_create(pager, &l4_page);
	page_create(pager, &l5_page);
	page_create(pager, &l6_page);
	page_create(pager, &l7_page);

	btree_node_create_as_page_number(&parent_node, 2, parent_page);
	btree_node_create_as_page_number(&left_node, 3, left_page);
	btree_node_create_as_page_number(&middle_node, 4, middle_page);
	btree_node_create_as_page_number(&right_node, 5, right_page);
	btree_node_create_as_page_number(&l1_node, 6, l1_page);
	btree_node_create_as_page_number(&l2_node, 7, l2_page);
	btree_node_create_as_page_number(&l3_node, 8, l3_page);
	btree_node_create_as_page_number(&l4_node, 9, l4_page);
	btree_node_create_as_page_number(&l5_node, 10, l5_page);
	btree_node_create_as_page_number(&l6_node, 11, l6_page);
	btree_node_create_as_page_number(&l7_node, 12, l7_page);

	// This test is an impossibility, but it covers what we want.
	// We've given the cells children but also label them as leaves.
	l1_node->header->is_leaf = 1;
	l2_node->header->is_leaf = 1;
	l3_node->header->is_leaf = 1;
	l4_node->header->is_leaf = 1;
	l5_node->header->is_leaf = 1;
	l6_node->header->is_leaf = 1;
	l7_node->header->is_leaf = 1;

	u32 left_node_right_most_child = l3_node->page_number;
	u32 right_node_right_most_child = l5_node->page_number;
	left_node->header->right_child = left_node_right_most_child;
	right_node->header->right_child = right_node_right_most_child;

	parent_node->header->right_child = right_node->page_number;

	pager_write_page(pager, parent_page);
	pager_write_page(pager, left_page);
	pager_write_page(pager, right_page);
	char A[] = "A";
	char B[] = "B";
	char C[] = "C";
	char D[] = "D";
	char E[] = "E";
	char F[] = "F";
	char G[] = "G";

	//         3,  5, *
	// 1,   2,   *  |     4, *   |   6, *
	// 1  | 2 |  3  |   4 | 5    |  6 | 7
	struct InsertionIndex insert_index = {0};
	insert_index.index = 0;
	insert_index.mode = KLIM_END;
	u32 child_page = left_node->page_number;
	btree_node_write_ex(
		parent_node,
		pager,
		&insert_index,
		3,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	child_page = middle_node->page_number;
	btree_node_write_ex(
		parent_node,
		pager,
		&insert_index,
		5,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	parent_node->header->right_child = right_node->page_number;

	child_page = l1_node->page_number;
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		1,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	child_page = l2_node->page_number;
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		2,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	left_node->header->right_child = l3_node->page_number;

	btree_node_write_ex(
		l1_node, pager, &insert_index, 1, 0, A, sizeof(A), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l2_node, pager, &insert_index, 2, 0, B, sizeof(B), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l3_node, pager, &insert_index, 3, 0, C, sizeof(C), WRITER_EX_MODE_RAW);

	child_page = l4_node->page_number;
	btree_node_write_ex(
		middle_node,
		pager,
		&insert_index,
		4,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	middle_node->header->right_child = l5_node->page_number;
	btree_node_write_ex(
		l4_node, pager, &insert_index, 4, 0, D, sizeof(D), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l5_node, pager, &insert_index, 5, 0, E, sizeof(E), WRITER_EX_MODE_RAW);

	child_page = l6_node->page_number;
	btree_node_write_ex(
		right_node,
		pager,
		&insert_index,
		6,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	right_node->header->right_child = l7_node->page_number;

	btree_node_write_ex(
		l6_node, pager, &insert_index, 6, 0, F, sizeof(F), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l7_node, pager, &insert_index, 7, 0, G, sizeof(G), WRITER_EX_MODE_RAW);

	pager_write_page(pager, parent_page);
	pager_write_page(pager, left_page);
	pager_write_page(pager, middle_page);
	pager_write_page(pager, right_page);
	char found;
	struct Cursor* cursor = cursor_create(tree);
	// Traverse to leaf, then pop to parent to get to nonleaf.
	cursor_traverse_to(cursor, 5, &found);
	cursor_pop(cursor, NULL);

	bta_merge(cursor, BTA_REBALANCE_MODE_MERGE_RIGHT);
	cursor_destroy(cursor);

	u32 tests[] = {1, 2, 3, 4, 5, 6, 7};
	char* vals[] = {A, B, C, D, E, F, G};
	char buf[100] = {0};
	for( int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++ )
	{
		u32 test_key = tests[i];
		char* testi = vals[i];
		memset(buf, 0x00, sizeof(buf));
		cursor = cursor_create(tree);
		cursor_traverse_to(cursor, test_key, &found);

		if( !found )
			goto fail;

		btree_node_init_from_read(
			&test_node, test_page, pager, cursor->current_page_id);
		cursor_destroy(cursor);

		btree_node_read(&test_node, tree->pager, test_key, buf, sizeof(buf));

		if( memcmp(buf, testi, strlen(testi) + 1) != 0 )
			goto fail;
	}

end:
	if( test_page )
		page_destroy(pager, test_page);
	remove(db_name);

	return result;
fail:
	result = 0;
	goto end;
}

int
btree_alg_rebalance(void)
{
	char const* db_name = "btree_alg_rebalance.db";
	remove(db_name);

	int result = 1;
	struct BTreeNode test_node = {0};
	struct BTreeNode* parent_node = NULL;
	struct BTreeNode* left_node = NULL;
	struct BTreeNode* middle_node = NULL;
	struct BTreeNode* right_node = NULL;
	struct BTreeNode* l1_node = NULL;
	struct BTreeNode* l2_node = NULL;
	struct BTreeNode* l3_node = NULL;
	struct BTreeNode* l4_node = NULL;
	struct BTreeNode* l5_node = NULL;
	struct BTreeNode* l6_node = NULL;
	struct BTreeNode* l7_node = NULL;
	struct Page* parent_page = NULL;
	struct Page* left_page = NULL;
	struct Page* middle_page = NULL;
	struct Page* right_page = NULL;
	struct Page* l1_page = NULL;
	struct Page* l2_page = NULL;
	struct Page* l3_page = NULL;
	struct Page* l4_page = NULL;
	struct Page* l5_page = NULL;
	struct Page* l6_page = NULL;
	struct Page* l7_page = NULL;
	struct Page* test_page = NULL;

	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	page_cache_create(&cache, 11);
	pager_cstd_create(&pager, cache, db_name, 0x1000);
	struct BTreeNodeRC rcer;
	noderc_init(&rcer, pager);
	struct BTree* tree;
	btree_alloc(&tree);
	if( btree_init(tree, pager, &rcer, 2) != BTREE_OK )
	{
		result = 0;
		goto end;
	}

	page_create(pager, &test_page);
	page_create(pager, &parent_page);
	page_create(pager, &middle_page);
	page_create(pager, &left_page);
	page_create(pager, &right_page);
	page_create(pager, &l1_page);
	page_create(pager, &l2_page);
	page_create(pager, &l3_page);
	page_create(pager, &l4_page);
	page_create(pager, &l5_page);
	page_create(pager, &l6_page);
	page_create(pager, &l7_page);

	btree_node_create_as_page_number(&parent_node, 2, parent_page);
	btree_node_create_as_page_number(&left_node, 3, left_page);
	btree_node_create_as_page_number(&middle_node, 4, middle_page);
	btree_node_create_as_page_number(&right_node, 5, right_page);
	btree_node_create_as_page_number(&l1_node, 6, l1_page);
	btree_node_create_as_page_number(&l2_node, 7, l2_page);
	btree_node_create_as_page_number(&l3_node, 8, l3_page);
	btree_node_create_as_page_number(&l4_node, 9, l4_page);
	btree_node_create_as_page_number(&l5_node, 10, l5_page);
	btree_node_create_as_page_number(&l6_node, 11, l6_page);
	btree_node_create_as_page_number(&l7_node, 12, l7_page);

	// This test is an impossibility, but it covers what we want.
	// We've given the cells children but also label them as leaves.
	l1_node->header->is_leaf = 1;
	l2_node->header->is_leaf = 1;
	l3_node->header->is_leaf = 1;
	l4_node->header->is_leaf = 1;
	l5_node->header->is_leaf = 1;
	l6_node->header->is_leaf = 1;
	l7_node->header->is_leaf = 1;

	u32 left_node_right_most_child = l3_node->page_number;
	u32 right_node_right_most_child = l5_node->page_number;
	left_node->header->right_child = left_node_right_most_child;
	right_node->header->right_child = right_node_right_most_child;

	parent_node->header->right_child = right_node->page_number;

	pager_write_page(pager, parent_page);
	pager_write_page(pager, left_page);
	pager_write_page(pager, right_page);
	char A[] = "A";
	char B[] = "B";
	char C[] = "C";
	char D[] = "D";
	char E[] = "E";
	char F[] = "F";
	char G[] = "G";

	//         3,  5, *
	// 1,   2,   *  |     4, *   |   6, *
	// 1  | 2 |  3  |   4 | 5    |  6 | 7
	struct InsertionIndex insert_index = {0};
	insert_index.index = 0;
	insert_index.mode = KLIM_END;
	u32 child_page = left_node->page_number;
	btree_node_write_ex(
		parent_node,
		pager,
		&insert_index,
		3,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	child_page = middle_node->page_number;
	btree_node_write_ex(
		parent_node,
		pager,
		&insert_index,
		5,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	parent_node->header->right_child = right_node->page_number;

	child_page = l1_node->page_number;
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		1,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	child_page = l2_node->page_number;
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		2,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	left_node->header->right_child = l3_node->page_number;

	btree_node_write_ex(
		l1_node, pager, &insert_index, 1, 0, A, sizeof(A), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l2_node, pager, &insert_index, 2, 0, B, sizeof(B), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l3_node, pager, &insert_index, 3, 0, C, sizeof(C), WRITER_EX_MODE_RAW);

	child_page = l4_node->page_number;
	btree_node_write_ex(
		middle_node,
		pager,
		&insert_index,
		4,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	middle_node->header->right_child = l5_node->page_number;
	btree_node_write_ex(
		l4_node, pager, &insert_index, 4, 0, D, sizeof(D), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l5_node, pager, &insert_index, 5, 0, E, sizeof(E), WRITER_EX_MODE_RAW);

	child_page = l6_node->page_number;
	btree_node_write_ex(
		right_node,
		pager,
		&insert_index,
		6,
		0,
		&child_page,
		sizeof(child_page),
		WRITER_EX_MODE_RAW);
	right_node->header->right_child = l7_node->page_number;

	btree_node_write_ex(
		l6_node, pager, &insert_index, 6, 0, F, sizeof(F), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l7_node, pager, &insert_index, 7, 0, G, sizeof(G), WRITER_EX_MODE_RAW);

	pager_write_page(pager, parent_page);
	pager_write_page(pager, left_page);
	pager_write_page(pager, middle_page);
	pager_write_page(pager, right_page);

	btree_delete(tree, 7);
	char found;
	struct Cursor* cursor = cursor_create(tree);
	u32 tests[] = {
		1,
		2,
		3,
		4,
		5,
		6,
	};
	char* vals[] = {A, B, C, D, E, F};
	char buf[100] = {0};
	for( int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++ )
	{
		u32 test_key = tests[i];
		char* testi = vals[i];
		memset(buf, 0x00, sizeof(buf));
		cursor = cursor_create(tree);
		cursor_traverse_to(cursor, test_key, &found);

		if( !found )
			goto fail;

		btree_node_init_from_read(
			&test_node, test_page, pager, cursor->current_page_id);
		cursor_destroy(cursor);

		btree_node_read(&test_node, tree->pager, test_key, buf, sizeof(buf));

		if( memcmp(buf, testi, strlen(testi) + 1) != 0 )
			goto fail;
	}

end:
	if( test_page )
		page_destroy(pager, test_page);
	remove(db_name);

	return result;
fail:
	result = 0;
	goto end;
}