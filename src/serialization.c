#include "serialization.h"

void
ser_write_32bit_le(void* buffer, unsigned int val)
{
	unsigned char* byte_buffer = buffer;
	for( int i = 0; i < 4; i++ )
	{
		byte_buffer[i] = ((val >> i * 8) & 0xff);
	}
}

void
ser_read_32bit_le(unsigned int* val, void* buffer)
{
	*val = 0;
	unsigned char* byte_buffer = buffer;
	for( int i = 0; i < 4; i++ )
	{
		*val = *val | (byte_buffer[i] << i * 8);
	}
}