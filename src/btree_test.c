#include "btree_test.h"

#include "btree.h"
#include "btree_alg.h"
#include "btree_cell.h"
#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_node_debug.h"
#include "btree_node_reader.h"
#include "btree_node_writer.h"
#include "btree_utils.h"
#include "page_cache.h"
#include "pager_ops_cstd.h"
#include "serialization.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

int
btree_test_insert()
{
	int result = 1;
	struct Pager* pager;
	char const* db_name = "btree_test_insert2.db";
	struct PageCache* cache = NULL;
	remove(db_name);

	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, db_name, 0x1000);

	struct BTree* tree;
	btree_alloc(&tree);
	btree_init(tree, pager);

	char billy[0x1000 - 200] = "billy";
	billy[3600] = 'q';
	btree_insert(tree, 12, billy, sizeof(billy));

	char ruth[500] = "ruth";
	btree_insert(tree, 1, ruth, sizeof(ruth));
	// Get the root node
	struct Page* raw_page;
	page_create(pager, &raw_page);

	struct PageSelector selector;
	pager_reselect(&selector, tree->root_page_id);
	pager_read_page(pager, &selector, raw_page);

	struct BTreeNode* raw_node;
	btree_node_create_from_page(&raw_node, raw_page);

	char buf[sizeof(billy)] = {0};
	btree_node_read(raw_node, pager, 12, buf, sizeof(buf));

	if( memcmp(buf, billy, sizeof(billy)) != 0 )
		result = 0;

end:
	btree_node_destroy(raw_node);
	page_destroy(pager, raw_page);
	btree_deinit(tree);
	btree_dealloc(tree);
	remove(db_name);
	return result;
}

int
btree_test_insert_root_with_space()
{
	int result = 1;
	struct Pager* pager;

	struct PageCache* cache = NULL;
	remove("test_.db");

	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, "test_.db", 0x1000);

	struct BTree* tree;
	btree_alloc(&tree);
	btree_init(tree, pager);

	char billy[] = "billy";
	char ruth[] = "ruth";
	btree_insert(tree, 12, billy, sizeof(billy));
	btree_insert(tree, 1, ruth, sizeof(ruth));

	// Get the root node
	struct Page* raw_page;
	page_create(pager, &raw_page);

	struct PageSelector selector;
	pager_reselect(&selector, tree->root_page_id);
	pager_read_page(pager, &selector, raw_page);

	struct BTreeNode* raw_node;
	btree_node_create_from_page(&raw_node, raw_page);

	if( raw_node->header->num_keys != 2 )
	{
		result = 0;
		goto end;
	}

	if( raw_node->keys[0].key != 1 || raw_node->keys[1].key != 12 )
	{
		result = 0;
		goto end;
	}

end:
	btree_node_destroy(raw_node);
	page_destroy(pager, raw_page);
	btree_deinit(tree);
	btree_dealloc(tree);
	remove("test_.db");
	return result;
}

int
btree_test_split_root_node()
{
	int result = 1;
	struct BTreeNode* node = 0;

	struct Pager* pager;
	struct PageCache* cache = NULL;
	remove("split_root_node.db");
	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, "split_root_node.db", 0x1000);

	struct BTree* tree;
	btree_alloc(&tree);
	btree_init(tree, pager);

	char billy[] = "billy";
	char ruth[] = "ruth";
	char charlie[] = "charlie";
	btree_insert(tree, 12, billy, sizeof(billy));
	btree_insert(tree, 2, charlie, sizeof(charlie));
	btree_insert(tree, 1, ruth, sizeof(ruth));

	struct Page* raw_page;
	page_create(pager, &raw_page);

	struct PageSelector selector;
	pager_reselect(&selector, tree->root_page_id);
	pager_read_page(pager, &selector, raw_page);

	struct BTreeNode* raw_root_node;
	btree_node_create_from_page(&raw_root_node, raw_page);
	bta_split_node_as_parent(raw_root_node, tree->pager, NULL);

	struct Cursor* cursor = cursor_create(tree);

	char found = 0;
	cursor_traverse_to(cursor, 2, &found);

	if( !found )
	{
		result = 0;
		goto end;
	}

	struct Page* page;
	page_create(tree->pager, &page);

	pager_reselect(&selector, cursor->current_page_id);
	pager_read_page(tree->pager, &selector, page);
	btree_node_create_from_page(&node, page);

	if( node->header->num_keys != 2 )
	{
		result = 0;
		goto end;
	}

	struct CellData cell;
	btu_read_cell(node, cursor->current_key_index.index, &cell);

	if( btree_cell_get_size(&cell) != sizeof(charlie) )
	{
		result = 0;
		goto end;
	}

	if( memcmp(cell.pointer, charlie, sizeof(charlie)) != 0 )
	{
		result = 0;
		goto end;
	}

end:
	btree_node_destroy(raw_root_node);
	page_destroy(pager, raw_page);
	btree_node_destroy(node);
	remove("split_root_node.db");

	return 1;
}

int
btree_test_delete(void)
{
	char const* db_name = "delete_key.db";
	int result = 1;
	struct BTreeNode* node = 0;

	struct Pager* pager;
	struct PageCache* cache = NULL;
	remove(db_name);
	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, db_name, 0x1000);

	struct BTree* tree;
	btree_alloc(&tree);
	btree_init(tree, pager);

	char billy[] = "billy";
	char ruth[] = "ruth";
	char charlie[] = "charlie";
	btree_insert(tree, 12, billy, sizeof(billy));
	btree_insert(tree, 2, charlie, sizeof(charlie));
	btree_insert(tree, 1, ruth, sizeof(ruth));

	btree_delete(tree, 2);

	struct Page* raw_page;
	page_create(pager, &raw_page);

	struct PageSelector selector;
	pager_reselect(&selector, tree->root_page_id);
	pager_read_page(pager, &selector, raw_page);

	struct BTreeNode* raw_root_node;
	btree_node_create_from_page(&raw_root_node, raw_page);
	bta_split_node_as_parent(raw_root_node, tree->pager, NULL);

	struct Cursor* cursor = cursor_create(tree);

	char found = 0;
	cursor_traverse_to(cursor, 2, &found);

	if( found )
	{
		result = 0;
		goto end;
	}
	cursor_destroy(cursor);

	// Check that the insertion is reachable.
	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 1, &found);
	if( !found )
	{
		result = 0;
		goto end;
	}

	pager_reselect(&selector, cursor->current_page_id);
	pager_read_page(tree->pager, &selector, raw_page);
	btree_node_create_from_page(&node, raw_page);

	char buf[sizeof(ruth)] = {0};
	btree_node_read(node, pager, 1, buf, sizeof(buf));

	if( memcmp(buf, ruth, sizeof(ruth)) != 0 )
		result = 0;

	cursor_destroy(cursor);
	cursor = NULL;

	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 12, &found);
	if( !found )
	{
		result = 0;
		goto end;
	}

	pager_reselect(&selector, cursor->current_page_id);
	pager_read_page(tree->pager, &selector, raw_page);
	btree_node_create_from_page(&node, raw_page);

	char buf2[sizeof(billy)] = {0};
	btree_node_read(node, pager, 12, buf2, sizeof(buf2));

	if( memcmp(buf2, billy, sizeof(billy)) != 0 )
		result = 0;

	cursor_destroy(cursor);
	cursor = NULL;

end:
	if( cursor )
		cursor_destroy(cursor);
	btree_node_destroy(raw_root_node);
	page_destroy(pager, raw_page);
	btree_node_destroy(node);
	remove(db_name);

	return 1;
}

int
btree_test_free_heap_calcs()
{
	int result = 1;
	struct Pager* pager;

	struct PageCache* cache = NULL;
	remove("test_free_heap.db");

	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, "test_free_heap.db", 0x1000);

	struct BTree* tree;
	btree_alloc(&tree);
	btree_init(tree, pager);

	char billy[] = "billy";
	char ruth[] = "ruth";
	btree_insert(tree, 12, billy, sizeof(billy));

	// Get the root node
	struct Page* raw_page;
	page_create(pager, &raw_page);

	struct PageSelector selector;
	pager_reselect(&selector, tree->root_page_id);
	pager_read_page(pager, &selector, raw_page);

	struct BTreeNode* raw_node;
	btree_node_create_from_page(&raw_node, raw_page);

	int free_heap_billy = raw_node->header->free_heap;

	btree_insert(tree, 1, ruth, sizeof(ruth));

	pager_read_page(pager, &selector, raw_page);
	btree_node_create_from_page(&raw_node, raw_page);

	if( free_heap_billy - raw_node->header->free_heap !=
		btree_node_get_heap_required_for_insertion(
			btree_cell_inline_get_inline_heap_size(sizeof(ruth))) )
		result = 0;

end:
	btree_node_destroy(raw_node);
	page_destroy(pager, raw_page);
	btree_deinit(tree);
	btree_dealloc(tree);
	remove("test_free_heap.db");
	return result;
}

static void
dbg_deep_tree_pages(struct Pager* pager)
{
	struct Page* page = NULL;
	struct BTreeNode* node = NULL;
	struct PageSelector selector;
	page_create(pager, &page);

	for( int i = 0; i < 50; i++ )
	{
		pager_reselect(&selector, i + 1);
		enum pager_e result = pager_read_page(pager, &selector, page);
		if( result == PAGER_ERR_NIF )
			break;
		btree_node_create_from_page(&node, page);

		dbg_print_node(node);
	}

	if( page )
		page_destroy(pager, page);
}

struct TestRecord
{
	unsigned int key;
	void* data;
	unsigned int size;
};

int
btree_test_deep_tree(void)
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
	enum btree_e btresult = btree_init(tree, pager);
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
	char herman[40] = "herman";
	char flaur[100] = "flaur";
	char flemming[100] = "flemming";
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
		btree_insert(tree, test->key, test->data, test->size);

		// Always check that 1 is still reachable.
		cursor = cursor_create(tree);
		cursor_traverse_to(cursor, 1, &found);
		if( !found )
			goto fail;
		cursor_destroy(cursor);

		// Check that the insertion is reachable.
		cursor = cursor_create(tree);
		cursor_traverse_to(cursor, test->key, &found);
		if( !found )
			goto fail;
		cursor_destroy(cursor);
	}

	/**
	 * @brief Test reading the cell returns the correct data.
	 */
	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 12, &found);
	if( !found )
		goto fail;

	page_create(tree->pager, &page);
	pager_reselect(&selector, cursor->current_page_id);
	pager_read_page(tree->pager, &selector, page);
	btree_node_create_from_page(&node, page);

	char buf[sizeof(hoosin)] = {0};
	btree_node_read(node, pager, 12, buf, sizeof(buf));

	if( memcmp(buf, hoosin, sizeof(hoosin)) != 0 )
		result = 0;

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
btree_test_delete_last(void)
{
	char const* db_name = "delete_last_key.db";
	int result = 1;
	struct BTreeNode* node = 0;
	struct PageSelector selector;
	struct Pager* pager;
	struct PageCache* cache = NULL;
	struct InsertionIndex index = {0};
	struct BTreeCellInline cell = {0};

	remove(db_name);
	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, db_name, 0x1000);

	struct BTree* tree;
	btree_alloc(&tree);
	enum btree_e btresult = btree_init(tree, pager);
	if( btresult != BTREE_OK )
	{
		result = 0;
		goto end;
	}

	struct Page* test_page;
	struct Page* root_page;
	struct Page* left_page;
	struct Page* middle_page;
	struct Page* right_page;

	page_create(pager, &test_page);
	page_create(pager, &root_page);
	page_create(pager, &left_page);
	page_create(pager, &middle_page);
	page_create(pager, &right_page);

	pager_reselect(&selector, 1);
	pager_read_page(pager, &selector, root_page);

	struct BTreeNode* test_node;
	struct BTreeNode* root_node;
	struct BTreeNode* left_node;
	struct BTreeNode* middle_node;
	struct BTreeNode* right_node;

	btree_node_create_from_page(&root_node, root_page);
	root_node->header->is_leaf = 0;
	pager_write_page(pager, root_page);

	btree_node_create_from_page(&left_node, left_page);
	left_node->header->is_leaf = 1;
	pager_write_page(pager, left_page);

	btree_node_create_from_page(&middle_node, middle_page);
	middle_node->header->is_leaf = 1;
	pager_write_page(pager, middle_page);

	btree_node_create_from_page(&right_node, right_page);
	right_node->header->is_leaf = 1;
	pager_write_page(pager, right_page);

	char const alpha[] = "alpha";
	char const beta[] = "beta";
	char const gamma[] = "gamma";

	index.mode = KLIM_END;

	cell.inline_size = sizeof(alpha);
	cell.payload = alpha;
	btree_node_insert_inline(left_node, &index, 1, &cell);
	pager_write_page(pager, left_page);
	cell.inline_size = sizeof(beta);
	cell.payload = beta;
	btree_node_insert_inline(middle_node, &index, 2, &cell);
	pager_write_page(pager, middle_page);
	cell.inline_size = sizeof(gamma);
	cell.payload = gamma;
	btree_node_insert_inline(right_node, &index, 3, &cell);
	pager_write_page(pager, right_page);

	char page_key_buf[sizeof(u32)] = {0};
	cell.payload = page_key_buf;
	cell.inline_size = sizeof(page_key_buf);

	root_node->header->is_leaf = 0;
	ser_write_32bit_le(page_key_buf, left_node->page->page_id);
	btree_node_insert_inline(root_node, &index, 1, &cell);
	ser_write_32bit_le(page_key_buf, middle_node->page->page_id);
	btree_node_insert_inline(root_node, &index, 2, &cell);
	ser_write_32bit_le(page_key_buf, right_node->page->page_id);
	root_node->header->right_child = right_node->page->page_id;
	pager_write_page(pager, root_page);

	btree_delete(tree, 3);

	pager_reselect(&selector, 1);
	pager_read_page(pager, &selector, test_page);
	btree_node_create_from_page(&test_node, test_page);

	if( test_node->header->num_keys != 1 )
	{
		result = 0;
		goto end;
	}
	if( test_node->header->right_child != middle_node->page->page_id )
	{
		result = 0;
		goto end;
	}

end:

	remove(db_name);

	return result;
}

int
btree_test_delete_merge_root(void)
{
	char const* db_name = "delete_merge_root.db";
	int result = 1;
	struct BTreeNode* node = 0;
	struct PageSelector selector;
	struct Pager* pager;
	struct PageCache* cache = NULL;
	struct InsertionIndex index = {0};
	struct BTreeCellInline cell = {0};

	remove(db_name);
	page_cache_create(&cache, 3);
	// 1 byte of payload can fit on the first page.
	u32 page_size = btree_min_page_size() + 1 * 4;
	pager_cstd_create(&pager, cache, db_name, page_size);

	struct BTree* tree;
	btree_alloc(&tree);
	enum btree_e btresult = btree_init(tree, pager);
	if( btresult != BTREE_OK )
	{
		result = 0;
		goto end;
	}

	struct Page* test_page;
	struct Page* root_page;
	struct Page* left_page;
	struct Page* right_page;

	page_create(pager, &test_page);
	page_create(pager, &root_page);
	page_create(pager, &left_page);
	page_create(pager, &right_page);

	pager_reselect(&selector, 1);
	pager_read_page(pager, &selector, root_page);

	struct BTreeNode* test_node;
	struct BTreeNode* root_node;
	struct BTreeNode* left_node;
	struct BTreeNode* right_node;

	btree_node_create_from_page(&root_node, root_page);
	root_node->header->is_leaf = 0;
	pager_write_page(pager, root_page);

	btree_node_create_from_page(&left_node, left_page);
	left_node->header->is_leaf = 1;
	pager_write_page(pager, left_page);

	btree_node_create_from_page(&right_node, right_page);
	right_node->header->is_leaf = 1;
	pager_write_page(pager, right_page);

	// TODO: Reliably allocate one field that will be larger than a inline cell
	// and one that will be less.
	char const alpha[30] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123";
	char const beta[60] =
		"abcdefghijkLMnopqrstuvwxyz0123abcdefghijklmnopqrstuvwxyz0123";

	index.mode = KLIM_END;

	// TODO:
	cell.inline_size = sizeof(alpha);
	cell.payload = alpha;
	btree_node_write(left_node, pager, 1, alpha, sizeof(alpha));

	pager_write_page(pager, left_page);

	btree_node_write(left_node, pager, 2, beta, sizeof(beta));
	pager_write_page(pager, left_page);
	btree_node_write(right_node, pager, 3, beta, sizeof(beta));
	pager_write_page(pager, right_page);

	char page_key_buf[sizeof(u32)] = {0};
	cell.payload = page_key_buf;
	cell.inline_size = sizeof(page_key_buf);

	root_node->header->is_leaf = 0;
	ser_write_32bit_le(page_key_buf, left_node->page->page_id);
	btree_node_write(root_node, pager, 2, page_key_buf, sizeof(page_key_buf));

	root_node->header->right_child = right_node->page->page_id;
	pager_write_page(pager, root_page);

	// dbg_print_buffer(left_page->page_buffer, left_page->page_size);
	btree_delete(tree, 3);

	pager_reselect(&selector, 1);
	pager_read_page(pager, &selector, test_page);
	btree_node_create_from_page(&test_node, test_page);
	// dbg_print_buffer(test_page->page_buffer, test_page->page_size);

	if( test_node->header->num_keys != 2 )
	{
		result = 0;
		goto end;
	}
	if( test_node->header->right_child != 0 )
	{
		result = 0;
		goto end;
	}
	if( test_node->header->is_leaf == 0 )
	{
		result = 0;
		goto end;
	}

	char alpha_buf[sizeof(alpha)] = {0};

	btree_node_read(test_node, tree->pager, 1, alpha_buf, sizeof(alpha_buf));

	if( memcmp(alpha, alpha_buf, sizeof(alpha)) != 0 )
	{
		result = 0;
		goto end;
	}

	char beta_buf[sizeof(beta)] = {0};
	btree_node_read(test_node, tree->pager, 2, beta_buf, sizeof(beta_buf));

	if( memcmp(beta, beta_buf, sizeof(beta)) != 0 )
	{
		result = 0;
		goto end;
	}

end:

	remove(db_name);

	return result;
}