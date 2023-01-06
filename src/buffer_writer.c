#include "buffer_writer.h"

#include <string.h>

int
bw_write_lr(struct BufferWriter* writer, void* data, unsigned int data_size)
{
	if( writer->bytes_written + data_size < writer->buffer_size )
	{
		memcpy(writer->write_head, data, data_size);
		writer->write_head += data_size;
		writer->bytes_written += data_size;
		return data_size;
	}
	else
	{
		return 0;
	}
}

int
bw_read_lr(struct BufferReader* reader, void* buffer, unsigned int buffer_size)
{
	if( buffer_size <= reader->buffer_size - reader->bytes_read )
	{
		memcpy(buffer, reader->read_head, buffer_size);
		reader->read_head += buffer_size;
		reader->bytes_read += buffer_size;
		return buffer_size;
	}
	else
	{
		return 0;
	}
}

int
bw_write_rl(struct BufferWriter* writer, void* data, unsigned int data_size)
{
	if( writer->bytes_written + data_size < writer->buffer_size )
	{
		writer->write_head -= data_size;

		memcpy(writer->write_head, data, data_size);

		writer->bytes_written += data_size;

		return data_size;
	}
	else
	{
		return 0;
	}
}

void
buffer_writer_init(
	struct BufferWriter* writer,
	void* buffer,
	unsigned int buffer_size,
	writer_t writer_cb)
{
	memset(writer, 0x00, sizeof(*writer));

	writer->buffer = buffer;
	writer->buffer_size = buffer_size;
	writer->write = writer_cb == NULL ? bw_write_lr : writer_cb;
	writer->write_head = buffer;
	writer->bytes_written = 0;
}

int
buffer_writer_write(
	struct BufferWriter* writer, void* data, unsigned int data_size)
{
	assert(writer->write != NULL);

	return writer->write(writer, data, data_size);
}

void
buffer_reader_init(
	struct BufferReader* reader,
	void* buffer,
	unsigned int buffer_size,
	reader_t reader_cb)
{
	memset(reader, 0x00, sizeof(*reader));

	reader->buffer = buffer;
	reader->buffer_size = buffer_size;
	reader->read = reader_cb == NULL ? bw_read_lr : reader_cb;
	reader->read_head = buffer;
	reader->bytes_read = 0;
}

int
buffer_reader_read(
	struct BufferReader* reader, void* buffer, unsigned int buffer_size)
{
	assert(reader->read != NULL);

	return reader->read(reader, buffer, buffer_size);
}