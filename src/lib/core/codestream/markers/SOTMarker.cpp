/*
 *    Copyright (C) 2016-2025 Grok Image Compression Inc.
 *
 *    This source code is free software: you can redistribute it and/or  modify
 *    it under the terms of the GNU Affero General Public License, version 3,
 *    as published by the Free Software Foundation.
 *
 *    This source code is distributed in the hope that it will be useful,
 *    but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU Affero General Public License for more details.
 *
 *    You should have received a copy of the GNU Affero General Public License
 *    along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *
 *    This source code incorporates work covered by the BSD 2-clause license.
 *    Please see the LICENSE file in the root directory for details.
 *
 */

#include "grk_includes.h"

namespace grk
{
SOTMarker::SOTMarker(void) : psot_location_(0) {}
bool SOTMarker::write_psot(BufferedStream* stream, uint32_t tileLength)
{
  if(psot_location_)
  {
    auto currentLocation = stream->tell();
    stream->seek(psot_location_);
    if(!stream->writeInt(tileLength))
      return false;
    stream->seek(currentLocation);
  }

  return true;
}

bool SOTMarker::write(TileProcessor* proc, uint32_t tileLength)
{
  auto stream = proc->getStream();
  /* SOT */
  if(!stream->writeShort(J2K_SOT))
    return false;

  /* Lsot */
  if(!stream->writeShort(10))
    return false;
  /* Isot */
  if(!stream->writeShort((uint16_t)proc->getIndex()))
    return false;

  /* Psot  */
  if(tileLength)
  {
    if(!stream->writeInt(tileLength))
      return false;
  }
  else
  {
    psot_location_ = stream->tell();
    if(!stream->skip(4))
      return false;
  }

  /* TPsot */
  if(!stream->writeByte(proc->tilePartCounter_))
    return false;

  /* TNsot */
  if(!stream->writeByte(proc->cp_->tcps[proc->getIndex()].numTileParts_))
    return false;

  return true;
}

bool SOTMarker::read(CodeStreamDecompress* codeStream, uint8_t* headerData, uint32_t headerSize,
                     uint32_t* tilePartLength, uint16_t* tile_index, uint8_t* tilePartIndex,
                     uint8_t* numTileParts)
{
  assert(headerData != nullptr);
  if(headerSize != sot_marker_segment_len_minus_tile_data_len - MARKER_PLUS_MARKER_LENGTH_BYTES)
  {
    grklog.error("Error reading next SOT marker");
    return false;
  }
  uint32_t len;
  uint16_t index;
  uint8_t tp_index, num_tile_parts;
  grk_read(headerData, &index);
  headerData += sizeof(uint16_t);
  grk_read(headerData, &len);
  headerData += sizeof(uint32_t);
  grk_read(headerData++, &tp_index);
  grk_read(headerData++, &num_tile_parts);

  if(num_tile_parts && (tp_index >= num_tile_parts))
  {
    grklog.error("Tile %u: Tile part index (%u) must be less than number of tile parts (%u)", index,
                 tp_index, num_tile_parts);
    throw CorruptSOTMarkerException();
  }

  if(!codeStream->allocateProcessor(index))
    return false;
  *tilePartLength = len;
  *tile_index = index;
  *tilePartIndex = tp_index;
  *numTileParts = num_tile_parts;

  return true;
}

bool SOTMarker::read(CodeStreamDecompress* codeStream, uint8_t* headerData, uint16_t header_size)
{
  uint32_t tilePartLength = 0;
  uint8_t numTileParts = 0;
  uint16_t tile_index;
  uint8_t currentTilePart;

  if(!read(codeStream, headerData, header_size, &tilePartLength, &tile_index, &currentTilePart,
           &numTileParts))
  {
    grklog.error("Error reading SOT marker");
    return false;
  }
  auto cp = codeStream->getCodingParams();
  if(tile_index >= cp->t_grid_width * cp->t_grid_height)
  {
    grklog.error("Invalid tile number %u", tile_index);
    return false;
  }

  auto tcp = cp->tcps + tile_index;
  if(!tcp->advanceTilePartCounter(tile_index, currentTilePart))
    return false;

  // grklog.info("SOT: Tile %u, tile part %u",tile_index, currentTilePart);

  if(tilePartLength == sot_marker_segment_len_minus_tile_data_len)
  {
    // next marker should be SOD
    codeStream->setExpectSOD();
  }
  else
  {
    /* PSot should be equal to zero, or greater than or equal to sot_marker_segment_min_len.
     * There is a third, technically illegal case where PSot equals
     * sot_marker_segment_len_minus_tile_data_len, where there is just a single SOD marker, and
     * the SOD marker length is excluded from the signalled PSot, for some reason.
     */
    if(tilePartLength && (tilePartLength < sot_marker_segment_min_len))
    {
      grklog.error("Illegal Psot value %u", tilePartLength);
      return false;
    }
  }
  /* Ref A.4.2: Psot may equal zero if it is the last tile-part of the code stream.*/
  auto decompressState = codeStream->getDecompressorState();
  if(!tilePartLength)
    decompressState->lastTilePartInCodeStream = true;

  // ensure that current tile part number read from SOT marker
  // is not larger than total number of tile parts
  if(tcp->numTileParts_ != 0 && currentTilePart >= tcp->numTileParts_)
  {
    /* Fixes https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=2851 */
    grklog.error("Current tile part number (%u) read from SOT marker is greater\n than total "
                 "number of tile-parts (%u).",
                 currentTilePart, tcp->numTileParts_);
    decompressState->lastTilePartInCodeStream = true;
    return false;
  }

  if(numTileParts)
  { /* Number of tile-part header is provided by this tile-part header */
    /* tile-parts for that tile and zero (A.4.2 of 15444-1 : 2002). */

    if(tcp->numTileParts_)
    {
      if(currentTilePart >= tcp->numTileParts_)
      {
        grklog.error("In SOT marker, TPSot (%u) is not valid with regards to the current "
                     "number of tile-part (%u)",
                     currentTilePart, tcp->numTileParts_);
        decompressState->lastTilePartInCodeStream = true;
        return false;
      }
      if(numTileParts != tcp->numTileParts_)
      {
        grklog.error("Invalid number of tile parts for tile number %u. "
                     "Got %u, expected %u as signalled in previous tile part(s).",
                     tile_index, numTileParts, tcp->numTileParts_);
        return false;
      }
    }
    if(currentTilePart >= numTileParts)
    {
      grklog.error("In SOT marker, TPSot (%u) must be less than number of tile-parts (%u)",
                   currentTilePart, numTileParts);
      decompressState->lastTilePartInCodeStream = true;
      return false;
    }
    tcp->numTileParts_ = numTileParts;
  }

  /* If we know the number of tile parts from the header, we check whether we have read the last
   * one*/
  if(tcp->numTileParts_ && (tcp->numTileParts_ == (currentTilePart + 1)))
    decompressState->setComplete(tile_index);

  codeStream->currentProcessor()->setTilePartDataLength(currentTilePart, tilePartLength,
                                                        decompressState->lastTilePartInCodeStream);
  decompressState->setState(DECOMPRESS_STATE_TPH);

  grk_pt16 currTile(tile_index % cp->t_grid_width, tile_index / cp->t_grid_width);
  auto codeStreamInfo = codeStream->getCodeStreamInfo();

  return !codeStreamInfo ||
         codeStreamInfo->updateTileInfo(tile_index, currentTilePart, numTileParts);
}

} /* namespace grk */
