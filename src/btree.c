#include "btree.h"

#include "btree_alg.h"
#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_node_reader.h"
#include "btree_node_writer.h"
#include "btree_utils.h"
#include "noderc.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Initializes the first page of a file as the root page.
 *
 * page->page_id must be 1.
 *
 * @param tree
 * @param page
 * @return enum btree_e
 */
static enum btree_e
init_new_root_page(struct BTree* tree, struct Page* page, u32 page_id)
{
	struct BTreeNode temp_node = {0};
	enum btree_e result = BTREE_OK;

	page->page_id = page_id;
	result = btree_node_init_from_page(&temp_node, page);
	if( result != BTREE_OK )
		return result;

	node_is_leaf_set(&temp_node, true);

	return BTREE_OK;
}

static enum btree_e
btree_init_root_page(struct BTree* tree, struct Page* page, u32 page_id)
{
	enum btree_e result = BTREE_OK;
	enum pager_e pager_status = PAGER_OK;
	struct PageSelector selector;
	pager_reselect(&selector, page_id);

	pager_status = pager_read_page(tree->pager, &selector, page);
	if( pager_status == PAGER_ERR_NIF )
	{
		result = init_new_root_page(tree, page, page_id);
		if( result != BTREE_OK )
			return result;

		result = btpage_err(pager_write_page(tree->pager, page));

		return result;
	}
	else if( pager_status != PAGER_OK )
	{
		return BTREE_ERR_PAGING;
	}
	else
	{
		return BTREE_OK;
	}
}

enum btree_e
btree_alloc(struct BTree** r_tree)
{
	*r_tree = (struct BTree*)malloc(sizeof(struct BTree));
	return BTREE_OK;
}

enum btree_e
btree_dealloc(struct BTree* tree)
{
	free(tree);
	return BTREE_OK;
}

u32
btree_min_page_size(void)
{
	u32 min_heap_required_for_largest_cell_type =
		btree_node_heap_required_for_insertion(
			btree_cell_overflow_min_disk_size());
	// Minimum size to fit 4 overflow cells on the root page.
	return (
		(BTREE_HEADER_SIZE) + (4 * min_heap_required_for_largest_cell_type) +
		+sizeof(struct BTreePageHeader));
}

/**
 * @brief Load the root page into memory and read out information from the btree
 * header.
 *
 * @param tree
 * @return enum btree_e
 */
enum btree_e
btree_init(
	struct BTree* tree,
	struct Pager* pager,
	struct BTreeNodeRC* rcer,
	u32 root_page_id)
{
	enum btree_e btree_result = BTREE_OK;

	tree->type = BTREE_TBL;

	tree->compare = &btree_compare;
	tree->reset_compare = &btree_compare_reset;
	tree->keyof = &btree_keyof;

	// Arbitrary 4*12 is approximately 4 entries that overflow.
	// 1 int for cell size, 2 more for overflow page meta.
	if( pager->page_size < btree_min_page_size() )
		return BTREE_ERR_INVALID_PAGE_SIZE_TOO_SMALL;

	tree->root_page_id = root_page_id;
	tree->pager = pager;
	tree->rcer = rcer;

	struct Page* page = NULL;
	btree_result = btpage_err(page_create(tree->pager, &page));
	if( btree_result != BTREE_OK )
		goto end;

	btree_result = btree_init_root_page(tree, page, root_page_id);
	if( btree_result != BTREE_OK )
		goto end;

end:
	if( page )
		page_destroy(tree->pager, page);
	return btree_result;
}

enum btree_e
btree_deinit(struct BTree* tree)
{
	return BTREE_OK;
}

int
btree_compare(
	void* compare_context,
	void* cmp_window,
	u32 cmp_window_size,
	u32 cmp_total_size,
	void* right,
	u32 right_size,
	u32 bytes_compared,
	u32* out_bytes_compared,
	u32* out_key_size_remaining)
{
	// Assumes key fits in entire window.
	// Assumes key is a pointer to u32
	assert(cmp_window_size == sizeof(u32));
	u32 left_val = *(u32*)cmp_window;
	u32 right_val = *(u32*)right;

	*out_bytes_compared = cmp_window_size;
	*out_key_size_remaining = 0;

	if( left_val == right_val )
		return 0;
	else
		return left_val < right_val ? -1 : 1;
}

void
btree_compare_reset(void* compare_context)
{}

byte*
btree_keyof(
	void* compare_context,
	struct BTreeNode* node,
	u32 index,
	u32* out_size,
	u32* out_total_size,
	u32* out_follow_page)
{
	*out_size = sizeof(node->keys[index].key);
	*out_total_size = sizeof(node->keys[index].key);
	*out_follow_page = 0;
	return (byte*)&node->keys[index].key;
}

enum btree_e
btree_insert(struct BTree* tree, int key, void* data, int data_size)
{
	enum btree_e result = BTREE_OK;
	char found = 0;
	struct NodeView nv = {0};
	struct CursorBreadcrumb crumb = {0};

	struct Cursor* cursor = cursor_create(tree);

	result = noderc_acquire(tree->rcer, &nv);
	if( result != BTREE_OK )
		goto end;

	result = cursor_traverse_to(cursor, key, &found);
	if( result != BTREE_OK )
		goto end;

	while( 1 )
	{
		result = cursor_pop(cursor, &crumb);
		if( result != BTREE_OK )
			goto end;

		result = noderc_reinit_read(tree->rcer, &nv, crumb.page_id);
		if( result != BTREE_OK )
			goto end;

		result =
			btree_node_write(nv_node(&nv), tree->pager, key, data, data_size);
		if( result == BTREE_ERR_NODE_NOT_ENOUGH_SPACE )
		{
			// We want to keep the root node as the first page.
			// So if the first page is to be split, then split
			// the data in this page between two children nodes.
			// This node becomes the new parent of those nodes.
			if( nv_page(&nv)->page_id == tree->root_page_id )
			{
				struct SplitPageAsParent split_result;
				result = bta_split_node_as_parent(
					nv_node(&nv), tree->rcer, &split_result);
				if( result != BTREE_OK )
					goto end;

				result = noderc_reinit_read(
					tree->rcer,
					&nv,
					key <= split_result.left_child_high_key
						? split_result.left_child_page_id
						: split_result.right_child_page_id);
				if( result != BTREE_OK )
					goto end;

				result = btree_node_write(
					nv_node(&nv), tree->pager, key, data, data_size);
				if( result != BTREE_OK )
					goto end;

				goto end;
			}
			else
			{
				struct SplitPage split_result;
				result =
					bta_split_node(nv_node(&nv), tree->rcer, &split_result);
				if( result != BTREE_OK )
					goto end;

				// TODO: Key compare function.
				if( key <= split_result.left_page_high_key )
				{
					result = noderc_reinit_read(
						tree->rcer, &nv, split_result.left_page_id);
					if( result != BTREE_OK )
						goto end;
				}

				result = btree_node_write(
					nv_node(&nv), tree->pager, key, data, data_size);
				if( result != BTREE_OK )
					goto end;

				// Move the cursor one to the left so we can insert there.
				result = cursor_pop(cursor, &crumb);
				if( result != BTREE_OK )
					goto end;

				// TODO: Better way to do this?
				// Basically we have to insert the left child to the left of the
				// node that was split.
				//
				// It is expected that the index of a key with KLIM_RIGHT_CHILD
				// is the num keys on that page.
				// cursor->current_key_index.index;

				crumb.key_index.mode = KLIM_INDEX;
				result = cursor_push_crumb(cursor, &crumb);
				if( result != BTREE_OK )
					goto end;

				// Args for next iteration
				// Insert the new child's page number into the parent.
				// The parent pointer is already the key of the highest
				// key in the right page, so that can stay the same.
				// Insert the highest key of the left page.
				//    K5              K<5   K5
				//     |   ---->      |     |
				//    K5              K<5   K5
				//
				key = split_result.left_page_high_key;
				data = (void*)&split_result.left_page_id;
				data_size = sizeof(split_result.left_page_id);
			}
		}
		else
		{
			break;
		}
	}

end:
	cursor_destroy(cursor);
	noderc_release(tree->rcer, &nv);
	return BTREE_OK;
}

static enum btree_e
delete_single(struct Cursor* cursor, int key)
{
	enum btree_e result = BTREE_OK;
	char found = 0;
	struct NodeView nv = {0};
	bool underflow = false;
	struct CursorBreadcrumb crumb = {0};

	result = cursor_traverse_to(cursor, key, &found);
	if( result != BTREE_OK )
		goto end;

	// TODO: Error?
	if( !found )
		goto end;

	result = cursor_peek(cursor, &crumb);
	if( result != BTREE_OK )
		goto end;

	result = noderc_acquire_load(cursor_rcer(cursor), &nv, crumb.page_id);
	if( result != BTREE_OK )
		goto end;

	result = btree_node_remove(nv_node(&nv), &crumb.key_index, NULL, NULL, 0);
	if( result != BTREE_OK )
		goto end;

	result = noderc_persist_n(cursor_rcer(cursor), 1, &nv);
	if( result != BTREE_OK )
		goto end;

	underflow = node_num_keys(nv_node(&nv)) == 0;

end:
	noderc_release(cursor_rcer(cursor), &nv);

	if( underflow && result == BTREE_OK )
		result = BTREE_ERR_UNDERFLOW;

	return result;
}

enum btree_e
btree_delete(struct BTree* tree, int key)
{
	enum btree_e result = BTREE_OK;

	struct Cursor* cursor = cursor_create(tree);
	result = delete_single(cursor, key);

	if( result == BTREE_ERR_UNDERFLOW )
	{
		result = bta_rebalance(cursor);
		if( result != BTREE_OK )
			goto end;
	}

end:
	cursor_destroy(cursor);

	return result;
}

enum btree_e
btree_select_ex(struct BTree* tree, u32 key, void* buffer, u32 buffer_size)
{
	enum btree_e result = BTREE_OK;
	char found;
	struct NodeView nv = {0};
	struct Cursor* cursor = cursor_create(tree);

	result = noderc_acquire(cursor_rcer(cursor), &nv);
	if( result != BTREE_OK )
		goto end;

	result = cursor_traverse_to(cursor, key, &found);
	if( result != BTREE_OK )
		goto end;

	if( !found )
	{
		result = BTREE_ERR_KEY_NOT_FOUND;
		goto end;
	}

	result = cursor_read_current(cursor, &nv);
	if( result != BTREE_OK )
		goto end;

	assert(cursor->current_key_index.index != node_num_keys(nv_node(&nv)));

	u32 ind = cursor->current_key_index.index;
	result = btree_node_read_at(tree, nv_node(&nv), ind, buffer, buffer_size);
	if( result != BTREE_OK )
		goto end;

end:
	noderc_release(cursor_rcer(cursor), &nv);

	if( cursor )
		cursor_destroy(cursor);

	return result;
}