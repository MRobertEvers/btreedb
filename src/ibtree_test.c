#include "ibtree_test.h"

#include "btree.h"
#include "btree_cursor.h"
#include "btree_defs.h"
#include "btree_node.h"
#include "btree_node_debug.h"
#include "btree_node_reader.h"
#include "btree_node_writer.h"
#include "ibtree.h"
#include "ibtree_alg.h"
#include "noderc.h"
#include "pager.h"
#include "pager_ops_cstd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int
ibtree_test_insert_shallow(void)
{
	char const* db_name = "ibtree_insert_shallow.db";
	int result = 1;
	struct BTreeNode* node = 0;
	struct PageSelector selector;
	struct Pager* pager;
	struct PageCache* cache = NULL;
	struct InsertionIndex index = {0};
	struct Page* test_page = NULL;
	struct BTreeNode* test_node = NULL;

	remove(db_name);
	page_cache_create(&cache, 3);
	// 1 byte of payload can fit on the first page.
	u32 page_size = btree_min_page_size() + 1 * 4;
	pager_cstd_create(&pager, cache, db_name, page_size);
	struct BTreeNodeRC rcer;
	noderc_init(&rcer, pager);
	struct BTree* tree;
	btree_alloc(&tree);
	enum btree_e btresult = ibtree_init(
		tree, pager, &rcer, 1, &ibtree_compare, &ibtree_compare_reset);
	if( btresult != BTREE_OK )
	{
		result = 0;
		goto end;
	}

	char flurb[] = "BLURING BLARGER BOLD";
	ibtree_insert(tree, flurb, sizeof(flurb));

	char churb[] = "BMRBING BLARGER BOLD";
	ibtree_insert(tree, churb, sizeof(churb));

	page_create(pager, &test_page);
	pager_reselect(&selector, 1);
	pager_read_page(pager, &selector, test_page);
	btree_node_create_from_page(&test_node, test_page);

	char alpha_buf[sizeof(flurb)] = {0};

	btree_node_read_ex(
		tree, test_node, flurb, sizeof(flurb), alpha_buf, sizeof(alpha_buf));

	if( memcmp(flurb, alpha_buf, sizeof(flurb)) != 0 )
	{
		result = 0;
		goto end;
	}

end:
	if( test_page )
		page_destroy(pager, test_page);
	if( test_node )
		btree_node_destroy(test_node);

	if( tree )
	{
		btree_deinit(tree);
		btree_dealloc(tree);
	}

	remove(db_name);

	return result;
}

int
ibtree_test_insert_split_root(void)
{
	char const* db_name = "ibtree_insert_spl_root.db";
	int result = 1;
	struct BTreeNode* node = 0;
	struct PageSelector selector;
	struct Pager* pager;
	struct PageCache* cache = NULL;
	struct InsertionIndex index = {0};
	struct Page* test_page = NULL;
	struct BTreeNode* test_node = NULL;
	struct Cursor* cursor = NULL;

	remove(db_name);
	page_cache_create(&cache, 3);
	// 1 byte of payload can fit on the first page.
	u32 page_size = btree_min_page_size() + 6 * 4;
	pager_cstd_create(&pager, cache, db_name, page_size);
	struct BTreeNodeRC rcer;
	noderc_init(&rcer, pager);
	struct BTree* tree;
	btree_alloc(&tree);
	enum btree_e btresult = ibtree_init(
		tree, pager, &rcer, 1, &ibtree_compare, &ibtree_compare_reset);
	if( btresult != BTREE_OK )
	{
		result = 0;
		goto end;
	}

	char flurb[] = "BLURING BLARGER BOLD";
	ibtree_insert(tree, flurb, sizeof(flurb));

	char churb[] = "BMRBING BLARGER BOLD";
	ibtree_insert(tree, churb, sizeof(churb));

	char murb[] = "CMRBING BLARGER BOLD";
	ibtree_insert(tree, murb, sizeof(murb));

	char durb[] = "DMRBING BLARGER BOLD";
	ibtree_insert(tree, durb, sizeof(durb));

	char splurb[] = "SPLURB DMRBING BLARGER BOLD";
	ibtree_insert(tree, splurb, sizeof(splurb));

	page_create(pager, &test_page);
	pager_reselect(&selector, 1);
	pager_read_page(pager, &selector, test_page);
	btree_node_create_from_page(&test_node, test_page);

	if( test_node->header->right_child == 0 )
	{
		result = 0;
		goto end;
	}
	btree_node_destroy(test_node);
	test_node = NULL;

	// Always check that 1 is still reachable.
	char found;
	cursor = cursor_create(tree);
	cursor_traverse_to_ex(cursor, splurb, sizeof(splurb), &found);
	if( !found )
		goto fail;

	pager_reselect(&selector, cursor->current_page_id);
	pager_read_page(pager, &selector, test_page);
	btree_node_create_from_page(&test_node, test_page);

	if( test_node->header->num_keys == 0 )
		goto fail;

	char alpha_buf[sizeof(splurb)] = {0};

	btree_node_read_ex(
		tree, test_node, splurb, sizeof(splurb), alpha_buf, sizeof(alpha_buf));

	if( memcmp(splurb, alpha_buf, sizeof(splurb)) != 0 )
		goto fail;

end:
	if( test_page )
		page_destroy(pager, test_page);
	if( test_node )
		btree_node_destroy(test_node);

	if( cursor )
		cursor_destroy(cursor);

	if( tree )
	{
		btree_deinit(tree);
		btree_dealloc(tree);
	}

	return result;

fail:
	result = 0;
	goto end;
}

struct TestRecord
{
	unsigned int key;
	void* data;
	unsigned int size;
};

int
ibtree_test_deep_tree(void)
{
	char const* db_name = "btree_test_deep_tree.db";
	int result = 1;
	struct BTreeNode* node = NULL;
	struct PageSelector selector;
	struct CellData cell;
	struct Page* page = NULL;
	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	remove(db_name);

	page_cache_create(&cache, 11);
	u32 page_size = btree_min_page_size() + 1 * 4;
	pager_cstd_create(&pager, cache, db_name, page_size);
	struct BTreeNodeRC rcer;
	noderc_init(&rcer, pager);
	struct BTree* tree;
	btree_alloc(&tree);
	enum btree_e btresult = ibtree_init(
		tree, pager, &rcer, 1, &ibtree_compare, &ibtree_compare_reset);
	if( btresult != BTREE_OK )
	{
		result = 0;
		goto end;
	}

	char billy[] = "billy_"
				   "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVW"
				   "XYZ";
	char ruth[40] = "ruth";
	char yuta[100] = "999yutaaa";
	char charlie[40] = "charlie";
	char buxley[40] = "buxley";
	char herman[40] = "herman_abcdefghijklmnopqrstuvwxyz";
	char flaur[100] =
		"flaur_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char flemming[100] =
		"flemming_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char xiao[100] = "xiao";
	char gilly[100] = "gilly";
	char brock[100] = "brock";
	char sloan[100] = "sloan";
	char ryan[100] = "ryan";
	char jacob[100] = "jacob";
	char megan[100] = "megan";
	char donata[100] = "donata";
	char honda[100] = "honda";
	char xondley[100] = "xondley";
	char perkins[100] = "perkins";
	char rick[100] = "rick";
	char woally[100] = "woally";
	char hoosin[100] = "hoosin";
	char sybian[100] = "sybian";
	char trillian[100] = "trillian";
	char coxes[100] = "coxes";

	struct TestRecord records[] = {
		{.key = 1, .data = billy, .size = sizeof(billy)},		 // 1
		{.key = 2, .data = ruth, .size = sizeof(ruth)},			 // 2
		{.key = 3, .data = yuta, .size = sizeof(yuta)},			 // 3
		{.key = 8, .data = charlie, .size = sizeof(charlie)},	 // 4
		{.key = 5, .data = buxley, .size = sizeof(buxley)},		 // 5
		{.key = 9, .data = herman, .size = sizeof(herman)},		 // 6
		{.key = 25, .data = flaur, .size = sizeof(flaur)},		 // 7
		{.key = 23, .data = flemming, .size = sizeof(flemming)}, // 8
		{.key = 4, .data = xiao, .size = sizeof(xiao)},			 // 9
		{.key = 13, .data = gilly, .size = sizeof(billy)},		 // 10
		{.key = 40, .data = brock, .size = sizeof(brock)},		 // 11
		{.key = 41, .data = sloan, .size = sizeof(sloan)},		 // 12
		{.key = 42, .data = ryan, .size = sizeof(ryan)},		 // 13
		{.key = 35, .data = jacob, .size = sizeof(jacob)},		 // 14
		{.key = 32, .data = megan, .size = sizeof(megan)},		 // 15
		{.key = 67, .data = donata, .size = sizeof(donata)},	 // 16
		{.key = 22, .data = honda, .size = sizeof(honda)},		 // 17
		{.key = 21, .data = xondley, .size = sizeof(xondley)},	 // 18
		{.key = 19, .data = perkins, .size = sizeof(perkins)},	 // 19
		{.key = 10, .data = rick, .size = sizeof(rick)},		 // 20
		{.key = 11, .data = woally, .size = sizeof(woally)},	 // 21
		{.key = 12, .data = hoosin, .size = sizeof(hoosin)},	 // 22
		{.key = 17, .data = sybian, .size = sizeof(sybian)},	 // 23
		{.key = 16, .data = trillian, .size = sizeof(trillian)}, // 24
		{.key = 55, .data = coxes, .size = sizeof(coxes)},		 // 25
	};

	/**
	 * Ensure that we can still reach certain keys
	 * after inserting others.
	 */
	struct Cursor* cursor = NULL;
	char found = 0;
	for( int i = 0; i < sizeof(records) / sizeof(records[0]); i++ )
	{
		struct TestRecord* test = &records[i];
		ibtree_insert(tree, test->data, test->size);

		// Always check that 1 is still reachable.
		cursor = cursor_create(tree);
		cursor_traverse_to_ex(cursor, billy, sizeof(billy), &found);
		if( !found )
			goto fail;
		cursor_destroy(cursor);

		// Check that the insertion is reachable.
		cursor = cursor_create(tree);
		cursor_traverse_to_ex(cursor, test->data, test->size, &found);
		if( !found )
			goto fail;
		cursor_destroy(cursor);
	}

	/**
	 * @brief Test reading the cell returns the correct data.
	 */
	cursor = cursor_create(tree);
	cursor_traverse_to_ex(cursor, hoosin, sizeof(hoosin), &found);
	if( !found )
		goto fail;

	page_create(tree->pager, &page);
	pager_reselect(&selector, cursor->current_page_id);
	pager_read_page(tree->pager, &selector, page);
	btree_node_create_from_page(&node, page);

	char buf[sizeof(hoosin)] = {0};
	btree_node_read_ex(tree, node, hoosin, sizeof(hoosin), buf, sizeof(buf));

	if( memcmp(hoosin, buf, sizeof(buf)) != 0 )
		goto fail;
end:
	if( page )
		page_destroy(pager, page);
	if( node )
		btree_node_destroy(node);
	remove(db_name);

	return result;
fail:
	result = 0;
	goto end;
}

int
ibta_rotate_test(void)
{
	char const* db_name = "ibtree_rotate_test.db";
	remove(db_name);

	int result = 1;
	struct BTreeNode* parent_node = NULL;
	struct BTreeNode* left_node = NULL;
	struct BTreeNode* right_node = NULL;
	struct Page* parent_page = NULL;
	struct Page* left_page = NULL;
	struct Page* right_page = NULL;
	struct Page* test_page = NULL;

	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	page_cache_create(&cache, 11);
	pager_cstd_create(&pager, cache, db_name, 0x1000);
	struct BTreeNodeRC rcer;
	noderc_init(&rcer, pager);
	struct BTree* tree;
	btree_alloc(&tree);
	if( ibtree_init(
			tree, pager, &rcer, 1, &ibtree_compare, &ibtree_compare_reset) !=
		BTREE_OK )
	{
		result = 0;
		goto end;
	}

	page_create(pager, &parent_page);
	page_create(pager, &left_page);
	page_create(pager, &right_page);

	btree_node_create_as_page_number(&parent_node, 1, parent_page);
	btree_node_create_as_page_number(&left_node, 2, left_page);
	btree_node_create_as_page_number(&right_node, 3, right_page);

	// This test is an impossibility, but it covers what we want.
	// We've given the cells children but also label them as leaves.
	right_node->header->is_leaf = 1;
	left_node->header->is_leaf = 1;

	u32 left_node_right_most_child = 88;
	left_node->header->right_child = left_node_right_most_child;
	pager_write_page(pager, parent_page);
	pager_write_page(pager, left_page);
	pager_write_page(pager, right_page);

	parent_node->header->right_child = 3;
	struct InsertionIndex insert_index = {0};
	insert_index.index = 0;
	insert_index.mode = KLIM_END;
	char rightmost[] = "xbilly";
	btree_node_write_ex(
		parent_node,
		pager,
		&insert_index,
		2,
		0,
		rightmost,
		sizeof(rightmost),
		WRITER_EX_MODE_RAW);

	char leftmost[] = "lkrilly";
	u32 leftmost_child_page = 13;
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		leftmost_child_page,
		0,
		leftmost,
		sizeof(leftmost),
		WRITER_EX_MODE_RAW);
	char middlemost[] = "mobly";
	u32 middlemost_child_page = 13;
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		middlemost_child_page,
		0,
		middlemost,
		sizeof(middlemost),
		WRITER_EX_MODE_RAW);

	char deadend[] = "zbilly";
	char found;
	struct Cursor* cursor = cursor_create(tree);
	cursor_traverse_to_ex(cursor, deadend, sizeof(deadend), &found);

	struct CursorBreadcrumb crumb = {0};

	ibta_rotate(cursor, BTA_REBALANCE_MODE_ROTATE_RIGHT);
	cursor_destroy(cursor);

	btree_node_init_from_read(
		right_node, right_node->page, pager, right_node->page_number);

	if( right_node->header->num_keys != 1 )
		goto fail;
	if( right_node->keys[0].key != left_node_right_most_child )
		goto fail;

	btree_node_init_from_read(
		left_node, left_node->page, pager, left_node->page_number);

	if( left_node->header->num_keys != 1 )
		goto fail;
	if( left_node->header->right_child != middlemost_child_page )
		goto fail;

	char abuf[sizeof(leftmost)] = {0};
	btree_node_read_ex(
		tree, left_node, leftmost, sizeof(leftmost), abuf, sizeof(abuf));

	if( memcmp(leftmost, abuf, sizeof(abuf)) != 0 )
		goto fail;

	cursor = cursor_create(tree);
	cursor_traverse_to_ex(cursor, rightmost, sizeof(rightmost), &found);

	if( !found || cursor->current_page_id != 3 )
		goto fail;
	cursor_destroy(cursor);

	char buf[sizeof(rightmost)] = {0};
	btree_node_read_ex(
		tree, right_node, rightmost, sizeof(rightmost), buf, sizeof(buf));

	if( memcmp(rightmost, buf, sizeof(buf)) != 0 )
		goto fail;

	/**
	 * @brief ROTATE BACK
	 *
	 */

	cursor = cursor_create(tree);
	cursor_traverse_to_ex(cursor, leftmost, sizeof(leftmost), &found);

	if( !found || cursor->current_page_id != 2 )
		goto fail;
	ibta_rotate(cursor, BTA_REBALANCE_MODE_ROTATE_LEFT);
	cursor_destroy(cursor);

	btree_node_init_from_read(
		left_node, left_node->page, pager, left_node->page_number);

	if( left_node->header->num_keys != 2 )
		goto fail;
	if( left_node->header->right_child != left_node_right_most_child )
		goto fail;
	if( left_node->keys[1].key != middlemost_child_page )
		goto fail;

	btree_node_init_from_read(
		right_node, right_node->page, pager, right_node->page_number);

	if( right_node->header->num_keys != 0 )
		goto fail;

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
ibta_merge_test(void)
{
	char const* db_name = "ibta_merge_test.db";
	remove(db_name);

	int result = 1;
	struct BTreeNode* parent_node = NULL;
	struct BTreeNode* left_node = NULL;
	struct BTreeNode* right_node = NULL;
	struct Page* parent_page = NULL;
	struct Page* left_page = NULL;
	struct Page* right_page = NULL;
	struct Page* test_page = NULL;

	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	page_cache_create(&cache, 11);
	pager_cstd_create(&pager, cache, db_name, 0x1000);
	struct BTreeNodeRC rcer;
	noderc_init(&rcer, pager);
	struct BTree* tree;
	btree_alloc(&tree);
	if( ibtree_init(
			tree, pager, &rcer, 2, &ibtree_compare, &ibtree_compare_reset) !=
		BTREE_OK )
	{
		result = 0;
		goto end;
	}

	page_create(pager, &parent_page);
	page_create(pager, &left_page);
	page_create(pager, &right_page);

	btree_node_create_as_page_number(&parent_node, 2, parent_page);
	btree_node_create_as_page_number(&left_node, 3, left_page);
	btree_node_create_as_page_number(&right_node, 4, right_page);

	// This test is an impossibility, but it covers what we want.
	// We've given the cells children but also label them as leaves.
	right_node->header->is_leaf = 1;
	left_node->header->is_leaf = 1;

	u32 left_node_right_most_child = 88;
	left_node->header->right_child = left_node_right_most_child;
	pager_write_page(pager, parent_page);
	pager_write_page(pager, left_page);
	pager_write_page(pager, right_page);

	parent_node->header->right_child = right_node->page_number;
	struct InsertionIndex insert_index = {0};
	insert_index.index = 0;
	insert_index.mode = KLIM_END;
	char rightmost[] = "xbilly";
	btree_node_write_ex(
		parent_node,
		pager,
		&insert_index,
		left_node->page_number,
		0,
		rightmost,
		sizeof(rightmost),
		WRITER_EX_MODE_RAW);

	char leftmost[] = "lkrilly";
	u32 leftmost_child_page = 13;
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		leftmost_child_page,
		0,
		leftmost,
		sizeof(leftmost),
		WRITER_EX_MODE_RAW);
	char middlemost[] = "mobly";
	u32 middlemost_child_page = 11;
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		middlemost_child_page,
		0,
		middlemost,
		sizeof(middlemost),
		WRITER_EX_MODE_RAW);
	char rightest[] = "zbilly";
	u32 rightest_child_page = 18;
	btree_node_write_ex(
		right_node,
		pager,
		&insert_index,
		rightest_child_page,
		0,
		rightest,
		sizeof(rightest),
		WRITER_EX_MODE_RAW);

	char deadend[] = "zzbilly";
	char found;
	struct Cursor* cursor = cursor_create(tree);
	cursor_traverse_to_ex(cursor, deadend, sizeof(deadend), &found);

	struct CursorBreadcrumb crumb = {0};

	ibta_merge(cursor, BTA_REBALANCE_MODE_MERGE_LEFT);
	cursor_destroy(cursor);

	btree_node_init_from_read(
		parent_node, parent_node->page, pager, parent_node->page_number);

	// TODO: Better merge test
	if( parent_node->header->num_keys != 0 )
		goto fail;

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
ibta_rebalance_test(void)
{
	char const* db_name = "ibta_rebalance_test.db";
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
	if( ibtree_init(
			tree, pager, &rcer, 2, &ibtree_compare, &ibtree_compare_reset) !=
		BTREE_OK )
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
	char G[] = "G";
	char H[] = "H";
	char I[] = "I";

	struct InsertionIndex insert_index = {0};
	insert_index.index = 0;
	insert_index.mode = KLIM_END;
	btree_node_write_ex(
		parent_node,
		pager,
		&insert_index,
		left_node->page_number,
		0,
		F,
		sizeof(F),
		WRITER_EX_MODE_RAW);

	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		l1_node->page_number,
		0,
		B,
		sizeof(B),
		WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		l2_node->page_number,
		0,
		D,
		sizeof(D),
		WRITER_EX_MODE_RAW);
	left_node->header->right_child = l3_node->page_number;
	btree_node_write_ex(
		l1_node, pager, &insert_index, 0, 0, A, sizeof(A), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l2_node, pager, &insert_index, 0, 0, C, sizeof(C), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l3_node, pager, &insert_index, 0, 0, E, sizeof(E), WRITER_EX_MODE_RAW);

	btree_node_write_ex(
		right_node,
		pager,
		&insert_index,
		l4_node->page_number,
		0,
		H,
		sizeof(H),
		WRITER_EX_MODE_RAW);
	right_node->header->right_child = l5_node->page_number;
	btree_node_write_ex(
		l4_node, pager, &insert_index, 0, 0, G, sizeof(G), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l5_node, pager, &insert_index, 0, 0, I, sizeof(I), WRITER_EX_MODE_RAW);

	ibtree_delete(tree, G, sizeof(G));

	struct Cursor* cursor;
	char found;
	char* tests[] = {A, B, C, D, E, F, H, I};
	char buf[100] = {0};
	for( int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++ )
	{
		char* testi = tests[i];
		memset(buf, 0x00, sizeof(buf));
		cursor = cursor_create(tree);
		cursor_traverse_to_ex(cursor, testi, strlen(testi) + 1, &found);

		if( !found )
			goto fail;

		btree_node_init_from_read(
			&test_node, test_page, pager, cursor->current_page_id);
		cursor_destroy(cursor);

		btree_node_read_ex(
			tree, &test_node, testi, strlen(testi) + 1, buf, sizeof(buf));

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
ibta_rebalance_nonleaf_test(void)
{
	char const* db_name = "ibta_ance_nl_test.db";
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
	if( ibtree_init(
			tree, pager, &rcer, 2, &ibtree_compare, &ibtree_compare_reset) !=
		BTREE_OK )
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
	char G[] = "G";
	char H[] = "H";
	char I[] = "I";

	struct InsertionIndex insert_index = {0};
	insert_index.index = 0;
	insert_index.mode = KLIM_END;
	btree_node_write_ex(
		parent_node,
		pager,
		&insert_index,
		left_node->page_number,
		0,
		F,
		sizeof(F),
		WRITER_EX_MODE_RAW);

	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		l1_node->page_number,
		0,
		B,
		sizeof(B),
		WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		l2_node->page_number,
		0,
		D,
		sizeof(D),
		WRITER_EX_MODE_RAW);
	left_node->header->right_child = l3_node->page_number;
	btree_node_write_ex(
		l1_node, pager, &insert_index, 0, 0, A, sizeof(A), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l2_node, pager, &insert_index, 0, 0, C, sizeof(C), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l3_node, pager, &insert_index, 0, 0, E, sizeof(E), WRITER_EX_MODE_RAW);

	btree_node_write_ex(
		right_node,
		pager,
		&insert_index,
		l4_node->page_number,
		0,
		H,
		sizeof(H),
		WRITER_EX_MODE_RAW);
	right_node->header->right_child = l5_node->page_number;
	btree_node_write_ex(
		l4_node, pager, &insert_index, 0, 0, G, sizeof(G), WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		l5_node, pager, &insert_index, 0, 0, I, sizeof(I), WRITER_EX_MODE_RAW);

	ibtree_delete(tree, H, sizeof(H));

	struct Cursor* cursor;
	char found;
	char* tests[] = {A, B, C, D, E, F, G, I};
	char buf[100] = {0};
	for( int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++ )
	{
		char* testi = tests[i];
		memset(buf, 0x00, sizeof(buf));
		cursor = cursor_create(tree);
		cursor_traverse_to_ex(cursor, testi, strlen(testi) + 1, &found);

		if( !found )
			goto fail;

		btree_node_init_from_read(
			&test_node, test_page, pager, cursor->current_page_id);
		cursor_destroy(cursor);

		btree_node_read_ex(
			tree, &test_node, testi, strlen(testi) + 1, buf, sizeof(buf));

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
ibta_rebalance_root_fit(void)
{
	char const* db_name = "ibta_rebalance_root_fit.db";
	remove(db_name);

	int result = 1;
	struct BTreeNode test_node = {0};
	struct BTreeNode* parent_node = NULL;
	struct BTreeNode* left_node = NULL;
	struct BTreeNode* right_node = NULL;
	struct Page* parent_page = NULL;
	struct Page* left_page = NULL;
	struct Page* right_page = NULL;
	struct Page* test_page = NULL;

	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	page_cache_create(&cache, 11);
	pager_cstd_create(&pager, cache, db_name, 0x1000);
	struct BTreeNodeRC rcer;
	noderc_init(&rcer, pager);
	struct BTree* tree;
	btree_alloc(&tree);
	if( ibtree_init(
			tree, pager, &rcer, 1, &ibtree_compare, &ibtree_compare_reset) !=
		BTREE_OK )
	{
		result = 0;
		goto end;
	}

	page_create(pager, &test_page);
	page_create(pager, &parent_page);
	page_create(pager, &left_page);
	page_create(pager, &right_page);
	btree_node_create_as_page_number(&parent_node, 1, parent_page);
	btree_node_create_as_page_number(&left_node, 2, left_page);
	btree_node_create_as_page_number(&right_node, 3, right_page);

	parent_node->header->right_child = right_node->page_number;
	node_is_leaf_set(left_node, true);
	node_is_leaf_set(right_node, true);

	pager_write_page(pager, parent_page);
	pager_write_page(pager, left_page);
	pager_write_page(pager, right_page);
	char A[] = "A";
	char B[] = "B";
	char C[] = "C";

	struct InsertionIndex insert_index = {0};
	insert_index.index = 0;
	insert_index.mode = KLIM_END;
	btree_node_write_ex(
		parent_node,
		pager,
		&insert_index,
		left_node->page_number,
		0,
		B,
		sizeof(B),
		WRITER_EX_MODE_RAW);

	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		0,
		0,
		A,
		sizeof(A),
		WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		right_node,
		pager,
		&insert_index,
		0,
		0,
		C,
		sizeof(C),
		WRITER_EX_MODE_RAW);

	ibtree_delete(tree, C, sizeof(C));

	struct Cursor* cursor;
	char found;
	char* tests[] = {A, B};
	char buf[100] = {0};
	for( int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++ )
	{
		char* testi = tests[i];
		memset(buf, 0x00, sizeof(buf));
		cursor = cursor_create(tree);
		cursor_traverse_to_ex(cursor, testi, strlen(testi) + 1, &found);

		if( !found )
			goto fail;

		btree_node_init_from_read(
			&test_node, test_page, pager, cursor->current_page_id);
		cursor_destroy(cursor);

		btree_node_read_ex(
			tree, &test_node, testi, strlen(testi) + 1, buf, sizeof(buf));

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
ibta_rebalance_root_nofit(void)
{
	char const* db_name = "ibta_rebalance_root_nofit.db";
	remove(db_name);

	int result = 1;
	struct BTreeNode test_node = {0};
	struct BTreeNode* parent_node = NULL;
	struct BTreeNode* left_node = NULL;
	struct BTreeNode* right_node = NULL;
	struct Page* parent_page = NULL;
	struct Page* left_page = NULL;
	struct Page* right_page = NULL;
	struct Page* test_page = NULL;

	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	// 1 byte of payload can fit on the first page.
	u32 page_size = btree_min_page_size() + 1 * 4;
	page_cache_create(&cache, 11);
	pager_cstd_create(&pager, cache, db_name, page_size);
	struct BTreeNodeRC rcer;
	noderc_init(&rcer, pager);
	struct BTree* tree;
	btree_alloc(&tree);
	if( ibtree_init(
			tree, pager, &rcer, 1, &ibtree_compare, &ibtree_compare_reset) !=
		BTREE_OK )
	{
		result = 0;
		goto end;
	}
	btree_underflow_lim_set(tree, 7);
	page_create(pager, &test_page);
	page_create(pager, &parent_page);
	page_create(pager, &left_page);
	page_create(pager, &right_page);
	btree_node_create_as_page_number(&parent_node, 1, parent_page);
	btree_node_create_as_page_number(&left_node, 2, left_page);
	btree_node_create_as_page_number(&right_node, 3, right_page);

	parent_node->header->right_child = right_node->page_number;
	node_is_leaf_set(left_node, true);
	node_is_leaf_set(right_node, true);

	pager_write_page(pager, parent_page);
	pager_write_page(pager, left_page);
	pager_write_page(pager, right_page);
	char A[] = "AAAAAAAA";
	char B[] = "BBBBBBBB";
	char C[] = "CCCCCCCC";
	char D[] = "DDDDDDDD";
	char E[] = "EEEEEEEE";
	char F[] = "FFFFFFFF";
	char G[] = "GGGGGGGG";
	char H[] = "HHHHHHHH";

	struct InsertionIndex insert_index = {0};
	insert_index.index = 0;
	insert_index.mode = KLIM_END;
	btree_node_write_ex(
		parent_node,
		pager,
		&insert_index,
		left_node->page_number,
		0,
		F,
		sizeof(F),
		WRITER_EX_MODE_RAW);

	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		0,
		0,
		A,
		sizeof(A),
		WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		0,
		0,
		B,
		sizeof(B),
		WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		0,
		0,
		C,
		sizeof(C),
		WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		0,
		0,
		D,
		sizeof(D),
		WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		left_node,
		pager,
		&insert_index,
		0,
		0,
		E,
		sizeof(E),
		WRITER_EX_MODE_RAW);
	btree_node_write_ex(
		right_node,
		pager,
		&insert_index,
		0,
		0,
		G,
		sizeof(G),
		WRITER_EX_MODE_RAW);

	ibtree_delete(tree, G, sizeof(G));

	ibtree_insert(tree, H, sizeof(H));

	struct Cursor* cursor;
	char found;
	char* tests[] = {A, B, C, D, E, F, H};
	char buf[100] = {0};
	for( int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++ )
	{
		char* testi = tests[i];
		memset(buf, 0x00, sizeof(buf));
		cursor = cursor_create(tree);
		cursor_traverse_to_ex(cursor, testi, strlen(testi) + 1, &found);

		if( !found )
			goto fail;

		btree_node_init_from_read(
			&test_node, test_page, pager, cursor->current_page_id);
		cursor_destroy(cursor);

		btree_node_read_ex(
			tree, &test_node, testi, strlen(testi) + 1, buf, sizeof(buf));

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

const int header_size = 7;
const int key_size = 4;
struct CompareCtx
{
	byte compare_buffer[4];
	u32 len;
	u32 offset;
};

static void
cmp_reset(void* ctx)
{
	memset(ctx, 0x00, sizeof(struct CompareCtx));
}

static int
compare(
	void* ctx,
	void* window,
	u32 window_size,
	u32 wnd_total_size,
	void* right,
	u32 right_size,
	u32 bytes_compared,
	u32* out_bytes_compared,
	u32* key_size_rem)
{
	struct CompareCtx* cmper = (struct CompareCtx*)ctx;
	*key_size_rem = key_size;
	byte* ptr = (byte*)window;
	byte* rptr = (byte*)right;
	rptr += header_size;

	u32 window_offset = 0;

	while( cmper->offset < header_size && window_offset < window_size )
	{
		ptr += 1;
		cmper->offset += 1;
		window_offset += 1;
	}

	if( cmper->offset >= header_size )
	{
		while( cmper->len < key_size && window_offset < window_size )
		{
			cmper->compare_buffer[cmper->len] = ptr[0];
			ptr += 1;
			cmper->len += 1;
			window_offset += 1;
		}
	}

	if( cmper->len == key_size )
	{
		int cmp = memcmp(cmper->compare_buffer, rptr, key_size);
		*out_bytes_compared = window_offset + key_size;
		*key_size_rem = 0;
		if( cmp == 0 )
		{
			return cmp;
		}
		else
		{
			return cmp < 0 ? -1 : 1;
		}
	}
	else
	{
		*out_bytes_compared = window_offset;
		return 0;
	}
}

int
ibta_cmp_ctx_test(void)
{
	char const* db_name = "ibta_cmp_ctx_test.db";
	remove(db_name);

	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;
	struct BTreeNodeRC rcer = {0};
	struct BTree* tree = NULL;
	struct BTreeNode test_node = {0};
	struct Page* test_page = NULL;
	int result = 1;

	page_cache_create(&cache, 11);
	u32 page_size = btree_min_page_size() + 1 * 4;
	pager_cstd_create(&pager, cache, db_name, page_size);
	page_create(pager, &test_page);
	noderc_init(&rcer, pager);
	btree_alloc(&tree);
	if( ibtree_init(tree, pager, &rcer, 1, &compare, &cmp_reset) != BTREE_OK )
	{
		result = 0;
		goto end;
	}

	char A[] = "AAADER-AKEYA-DATA";
	char B[] = "BBADER-BKEYB-DATA";
	char C[] = "CCADER-CKEYC-DATA";

	struct CompareCtx ctx = {0};
	ibtree_insert_ex(tree, A, sizeof(A), &ctx);
	ibtree_insert_ex(tree, B, sizeof(B), &ctx);
	ibtree_insert_ex(tree, C, sizeof(C), &ctx);

	// btree_node_init_from_read(&test_node, test_page, pager, 1);
	// dbg_print_buffer(test_page->page_buffer, test_page->page_size);

	struct Cursor* cursor;
	char found;
	char* tests[] = {A, B, C};
	char buf[100] = {0};
	for( int i = 0; i < sizeof(tests) / sizeof(tests[0]); i++ )
	{
		char* testi = tests[i];
		memset(buf, 0x00, sizeof(buf));
		cursor = cursor_create_ex(tree, &ctx);
		cursor_traverse_to_ex(cursor, testi, strlen(testi) + 1, &found);

		if( !found )
			goto fail;

		btree_node_init_from_read(
			&test_node, test_page, pager, cursor->current_page_id);

		cursor_destroy(cursor);

		btree_node_read_ex2(
			tree, &ctx, &test_node, testi, strlen(testi) + 1, buf, sizeof(buf));

		if( memcmp(buf, testi, strlen(testi) + 1) != 0 )
			goto fail;
	}

end:
	remove(db_name);
	return result;
fail:
	result = 0;
	goto end;
}