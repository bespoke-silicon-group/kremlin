#ifndef _CBUFFER_H
#define _CBUFFER_H

#include "lzoconf.h"

class LevelTable;

class CBuffer {
public:
	/*! @brief Initializes the compression buffer.
	 *
	 * @param size The number of entries allowed in the compression buffer.
	 * @pre size is positive.
	 */
	void init(unsigned size);

	/*! \brief De-initializes the compression buffer. */
	void deinit();

	/*! @brief Adds a level table entry to the compression buffer.
	 *
	 * @param table The level table to add to the compression buffer.
	 * @return The number of bytes gained as a result of adding.
	 * @pre table is non-NULL.
	 */
	int add(LevelTable *table);

	/*! \brief Marks buffer entry for level table as being accessed.
	 *
	 * \param table The level table to mark as accessed.
	 * \pre The table to mark must be valid and in the compression buffer.
	 */
	void touch(LevelTable *table);

	// TODO: change name to something like "getDecompressed"?
	/*! @brief Decompress a level table and adds it to compression buffer.
	 *
	 * @param table The level table to decompress.
	 * @return Number of extra bytes required for decompression.
	 * @pre table is non-NULL.
	 */
	int decompress(LevelTable *table);

private:
	unsigned num_entries; //!< number of entries in the compression buffer
	
	/*! \brief Adds a level table entry to the compression buffer.
	 *
	 * \param l_table The level table to add to the buffer.
	 * \pre l_table is non-NULL.
	 */
	void addToBuffer(LevelTable *l_table);

	/*! \brief Removes a suitable entry from the compression buffer.
	 *
	 * \return The number of bytes saved by removing from the buffer.
	 */
	int evictFromBuffer();
};

/*! @brief Compress data using LZO library
 *
 * @param decomp_data The data to be compressed
 * @param decomp_size The size of input data (in bytes)
 * @param[out] comp_size The size data after compressed
 * @return Pointer to the beginning of compressed data
 * @pre decomp_data is non-NULL.
 * @pre decomp_size is positive.
 * @pre comp_size is non-NULL.
 * @post Returned pointer is non-NULL.
 */
UInt8* compressData(UInt8* decomp_data, lzo_uint decomp_size, lzo_uintp comp_size);

/*! @brief Decompress data using LZO library
 *
 * @param decomp_data Chunk of memory where decompressed data is written.
 * @param comp_data Pointer to the data to be decompressed.
 * @param comp_size Size of the compressed data (in bytes)
 * @param[out] decomp_size Size of the decompressed data (in bytes)
 * @pre comp_data, decomp_data, and decomp_size are all non-NULL.
 * @pre comp_size is positive.
 * @post comp_data will point to NULL.
 */
void decompressData(UInt8* decomp_data, UInt8* comp_data, lzo_uint comp_size, lzo_uintp decomp_size);

#endif
