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
btree_alg_test_split_nonleaf(void)
{
	char const* db_name = "btree_test_alg_split.db";
	int result = 1;
	struct Pager* pager;

	struct PageSelector selector = {0};
	struct PageCache* cache = NULL;

	remove(db_name);

	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, db_name, 0x1000);

	struct BTreeNode* node = {0};
	struct Page* page = NULL;
	struct ChildListIndex inserter = {.mode = KLIM_END};

	page_create(pager, &page);

	btree_node_create_as_page_number(&node, 1, page);
	node->header->is_leaf = 0;

	unsigned int fake_child_page_id_as_key = 3;
	inserter.mode = KLIM_END;
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
	inserter.mode = KLIM_RIGHT_CHILD;
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

	remove(db_name);

	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, db_name, 0x1000);

	struct BTreeNode* node = {0};
	struct Page* page = NULL;
	struct ChildListIndex inserter = {.mode = KLIM_END};

	page_create(pager, &page);

	btree_node_create_as_page_number(&node, 1, page);
	node->header->is_leaf = 1;

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

	remove(db_name);

	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, db_name, 0x1000);

	struct BTreeNode* node = {0};
	struct Page* page = NULL;
	struct ChildListIndex inserter = {.mode = KLIM_END};

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
	inserter.mode = KLIM_RIGHT_CHILD;
	btree_node_insert(
		node,
		&inserter,
		fake_child_page_id_as_key,
		(void*)&fake_child_page_id_as_key,
		sizeof(fake_child_page_id_as_key));

	pager_write_page(pager, node->page);

	struct SplitPageAsParent split_result = {0};
	bta_split_node_as_parent(node, pager, &split_result);

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
btree_alg_test_split_as_parent_leaf(void)
{
	char const* db_name = "bta_split_p_leaf.db";
	int result = 1;
	struct Pager* pager;

	struct PageSelector selector = {0};
	struct PageCache* cache = NULL;

	remove(db_name);

	page_cache_create(&cache, 5);
	pager_cstd_create(&pager, cache, db_name, 0x1000);

	struct BTreeNode* node = {0};
	struct Page* page = NULL;
	struct ChildListIndex inserter = {.mode = KLIM_END};

	page_create(pager, &page);

	btree_node_create_as_page_number(&node, 1, page);
	node->header->is_leaf = 1;

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

	pager_write_page(pager, node->page);

	struct SplitPageAsParent split_result = {0};
	bta_split_node_as_parent(node, pager, &split_result);

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