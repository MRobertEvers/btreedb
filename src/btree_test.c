#include "btree_test.h"

#include "btree.h"
#include "btree_alg.h"
#include "btree_node.h"
#include "btree_utils.h"
#include "page_cache.h"
#include "pager_ops_cstd.h"

#include <stdio.h>
#include <string.h>

int
btree_test_insert2()
{
	int result = 1;
	struct Pager* pager;

	struct PageCache* cache = NULL;
	remove("btree_test_insert2.db");

	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, "btree_test_insert2.db");

	struct BTree* tree;
	btree_alloc(&tree);
	btree_init(tree, pager);

	char billy[4096 - 200] = "billy";
	btree_insert2(tree, 12, billy, sizeof(billy));

	char ruth[500] = "ruth";
	btree_insert2(tree, 1, ruth, sizeof(ruth));
	// Get the root node
	struct Page* raw_page;
	page_create(pager, tree->root_page_id, &raw_page);
	pager_read_page(pager, raw_page);

	struct BTreeNode* raw_node;
	btree_node_create_from_page(tree, &raw_node, raw_page);

	// There should be one key witha right child
	if( raw_node->header->num_keys != 1 || raw_node->header->right_child == 0 )
	{
		result = 0;
		goto end;
	}

end:
	btree_node_destroy(tree, raw_node);
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
	pager_cstd_create(&pager, cache, "test_.db");

	struct BTree* tree;
	btree_alloc(&tree);
	btree_init(tree, pager);

	char billy[] = "billy";
	char ruth[] = "ruth";
	btree_insert(tree, 12, billy, sizeof(billy));
	btree_insert(tree, 1, ruth, sizeof(ruth));

	// Get the root node
	struct Page* raw_page;
	page_create(pager, tree->root_page_id, &raw_page);
	pager_read_page(pager, raw_page);

	struct BTreeNode* raw_node;
	btree_node_create_from_page(tree, &raw_node, raw_page);

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
	btree_node_destroy(tree, raw_node);
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
	pager_cstd_create(&pager, cache, "split_root_node.db");

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
	page_create(pager, tree->root_page_id, &raw_page);
	pager_read_page(pager, raw_page);

	struct BTreeNode* raw_root_node;
	btree_node_create_from_page(tree, &raw_root_node, raw_page);
	bta_bplus_split_node(tree, raw_root_node);

	struct Cursor* cursor = create_cursor(tree);

	char found = 0;
	btree_traverse_to(cursor, 2, &found);

	if( !found )
	{
		result = 0;
		goto end;
	}

	struct Page* page;
	page_create(tree->pager, cursor->current_page_id, &page);
	pager_read_page(tree->pager, page);
	btree_node_create_from_page(tree, &node, page);

	if( node->header->num_keys != 2 )
	{
		result = 0;
		goto end;
	}

	struct CellData cell;
	btu_read_cell(node, cursor->current_key, &cell);

	if( *cell.size != sizeof(charlie) )
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
	btree_node_destroy(tree, raw_root_node);
	page_destroy(pager, raw_page);
	btree_node_destroy(tree, node);
	remove("split_root_node.db");

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
	pager_cstd_create(&pager, cache, "test_free_heap.db");

	struct BTree* tree;
	btree_alloc(&tree);
	btree_init(tree, pager);

	char billy[] = "billy";
	char ruth[] = "ruth";
	btree_insert(tree, 12, billy, sizeof(billy));

	// Get the root node
	struct Page* raw_page;
	page_create(pager, tree->root_page_id, &raw_page);
	pager_read_page(pager, raw_page);

	struct BTreeNode* raw_node;
	btree_node_create_from_page(tree, &raw_node, raw_page);

	int free_heap_billy = raw_node->header->free_heap;

	btree_insert(tree, 1, ruth, sizeof(ruth));

	pager_read_page(pager, raw_page);
	btree_node_create_from_page(tree, &raw_node, raw_page);

	if( free_heap_billy - raw_node->header->free_heap !=
		sizeof(ruth) + sizeof(struct BTreePageKey) + sizeof(int) )
		result = 0;

end:
	btree_node_destroy(tree, raw_node);
	page_destroy(pager, raw_page);
	btree_deinit(tree);
	btree_dealloc(tree);
	remove("test_free_heap.db");
	return result;
}