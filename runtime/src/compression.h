#ifndef _CBUFFER_H
#define _CBUFFER_H

#include "lzoconf.h"

class LevelTable;

/*! \brief Initializes the compression buffer.
 *
 * \param size The number of entries allowed in the compression buffer.
 */
void CBufferInit(int size);

/*! \brief De-initializes the compression buffer. */
void CBufferDeinit();

/*! \brief Adds a level table entry to the compression buffer.
 *
 * \param table The level table to add to the compression buffer.
 * \return The number of bytes gained as a result of adding.
 */
int  CBufferAdd(LevelTable* table);

/*! \brief Marks buffer entry for level table as being accessed.
 *
 * \param table The level table to mark as accessed.
 * \pre The table to mark must be valid and in the compression buffer.
 */
void CBufferAccess(LevelTable* table);

/*! \brief Decompress a level table and adds it to compression buffer.
 *
 * \param table The level table to decompress.
 * \return Number of extra bytes required for decompression.
 */
int CBufferDecompress(LevelTable* table);

/*! \brief Compress data using LZO library
 *
 * \pre Input data is valid and non-zero sized.
 * \param src The data to be compressed
 * \param src_size The size of input data (in bytes)
 * \param[out] dest_size The size data after compressed
 * \return Pointer to the beginning of compressed data
 */
UInt8* compressData(UInt8* src, lzo_uint src_size, lzo_uintp dest_size);

/*! \brief Decompress data using LZO library
 *
 * \param dest Chunk of memory where decompressed data is written
 * \param src The data to be decompressed
 * \param src_size Size of the compressed data (in bytes)
 * \param[out] dest_size Size of the decompressed data (in bytes)
 */
void decompressData(UInt8* dest, UInt8* src, lzo_uint src_size, lzo_uintp dest_size);

#endif
