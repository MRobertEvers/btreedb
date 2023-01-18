#include "noderc.h"

#include "btree_node.h"
#include "btree_utils.h"

#include <stdarg.h>
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
noderc_acquire_load_n(struct BTreeNodeRC* rcer, u32 num, ...)
{
	enum btree_e result = BTREE_OK;
	va_list argp;

	va_start(argp, num);
	for( int i = 0; i < num; i++ )
	{
		struct NodeView* node = va_arg(argp, struct NodeView*);
		va_arg(argp, u32);
		memset(node, 0x00, sizeof(struct NodeView));
	}
	va_end(argp);

	va_start(argp, num);
	for( int i = 0; i < num; i++ )
	{
		struct NodeView* node = va_arg(argp, struct NodeView*);
		u32 page = va_arg(argp, u32);

		if( page == 0 )
		{
			result = noderc_acquire(rcer, node);
			if( result != BTREE_OK )
				goto end;
		}
		else
		{
			result = noderc_acquire_load(rcer, node, page);
			if( result != BTREE_OK )
				goto end;
		}
	}
	va_end(argp);

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

enum btree_e
noderc_persist_n(struct BTreeNodeRC* rcer, u32 num, ...)
{
	enum btree_e result = BTREE_OK;
	va_list argp;

	va_start(argp, num);
	for( int i = 0; i < num; i++ )
	{
		struct NodeView* node = va_arg(argp, struct NodeView*);
		result = btpage_err(pager_write_page(node->pager, node->page));
		if( result != BTREE_OK )
			goto end;
	}
	va_end(argp);

end:
	return result;
}

void
noderc_release(struct BTreeNodeRC* rcer, struct NodeView* out_view)
{
	if( out_view->page )
		page_destroy(out_view->pager, out_view->page);

	out_view->page = NULL;
	out_view->pager = NULL;
	memset(&out_view->node, 0x00, sizeof(out_view->node));
}

void
noderc_release_n(struct BTreeNodeRC* rcer, u32 num, ...)
{
	enum btree_e result = BTREE_OK;
	va_list argp;

	va_start(argp, num);

	for( int i = 0; i < num; i++ )
	{
		struct NodeView* node = va_arg(argp, struct NodeView*);
		noderc_release(rcer, node);
	}

	va_end(argp);
}

enum btree_e
noderc_reinit_read(
	struct BTreeNodeRC* rcer, struct NodeView* out_view, u32 page_id)
{
	enum btree_e result;

	result = btree_node_init_from_read(
		&out_view->node, out_view->page, rcer->pager, page_id);
	if( result != BTREE_OK )
		goto end;

end:
	return result;
}

enum btree_e
noderc_reinit_as(
	struct BTreeNodeRC* rcer, struct NodeView* out_view, u32 page_id)
{
	enum btree_e result;

	result = btree_node_init_as_page_number(
		&out_view->node, page_id, out_view->page);
	if( result != BTREE_OK )
		goto end;

end:
	return result;
}

struct BTreeNode*
nv_node(struct NodeView* view)
{
	return &view->node;
}

struct Page*
nv_page(struct NodeView* view)
{
	return view->page;
}

struct Pager*
nv_pager(struct NodeView* view)
{
	return view->pager;
}