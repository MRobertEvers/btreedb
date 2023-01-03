#include "btree_alg_test.h"

#include "btree.h"
#include "btree_alg.h"
#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_utils.h"
#include "page_cache.h"
#include "pager_ops_cstd.h"

#include <stdio.h>
#include <string.h>

int
btree_alg_test_split(void)
{
	char const* db_name = "btree_test_alg_split.db";
	int result = 1;
	struct Pager* pager;

	struct PageSelector selector = {0};
	struct PageCache* cache = NULL;

	remove(db_name);

	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, db_name);

	struct BTreeNode* node = {0};
	struct Page* page = NULL;
	struct KeyListIndex inserter = {.mode = KLIM_RIGHT_CHILD};

	page_create(pager, &page);

	btree_node_create_as_page_number(&node, 1, page);
	node->header->is_leaf = 0;

	unsigned int fake_child_page_id_as_key = 3;
	btree_node_insert(
		node,
		&inserter,
		fake_child_page_id_as_key,
		(void*)&fake_child_page_id_as_key,
		sizeof(fake_child_page_id_as_key));
	fake_child_page_id_as_key = 4;
	btree_node_insert(
		node,
		&inserter,
		fake_child_page_id_as_key,
		(void*)&fake_child_page_id_as_key,
		sizeof(fake_child_page_id_as_key));
	fake_child_page_id_as_key = 5;
	btree_node_insert(
		node,
		&inserter,
		fake_child_page_id_as_key,
		(void*)&fake_child_page_id_as_key,
		sizeof(fake_child_page_id_as_key));
	fake_child_page_id_as_key = 6;
	btree_node_insert(
		node,
		&inserter,
		fake_child_page_id_as_key,
		(void*)&fake_child_page_id_as_key,
		sizeof(fake_child_page_id_as_key));

	struct SplitPage split_result = {0};
	bta_split_node(node, pager, &split_result);

	struct BTreeNode* other_node = {0};
	struct Page* other_page = NULL;
	page_create(pager, &other_page);

	selector.page_id = split_result.right_page_id;
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

	if( other_node->header->right_child != 6 || node->header->right_child != 4 )
	{
		result = 0;
		goto end;
	}

end:
	// 	btree_node_destroy(raw_node);
	page_destroy(pager, page);
	page_destroy(pager, other_page);
	// 	btree_deinit(tree);
	// 	btree_dealloc(tree);
	remove(db_name);
	return result;
}

int
btree_alg_test_split_as_parent(void)
{}