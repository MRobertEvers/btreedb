#include "serialization_test.h"

#include "serialization.h"
int
serialization_test(void)
{
	int result = 1;

	char buf[4] = {0};

	unsigned int val = 0xDEADBEEF;

	ser_write_32bit_le(buf, val);

	unsigned int out = 0;

	ser_read_32bit_le(buf, &out);

	if( out != val )
		result = 0;

	return result;
}