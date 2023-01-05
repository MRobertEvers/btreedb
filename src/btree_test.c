#include "btree_test.h"

#include "btree.h"
#include "btree_alg.h"
#include "btree_cell.h"
#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_node_debug.h"
#include "btree_utils.h"
#include "page_cache.h"
#include "pager_ops_cstd.h"

#include <stdio.h>
#include <string.h>

int
btree_test_insert()
{
	int result = 1;
	struct Pager* pager;

	struct PageCache* cache = NULL;
	remove("btree_test_insert2.db");

	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, "btree_test_insert2.db", 0x1000);

	struct BTree* tree;
	btree_alloc(&tree);
	btree_init(tree, pager);

	char billy[4096 - 200] = "billy";
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

	// There should be one key witha right child
	if( raw_node->header->num_keys != 1 || raw_node->header->right_child == 0 )
	{
		result = 0;
		goto end;
	}

end:
	btree_node_destroy(raw_node);
	page_destroy(pager, raw_page);
	btree_deinit(tree);
	btree_dealloc(tree);
	remove("btree_test_insert2.db");
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
	int result = 1;
	struct BTreeNode* node = 0;

	struct Pager* pager;
	struct PageCache* cache = NULL;
	remove("delete_key.db");
	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, "delete_key.db", 0x1000);

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

	struct Page* page;
	page_create(tree->pager, &page);

	pager_reselect(&selector, cursor->current_page_id);
	pager_read_page(tree->pager, &selector, page);
	btree_node_create_from_page(&node, page);

	if( node->header->num_keys != 1 )
	{
		result = 0;
		goto end;
	}

end:
	btree_node_destroy(raw_root_node);
	page_destroy(pager, raw_page);
	btree_node_destroy(node);
	// remove("delete_key.db");

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
		sizeof(ruth) + sizeof(struct BTreePageKey) + sizeof(int) )
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

	for( int i = 0; i < 10; i++ )
	{
		page_create(pager, &page);
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
	pager_cstd_create(&pager, cache, db_name, 200);

	struct BTree* tree;
	btree_alloc(&tree);
	enum btree_e btresult = btree_init(tree, pager);
	if( result != BTREE_OK )
	{
		result = 0;
		goto end;
	}

	char billy[40] = "billy";		 // 12
	char ruth[40] = "ruth";			 // 1
	char charlie[40] = "charlie";	 // 2
	char buxley[40] = "buxley";		 // 13
	char herman[40] = "herman";		 // 14
	char flaur[100] = "flaur";		 // 11
	char flemming[100] = "flemming"; // 30
	char yuta[100] = "yuta";		 // 7
	char xiao[100] = "xiao";		 // 15
	char gilly[100] = "gilly";		 // 21

	btree_insert(tree, 1, ruth, sizeof(ruth));
	btree_insert(tree, 2, charlie, sizeof(charlie));

	/**
	 * Ensure that we can still reach certain keys
	 * after inserting others.
	 */
	struct Cursor* cursor = cursor_create(tree);

	char found = 0;
	cursor_traverse_to(cursor, 1, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	// Insert 12
	btree_insert(tree, 12, billy, sizeof(billy));

	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 1, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	// Insert 13/14
	btree_insert(tree, 13, buxley, sizeof(buxley));
	btree_insert(tree, 14, herman, sizeof(herman));
	printf("\nAfter 13/14\n");
	dbg_deep_tree_pages(tree->pager);

	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 1, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	// Insert 11
	btree_insert(tree, 11, flaur, sizeof(flaur));
	printf("\nAfter 11\n");
	dbg_deep_tree_pages(tree->pager);

	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 1, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	// Insert 30
	btree_insert(tree, 30, flemming, sizeof(flemming));
	printf("\nAfter 30\n");
	dbg_deep_tree_pages(tree->pager);

	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 1, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	// Insert 7
	btree_insert(tree, 7, yuta, sizeof(yuta));
	printf("\nAfter 7\n");
	dbg_deep_tree_pages(tree->pager);

	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 1, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	// Insert 15
	btree_insert(tree, 15, xiao, sizeof(xiao));
	printf("\nAfter 15\n");
	dbg_deep_tree_pages(tree->pager);

	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 1, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	// Insert 21
	btree_insert(tree, 21, gilly, sizeof(gilly));

	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 1, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	/**
	 * @brief Ensure we can reach all keys.
	 */
	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 12, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 1, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 2, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 13, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 14, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 11, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 30, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 7, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 15, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	cursor = cursor_create(tree);
	cursor_traverse_to(cursor, 21, &found);
	if( !found )
		goto fail;
	cursor_destroy(cursor);

	dbg_deep_tree_pages(tree->pager);
	/**
	 * @brief
	 * Page 1 (leaf? 0): 1 (p.2), 2 (p.5), 12 (p.4), ;3
	 * Page 2 (leaf? 1): 1 (p.1752462706),
	 * Page 3 (leaf? 1): 13 (p.1819833698), 14 (p.1836213608),
	 * Page 4 (leaf? 1): 11 (p.1969319014), 12 (p.1819044194),
	 * Page 5 (leaf? 1): 2 (p.1918986339),
	 */

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

	if( node->header->num_keys != 2 )
	{
		result = 0;
		goto end;
	}

	btu_read_cell(node, cursor->current_key_index.index, &cell);

	if( btree_cell_get_size(&cell) != sizeof(billy) )
	{
		result = 0;
		goto end;
	}

	if( memcmp(cell.pointer, billy, sizeof(billy)) != 0 )
	{
		result = 0;
		goto end;
	}

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