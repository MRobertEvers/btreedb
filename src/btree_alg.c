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
	struct BTreeNode* node,
	struct Pager* pager,
	struct SplitPageAsParent* split_page)
{
	struct BTreeNode* parent = NULL;
	struct BTreeNode* left = NULL;
	struct BTreeNode* right = NULL;

	struct Page* parent_page = NULL;
	struct Page* left_page = NULL;
	struct Page* right_page = NULL;

	page_create(pager, &parent_page);
	btree_node_create_as_page_number(&parent, node->page_number, parent_page);

	page_create(pager, &left_page);
	btree_node_create_from_page(&left, left_page);

	page_create(pager, &right_page);
	btree_node_create_from_page(&right, right_page);

	struct KeyListIndex insert_end = {.mode = KLIM_END};
	int first_half = ((node->header->num_keys + 1) / 2);
	// We need to keep track of this. If this is a nonleaf node,
	// then the left child high key will be lost.
	unsigned int left_child_high_key = 0;
	for( int i = 0; i < node->header->num_keys; i++ )
	{
		struct CellData cell = {0};
		insert_end.mode = KLIM_END;

		int key = node->keys[i].key;
		btu_read_cell(node, i, &cell);
		if( i < first_half )
		{
			// The last element in the first half should be the right child.
			insert_end.mode = (i == first_half - 1 && !node->header->is_leaf)
								  ? KLIM_RIGHT_CHILD
								  : KLIM_END;
			// We must track this regardless of whether leaf or not.
			if( i == first_half - 1 )
				left_child_high_key = key;
			btree_node_insert(left, &insert_end, key, cell.pointer, *cell.size);
		}
		else
		{
			btree_node_insert(
				right, &insert_end, key, cell.pointer, *cell.size);
		}
	}

	// Non-leaf nodes also have to move right child too.
	if( !node->header->is_leaf )
	{
		insert_end.mode = KLIM_RIGHT_CHILD;
		btree_node_insert(
			right,
			&insert_end,
			node->header->right_child,
			(void*)&node->header->right_child,
			sizeof(node->header->right_child));
	}

	// We need to write the pages out to get the page ids.
	left->header->is_leaf = 1;
	pager_write_page(pager, left_page);

	right->header->is_leaf = 1;
	pager_write_page(pager, right_page);

	// When splitting a leaf-node,
	// the right_child pointer becomes the right_page id
	// When splitting a non-leaf node
	// The right_child pointer becomes the right_page id
	// and the old right_child pointer becomes the right_child pointer
	// of the right page.
	parent->header->is_leaf = 0;

	parent->header->right_child = right_page->page_id;
	insert_end.mode = KLIM_END;
	// Add the middle point between the left and right pages as a key to the
	// parent.
	btree_node_insert(
		parent,
		&insert_end,
		left_child_high_key,
		&left_page->page_id,
		sizeof(unsigned int));

	memcpy(
		btu_get_node_buffer(node),
		btu_get_node_buffer(parent),
		btu_get_node_size(parent));

	pager_write_page(pager, node->page);

	if( split_page != NULL )
	{
		split_page->left_child_page_id = left_page->page_id;
		split_page->right_child_page_id = right_page->page_id;
		split_page->left_child_high_key = left_child_high_key;
	}

end:
	btree_node_destroy(left);
	page_destroy(pager, left_page);

	btree_node_destroy(right);
	page_destroy(pager, right_page);

	btree_node_destroy(parent);
	page_destroy(pager, parent_page);

	return BTREE_OK;
}

/**
 * See header for details.
 */
enum btree_e
bta_split_node(
	struct BTreeNode* node, struct Pager* pager, struct SplitPage* split_page)
{
	struct BTreeNode* left = NULL;
	struct BTreeNode* right = NULL;

	struct Page* left_page = NULL;
	struct Page* right_page = NULL;

	// We want right page to remain stable since pointers
	// in parent nodes are already pointing to the high-key of the input
	// node which becomes the high key of the right child.
	page_create(pager, &right_page);
	btree_node_create_as_page_number(&right, node->page_number, right_page);

	page_create(pager, &left_page);
	btree_node_create_from_page(&left, left_page);

	struct KeyListIndex insert_end = {.mode = KLIM_END};
	int first_half = ((node->header->num_keys + 1) / 2);

	// We need to keep track of this. If this is a nonleaf node,
	// then the left child high key will be lost.
	unsigned int left_child_high_key = 0;
	for( int i = 0; i < node->header->num_keys; i++ )
	{
		struct CellData cell = {0};
		insert_end.mode = KLIM_END;

		int key = node->keys[i].key;
		btu_read_cell(node, i, &cell);
		if( i < first_half )
		{
			// The last element in the first half should be the right child.
			insert_end.mode = (i == first_half - 1 && !node->header->is_leaf)
								  ? KLIM_RIGHT_CHILD
								  : KLIM_END;
			if( i == first_half - 1 )
				left_child_high_key = key;
			btree_node_insert(left, &insert_end, key, cell.pointer, *cell.size);
		}
		else
		{
			btree_node_insert(
				right, &insert_end, key, cell.pointer, *cell.size);
		}
	}

	if( !node->header->is_leaf )
	{
		insert_end.mode = KLIM_RIGHT_CHILD;
		btree_node_insert(
			right,
			&insert_end,
			node->header->right_child,
			(void*)&node->header->right_child,
			sizeof(node->header->right_child));
	}

	right->header->is_leaf = node->header->is_leaf;
	left->header->is_leaf = node->header->is_leaf;
	// We need to write the pages out to get the page ids.
	pager_write_page(pager, right_page);
	// Write out the input page.
	pager_write_page(pager, left_page);

	memcpy(
		btu_get_node_buffer(node),
		btu_get_node_buffer(right),
		btu_get_node_size(right));
	node->page_number = right_page->page_id;

	if( split_page != NULL )
	{
		split_page->left_page_id = left_page->page_id;
		split_page->left_page_high_key = left_child_high_key;
	}

end:
	btree_node_destroy(left);
	page_destroy(pager, left_page);

	btree_node_destroy(right);
	page_destroy(pager, right_page);

	return BTREE_OK;
}