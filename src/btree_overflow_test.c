#include "btree_overflow_test.h"

#include "btree.h"
#include "btree_alg.h"
#include "btree_cursor.h"
#include "btree_node.h"
#include "btree_node_debug.h"
#include "btree_node_reader.h"
#include "btree_node_writer.h"
#include "btree_utils.h"
#include "page_cache.h"
#include "pager_ops_cstd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int
btree_overflow_test_overflow_rw(void)
{
	char const* db_name = "overflow_rw.db";
	int result = 1;
	struct BTreeNode* node = NULL;
	struct PageSelector selector = {0};
	struct CellData cell;
	struct Page* page = NULL;
	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	remove(db_name);

	page_cache_create(&cache, 11);
	pager_cstd_create(&pager, cache, db_name, 512);

	char billy[512 * 4] =
		"billy_hello_how_are_you we are good thanks. Your name is what?"; // 12

	billy[1008] = 'r';
	billy[1716] = 'b';

	page_create(pager, &page);
	selector.page_id = 1;
	pager_read_page(pager, &selector, page);
	btree_node_create_from_page(&node, page);

	node->header->is_leaf = 1;
	pager_write_page(pager, page);

	// char* h = page->page_buffer;

	// for( int i = 0; i < (btu_get_node_storage_size(node) - 8); i++ )
	// {
	// 	h[pager->page_size - (i + 1)] = ('a' + i % ('z' - 'a'));
	// }

	btree_node_write(node, pager, 12, billy, sizeof(billy));

	char buf[sizeof(billy)] = {0};
	btree_node_read(node, pager, 12, buf, sizeof(buf));

	if( memcmp(buf, billy, sizeof(billy)) != 0 )
		result = 0;

	return 1;
}