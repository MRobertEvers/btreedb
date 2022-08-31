#include "btree_test.h"

#include "btree.h"
#include "page_cache.h"
#include "pager_ops_cstd.h"

#include <stdio.h>
#include <string.h>
int
btree_test_insert_root_with_space()
{
	int result = 1;
	struct Pager* pager;

	struct PageCache* cache = NULL;
	page_cache_create(&cache, 5);
	pager_cstd_new(&pager, cache, "test_.db");

	struct BTree* tree;
	btree_alloc(&tree);
	btree_init(tree, pager);

	char billy[] = "billy";
	char ruth[] = "ruth";
	btree_insert(tree, 12, billy, sizeof(billy));
	btree_insert(tree, 1, ruth, sizeof(ruth));

	// Hack
	pager_read_page(pager, tree->root->page);

	if( tree->root->header->num_keys != 2 )
	{
		result = 0;
		goto end;
	}

	if( tree->root->keys[0].key != 1 || tree->root->keys[1].key != 12 )
	{
		result = 0;
		goto end;
	}

end:
	btree_deinit(tree);
	btree_dealloc(tree);
	remove("test_.db");
	return result;
}

int
btree_test_split_root_node()
{
	int result = 1;
	struct Page* page = 0;
	struct BTreeNode* node = 0;

	struct Pager* pager;
	struct PageCache* cache = NULL;
	page_cache_create(&cache, 5);
	pager_cstd_new(&pager, NULL, "split_root_node.db");

	struct BTree* tree;
	btree_alloc(&tree);
	btree_init(tree, pager);

	char billy[] = "billy";
	char ruth[] = "ruth";
	char charlie[] = "charlie";
	btree_insert(tree, 12, billy, sizeof(billy));
	btree_insert(tree, 2, charlie, sizeof(charlie));
	btree_insert(tree, 1, ruth, sizeof(ruth));

	split_node(tree, tree->root);

	pager_read_page(tree->pager, tree->root->page);
	btree_node_init_from_page(tree, tree->root, tree->root->page);

	struct Cursor* cursor = create_cursor(tree);

	char found = 0;
	btree_traverse_to(cursor, 2, &found);

	if( !found )
		return 0;

	page_create(tree->pager, cursor->current_page_id, &page);
	pager_read_page(tree->pager, page);
	btree_node_create_from_page(tree, &node, page);

	if( node->header->num_keys != 2 )
		return 0;

	struct CellData cell;
	read_cell(node, cursor->current_key, &cell);

	if( *cell.size != sizeof(charlie) )
		return 0;

	if( memcmp(cell.pointer, charlie, sizeof(charlie)) != 0 )
		return 0;

	btree_node_destroy(tree, node);
	page_destroy(tree->pager, page);
	remove("split_root_node.db");

	return 1;
}