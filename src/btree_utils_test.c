#include "btree_utils_test.h"

#include "btree_utils.h"

int
btree_utils_test_bin_search_keys(void)
{
	int result = 1;
	int index;

	struct BTreePageKey arr[10];
	unsigned int arr_size = sizeof(arr) / sizeof(arr[0]);
	for( int i = 0; i < arr_size; i++ )
	{
		arr[i].key = i * 2;
	}

	char found = 0;
	index = btu_binary_search_keys(arr, arr_size, 1, &found);

	// Expect this
	//  0 , 2 , 4.
	// Search for 1 should be index 1, i.e. index of 2.
	// This is the index where the value would be inserted.
	if( index != 1 || found != 0 )
	{
		result = 0;
		goto end;
	}

	index = btu_binary_search_keys(arr, arr_size, 2, &found);

	// Expect exact key match to be index of that key.
	if( index != 1 || found != 1 )
	{
		result = 0;
		goto end;
	}

	index = btu_binary_search_keys(arr, arr_size, 300, &found);

	// Expect past the end
	if( index != arr_size || found != 0 )
	{
		result = 0;
		goto end;
	}

end:

	return result;
}