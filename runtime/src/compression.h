#ifndef _CBUFFER_H
#define _CBUFFER_H

#include "MShadowSkadu.h"

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

#endif
