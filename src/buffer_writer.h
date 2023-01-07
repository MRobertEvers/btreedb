#ifndef BUFFER_WRITER_H_
#define BUFFER_WRITER_H_

struct BufferWriter;
struct BufferReader;

/**
 * @brief Writes data from left to right.
 *
 * @param int
 * @return int
 */
int bw_write_lr(struct BufferWriter*, void*, unsigned int size);
int bw_read_lr(struct BufferReader*, void*, unsigned int size);

int bw_write_rl(struct BufferWriter*, void*, unsigned int size);

// Don't really need this...
// int bw_read_rl(struct BufferReader*, void*, unsigned int);

/**
 * @brief Function passed to the partial writer. Writes to the buffer.
 *
 * @param writer_callback_state
 * @param data data to write
 * @param data_size
 */
typedef int (*writer_t)(struct BufferWriter*, void*, unsigned int);

struct BufferWriter
{
	char* write_head;
	unsigned int bytes_written;

	void* buffer;
	unsigned int buffer_size;

	writer_t write;

	// Additional data for use if custom callback used.
	void* client_data;
};

/**
 * @brief The buffer right edge should be used when using rl.
 *
 * @param writer
 * @param buffer
 * @param buffer_size
 * @param writer_cb May be null. Defaults to lr writer.
 */
void buffer_writer_init(
	struct BufferWriter* writer,
	void* buffer,
	unsigned int buffer_size,
	writer_t writer_cb);

/**
 * @brief
 *
 * @param writer
 * @param data
 * @param data_size
 * @return int bytes written
 */
int buffer_writer_write(
	struct BufferWriter* writer, void* data, unsigned int data_size);

/**
 * @brief Function passed to the partial writer. Writes to the buffer.
 *
 * @param writer_callback_state
 * @param data data to write
 * @param data_size
 */
typedef int (*reader_t)(struct BufferReader*, void*, unsigned int);

struct BufferReader
{
	char* read_head;
	unsigned int bytes_read;

	void* buffer;
	unsigned int buffer_size;

	reader_t read;

	// Additional data for use if custom callback used.
	void* client_data;
};

void buffer_reader_init(
	struct BufferReader* reader,
	void* buffer,
	unsigned int buffer_size,
	reader_t reader_cb);

/**
 * @brief
 *
 * @param writer
 * @param data
 * @param data_size
 * @return int bytes read
 */
int buffer_reader_read(
	struct BufferReader* writer, void* buffer, unsigned int buffer_size);

#endif