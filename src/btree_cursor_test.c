#include "btree_cursor_test.h"

#include "btree.h"
#include "btree_cursor.h"
#include "btree_defs.h"
#include "btree_node.h"
#include "btree_node_debug.h"
#include "btree_node_reader.h"
#include "btree_node_writer.h"
#include "ibtree.h"
#include "ibtree_alg.h"
#include "noderc.h"
#include "pager.h"
#include "pager_ops_cstd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct TestRecord
{
	unsigned int key;
	void* data;
	unsigned int size;
};

int
cursor_iter_ibtree_test(void)
{
	char const* db_name = "iter_test.db";
	int result = 1;
	struct NodeView nv = {0};
	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	remove(db_name);

	page_cache_create(&cache, 11);
	u32 page_size = pager_disk_page_size_for(btree_min_page_size() + 1 * 4);
	pager_cstd_create(&pager, cache, db_name, page_size);
	struct BTreeNodeRC rcer;
	noderc_init(&rcer, pager);
	struct BTree* tree;
	btree_alloc(&tree);
	enum btree_e btresult = ibtree_init(
		tree, pager, &rcer, 1, &ibtree_compare, &ibtree_compare_reset);
	if( btresult != BTREE_OK )
	{
		result = 0;
		goto end;
	}

	char billy[] = "billy_"
				   "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVW"
				   "XYZ";
	char ruth[40] = "ruth";
	char yuta[100] = "999yutaaa";
	char charlie[40] = "charlie";
	char buxley[40] = "buxley";
	char herman[40] = "herman_abcdefghijklmnopqrstuvwxyz";
	char flaur[100] =
		"flaur_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char flemming[100] =
		"flemming_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char xiao[100] = "xiao";
	char gilly[100] = "gilly";
	char brock[100] = "brock";
	char sloan[100] = "sloan";
	char ryan[100] = "ryan";
	char jacob[100] = "jacob";
	char megan[100] = "megan";
	char donata[100] = "donata";
	char honda[100] = "honda";
	char xondley[100] = "xondley";
	char perkins[100] = "perkins";
	char rick[100] = "rick";
	char woally[100] = "woally";
	char hoosin[100] = "hoosin";
	char sybian[100] = "sybian";
	char trillian[100] = "trillian";
	char coxes[100] = "coxes";

	struct TestRecord records[] = {
		{.key = 1, .data = billy, .size = sizeof(billy)},		 // 1
		{.key = 2, .data = ruth, .size = sizeof(ruth)},			 // 2
		{.key = 3, .data = yuta, .size = sizeof(yuta)},			 // 3
		{.key = 8, .data = charlie, .size = sizeof(charlie)},	 // 4
		{.key = 5, .data = buxley, .size = sizeof(buxley)},		 // 5
		{.key = 9, .data = herman, .size = sizeof(herman)},		 // 6
		{.key = 25, .data = flaur, .size = sizeof(flaur)},		 // 7
		{.key = 23, .data = flemming, .size = sizeof(flemming)}, // 8
		{.key = 4, .data = xiao, .size = sizeof(xiao)},			 // 9
		{.key = 13, .data = gilly, .size = sizeof(billy)},		 // 10
		{.key = 40, .data = brock, .size = sizeof(brock)},		 // 11
		{.key = 41, .data = sloan, .size = sizeof(sloan)},		 // 12
		{.key = 42, .data = ryan, .size = sizeof(ryan)},		 // 13
		{.key = 35, .data = jacob, .size = sizeof(jacob)},		 // 14
		{.key = 32, .data = megan, .size = sizeof(megan)},		 // 15
		{.key = 67, .data = donata, .size = sizeof(donata)},	 // 16
		{.key = 22, .data = honda, .size = sizeof(honda)},		 // 17
		{.key = 21, .data = xondley, .size = sizeof(xondley)},	 // 18
		{.key = 19, .data = perkins, .size = sizeof(perkins)},	 // 19
		{.key = 10, .data = rick, .size = sizeof(rick)},		 // 20
		{.key = 11, .data = woally, .size = sizeof(woally)},	 // 21
		{.key = 12, .data = hoosin, .size = sizeof(hoosin)},	 // 22
		{.key = 17, .data = sybian, .size = sizeof(sybian)},	 // 23
		{.key = 16, .data = trillian, .size = sizeof(trillian)}, // 24
		{.key = 55, .data = coxes, .size = sizeof(coxes)},		 // 25
	};

	/**
	 * Ensure that we can still reach certain keys
	 * after inserting others.
	 */
	struct Cursor* cursor = NULL;
	char found = 0;
	for( int i = 0; i < sizeof(records) / sizeof(records[0]); i++ )
	{
		struct TestRecord* test = &records[i];
		ibtree_insert(tree, test->data, test->size);
	}

	char buf2[120] = {0};
	char buf1[120] = {0};
	char* next_buf = buf1;
	char* prev_buf = NULL;
	cursor = cursor_create(tree);
	noderc_acquire(cursor_rcer(cursor), &nv);

	btresult = cursor_iter_begin(cursor);
	while( btresult == BTREE_OK )
	{
		if( nv.page->page_id != cursor->current_page_id )
			noderc_reinit_read(
				cursor_rcer(cursor), &nv, cursor->current_page_id);

		memset(next_buf, 0x00, sizeof(buf1));
		btree_node_read_at(
			tree,
			nv_node(&nv),
			cursor->current_key_index.index,
			next_buf,
			sizeof(buf1));
		// dbg_print_buffer(buf, strnlen(buf, sizeof(buf)));

		if( prev_buf != NULL && memcmp(prev_buf, next_buf, sizeof(buf1)) >= 0 )
			goto fail;

		prev_buf = next_buf;
		next_buf = next_buf == buf1 ? buf2 : buf1;

		btresult = cursor_iter_next(cursor);
	}

end:
	noderc_release(cursor_rcer(cursor), &nv);
	remove(db_name);

	return result;
fail:
	result = 0;
	goto end;
}

int
cursor_iter_btree_test(void)
{
	char const* db_name = "iter_test2.db";
	int result = 1;
	struct NodeView nv = {0};
	struct Pager* pager = NULL;
	struct PageCache* cache = NULL;

	remove(db_name);

	page_cache_create(&cache, 11);
	u32 page_size = pager_disk_page_size_for(btree_min_page_size() + 1 * 4);
	pager_cstd_create(&pager, cache, db_name, page_size);
	struct BTreeNodeRC rcer;
	noderc_init(&rcer, pager);
	struct BTree* tree;
	btree_alloc(&tree);
	enum btree_e btresult = btree_init(tree, pager, &rcer, 1);
	if( btresult != BTREE_OK )
	{
		result = 0;
		goto end;
	}

	char billy[] = "billy_"
				   "abcdefghijklmnopqrstuvwxyz0123456789ABCDEFGHIJKLMNOPQRSTUVW"
				   "XYZ";
	char ruth[40] = "ruth";
	char yuta[100] = "999yutaaa";
	char charlie[40] = "charlie";
	char buxley[40] = "buxley";
	char herman[40] = "herman_abcdefghijklmnopqrstuvwxyz";
	char flaur[100] =
		"flaur_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char flemming[100] =
		"flemming_abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ";
	char xiao[100] = "xiao";
	char gilly[100] = "gilly";
	char brock[100] = "brock";
	char sloan[100] = "sloan";
	char ryan[100] = "ryan";
	char jacob[100] = "jacob";
	char megan[100] = "megan";
	char donata[100] = "donata";
	char honda[100] = "honda";
	char xondley[100] = "xondley";
	char perkins[100] = "perkins";
	char rick[100] = "rick";
	char woally[100] = "woally";
	char hoosin[100] = "hoosin";
	char sybian[100] = "sybian";
	char trillian[100] = "trillian";
	char coxes[100] = "coxes";

	struct TestRecord records[] = {
		{.key = 1, .data = billy, .size = sizeof(billy)},		 // 1
		{.key = 2, .data = ruth, .size = sizeof(ruth)},			 // 2
		{.key = 3, .data = yuta, .size = sizeof(yuta)},			 // 3
		{.key = 8, .data = charlie, .size = sizeof(charlie)},	 // 4
		{.key = 5, .data = buxley, .size = sizeof(buxley)},		 // 5
		{.key = 9, .data = herman, .size = sizeof(herman)},		 // 6
		{.key = 25, .data = flaur, .size = sizeof(flaur)},		 // 7
		{.key = 23, .data = flemming, .size = sizeof(flemming)}, // 8
		{.key = 4, .data = xiao, .size = sizeof(xiao)},			 // 9
		{.key = 13, .data = gilly, .size = sizeof(billy)},		 // 10
		{.key = 40, .data = brock, .size = sizeof(brock)},		 // 11
		{.key = 41, .data = sloan, .size = sizeof(sloan)},		 // 12
		{.key = 42, .data = ryan, .size = sizeof(ryan)},		 // 13
		{.key = 35, .data = jacob, .size = sizeof(jacob)},		 // 14
		{.key = 32, .data = megan, .size = sizeof(megan)},		 // 15
		{.key = 67, .data = donata, .size = sizeof(donata)},	 // 16
		{.key = 22, .data = honda, .size = sizeof(honda)},		 // 17
		{.key = 21, .data = xondley, .size = sizeof(xondley)},	 // 18
		{.key = 19, .data = perkins, .size = sizeof(perkins)},	 // 19
		{.key = 10, .data = rick, .size = sizeof(rick)},		 // 20
		{.key = 11, .data = woally, .size = sizeof(woally)},	 // 21
		{.key = 12, .data = hoosin, .size = sizeof(hoosin)},	 // 22
		{.key = 17, .data = sybian, .size = sizeof(sybian)},	 // 23
		{.key = 16, .data = trillian, .size = sizeof(trillian)}, // 24
		{.key = 55, .data = coxes, .size = sizeof(coxes)},		 // 25
	};

	/**
	 * Ensure that we can still reach certain keys
	 * after inserting others.
	 */
	struct Cursor* cursor = NULL;
	char found = 0;
	for( int i = 0; i < sizeof(records) / sizeof(records[0]); i++ )
	{
		struct TestRecord* test = &records[i];
		btree_insert(tree, test->key, test->data, test->size);
	}

	char buf1[120] = {0};
	struct TestRecord* prev_rec = NULL;
	struct TestRecord* curr_rec = NULL;
	cursor = cursor_create(tree);
	noderc_acquire(cursor_rcer(cursor), &nv);

	btresult = cursor_iter_begin(cursor);
	while( btresult == BTREE_OK )
	{
		if( nv.page->page_id != cursor->current_page_id )
			noderc_reinit_read(
				cursor_rcer(cursor), &nv, cursor->current_page_id);

		memset(buf1, 0x00, sizeof(buf1));
		btree_node_read_at(
			tree,
			nv_node(&nv),
			cursor->current_key_index.index,
			buf1,
			sizeof(buf1));
		// dbg_print_buffer(buf, strnlen(buf, sizeof(buf)));

		bool found = false;
		for( int i = 0; i < sizeof(records) / sizeof(records[0]); i++ )
		{
			struct TestRecord* rec = &records[i];
			if( memcmp(buf1, rec->data, rec->size) == 0 )
			{
				curr_rec = rec;
				found = true;
				break;
			}
		}

		if( !found )
			goto fail;

		if( prev_rec != NULL && prev_rec->key >= curr_rec->key )
			goto fail;

		prev_rec = curr_rec;

		btresult = cursor_iter_next(cursor);
	}

end:
	noderc_release(cursor_rcer(cursor), &nv);
	remove(db_name);

	return result;
fail:
	result = 0;
	goto end;
}