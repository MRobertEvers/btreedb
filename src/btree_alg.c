#include "btree_alg.h"

#include "btree_node.h"
#include "btree_utils.h"
#include "pager.h"

#include <stdlib.h>
#include <string.h>

/**
 * See header for details.
 */
enum btree_e
bta_split_node_as_parent(
	struct BTree* tree,
	struct BTreeNode* node,
	struct SplitPageAsParent* split_page)
{
	struct BTreeNode* parent = NULL;
	struct BTreeNode* left = NULL;
	struct BTreeNode* right = NULL;

	struct Page* parent_page = NULL;
	struct Page* left_page = NULL;
	struct Page* right_page = NULL;

	page_create(tree->pager, &parent_page);
	btree_node_create_as_page_number(&parent, node->page_number, parent_page);

	page_create(tree->pager, &left_page);
	btree_node_create_from_page(&left, left_page);

	page_create(tree->pager, &right_page);
	btree_node_create_from_page(&right, right_page);

	int first_half = ((node->header->num_keys + 1) / 2);
	for( int i = 0; i < node->header->num_keys; i++ )
	{
		struct CellData cell = {0};
		int key = node->keys[i].key;
		btu_read_cell(node, i, &cell);
		if( i < first_half )
		{
			btree_node_insert(left, i, key, cell.pointer, *cell.size);
		}
		else
		{
			btree_node_insert(
				right, (i - first_half), key, cell.pointer, *cell.size);
		}
	}

	// We need to write the pages out to get the page ids.
	right->header->is_leaf = 1;
	pager_write_page(tree->pager, right_page);

	left->header->is_leaf = 1;
	pager_write_page(tree->pager, left_page);

	btree_node_insert(
		parent,
		0,
		left->keys[left->header->num_keys - 1].key,
		&left_page->page_id,
		sizeof(unsigned int));

	parent->header->is_leaf = 0;
	parent->header->right_child = right_page->page_id;

	memcpy(
		btu_get_node_buffer(node),
		btu_get_node_buffer(parent),
		btu_get_node_size(parent));

	pager_write_page(tree->pager, node->page);

	if( split_page != NULL )
	{
		split_page->left_child_page_id = left_page->page_id;
		split_page->right_child_page_id = right_page->page_id;
		split_page->right_child_low_key = right->keys[0].key;
	}

end:
	btree_node_destroy(left);
	page_destroy(tree->pager, left_page);

	btree_node_destroy(right);
	page_destroy(tree->pager, right_page);

	btree_node_destroy(parent);
	page_destroy(tree->pager, parent_page);

	return BTREE_OK;
}

/**
 * See header for details.
 */
enum btree_e
bta_split_node(
	struct BTree* tree, struct BTreeNode* node, struct SplitPage* split_page)
{
	struct BTreeNode* left = NULL;
	struct BTreeNode* right = NULL;

	struct Page* left_page = NULL;
	struct Page* right_page = NULL;

	page_create(tree->pager, &left_page);
	btree_node_create_as_page_number(&left, node->page_number, left_page);

	page_create(tree->pager, &right_page);
	btree_node_create_from_page(&right, right_page);

	int first_half = ((node->header->num_keys + 1) / 2);
	for( int i = 0; i < node->header->num_keys; i++ )
	{
		struct CellData cell = {0};
		int key = node->keys[i].key;
		btu_read_cell(node, i, &cell);
		if( i < first_half )
		{
			btree_node_insert(left, i, key, cell.pointer, *cell.size);
		}
		else
		{
			btree_node_insert(
				right, (i - first_half), key, cell.pointer, *cell.size);
		}
	}

	right->header->is_leaf = node->header->is_leaf;
	left->header->is_leaf = node->header->is_leaf;
	// Write out the input page.
	pager_write_page(tree->pager, left_page);
	// We need to write the pages out to get the page ids.
	pager_write_page(tree->pager, right_page);

	memcpy(
		btu_get_node_buffer(node),
		btu_get_node_buffer(left),
		btu_get_node_size(left));
	node->page_number = left_page->page_id;

	if( split_page != NULL )
	{
		split_page->right_page_id = right_page->page_id;
		split_page->right_page_low_key = right->keys[0].key;
	}

end:
	btree_node_destroy(left);
	page_destroy(tree->pager, left_page);

	btree_node_destroy(right);
	page_destroy(tree->pager, right_page);

	return BTREE_OK;
}