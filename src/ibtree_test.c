#include "ibtree_test.h"

#include "btree.h"
#include "btree_cursor.h"
#include "btree_defs.h"
#include "btree_node.h"
#include "btree_node_reader.h"
#include "ibtree.h"
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

	struct BTree* tree;
	btree_alloc(&tree);
	enum btree_e btresult = ibtree_init(tree, pager, 1, &ibtree_compare);
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
		tree,
		test_node,
		tree->pager,
		flurb,
		sizeof(flurb),
		alpha_buf,
		sizeof(alpha_buf));

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

	struct BTree* tree;
	btree_alloc(&tree);
	enum btree_e btresult = ibtree_init(tree, pager, 1, &ibtree_compare);
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
		tree,
		test_node,
		tree->pager,
		splurb,
		sizeof(splurb),
		alpha_buf,
		sizeof(alpha_buf));

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
	pager_cstd_create(&pager, cache, db_name, 220);

	struct BTree* tree;
	btree_alloc(&tree);
	enum btree_e btresult = ibtree_init(tree, pager, 1, &ibtree_compare);
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
		{.key = 13, .data = sybian, .size = sizeof(sybian)},	 // 23
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
		printf("Insert i: %i (%d)\r\n", i, test->key);
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
	btree_node_read_ex(
		tree, node, tree->pager, hoosin, sizeof(hoosin), buf, sizeof(buf));

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