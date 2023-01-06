#ifndef SERIALIZATION_H_
#define SERIALIZATION_H_

/**
 * @brief Write LE 32 bit to buffer.
 *
 * @param buffer
 * @param val
 */
void ser_write_32bit_le(void* buffer, unsigned int val);

/**
 * @brief Read LE 32 bit from buffer.
 *
 * @param buffer
 * @param val
 */
void ser_read_32bit_le(void* buffer, unsigned int* val);

#endif