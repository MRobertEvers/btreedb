#ifndef BUFFER_WRITER_H_
#define BUFFER_WRITER_H_

struct BufferWriter
{
	char* write_head;
	unsigned int bytes_written;

	void* buffer;
	unsigned int buffer_size;
};

void buffer_writer_init(
	struct BufferWriter* writer, void* buffer, unsigned int buffer_size);

void buffer_writer_write(
	struct BufferWriter* writer, void* data, unsigned int data_size);

#endif