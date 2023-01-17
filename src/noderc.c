#include "noderc.h"

#include "btree_node.h"
#include "btree_utils.h"

#include <string.h>

void
noderc_init(struct BTreeNodeRC* rcer, struct Pager* pager)
{
	rcer->pager = pager;
}

enum btree_e
noderc_acquire(struct BTreeNodeRC* rcer, struct NodeView* out_view)
{
	enum btree_e result;
	out_view->pager = rcer->pager;

	// Holding page and node is required to move cells up the tree.
	result = btpage_err(page_create(rcer->pager, &out_view->page));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_init_from_page(&out_view->node, out_view->page);
	if( result != BTREE_OK )
		goto end;

end:
	return result;
}

enum btree_e
noderc_acquire_n(struct BTreeNodeRC* rcer, struct NodeView* out_view, u32 num)
{
	enum btree_e result = BTREE_OK;

	for( int i = 0; i < num; i++ )
		memset(out_view + i, 0x00, sizeof(struct NodeView));

	for( int i = 0; i < num; i++ )
	{
		result = noderc_acquire(rcer, out_view + i);
		if( result != BTREE_OK )
			goto end;
	}
end:
	return result;
}

enum btree_e
noderc_acquire_load(
	struct BTreeNodeRC* rcer, struct NodeView* out_view, u32 page_id)
{
	enum btree_e result;
	out_view->pager = rcer->pager;

	// Holding page and node is required to move cells up the tree.
	result = btpage_err(page_create(rcer->pager, &out_view->page));
	if( result != BTREE_OK )
		goto end;

	result = btree_node_init_from_read(
		&out_view->node, out_view->page, rcer->pager, page_id);
	if( result != BTREE_OK )
		goto end;

end:
	return result;
}

void
noderc_release(struct BTreeNodeRC* rcer, struct NodeView* out_view)
{
	page_destroy(out_view->pager, out_view->page);

	out_view->page = NULL;
	out_view->pager = NULL;
	memset(&out_view->node, 0x00, sizeof(out_view->node));
}

struct BTreeNode*
nodeview_node(struct NodeView* view)
{
	return &view->node;
}

struct Page*
nodeview_page(struct NodeView* view)
{
	return view->page;
}

struct Pager*
nodeview_pager(struct NodeView* view)
{
	return view->pager;
}