#include "binary_search.h"

int
binary_search(int* arr, unsigned char num_keys, int value, char* found)
{
	int left = 0;
	int right = num_keys - 1;
	int mid = 0;
	*found = 0;

	while( left <= right )
	{
		mid = (right - left) / 2 + left;

		if( arr[mid] == value )
		{
			*found = 1;
			return mid;
		}
		else if( arr[mid] < value )
		{
			left = mid + 1;
		}
		else
		{
			right = mid - 1;
		}
	}

	return left;
}