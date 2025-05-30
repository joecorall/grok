/**
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
 */
#pragma once

#include "grk_includes.h"
#include <stdexcept>
#include <algorithm>

/*
 Various coordinate systems are used to describe regions in the tile component buffer.

 1) Canvas coordinates:  JPEG 2000 global image coordinates.

 2) Tile component coordinates: canvas coordinates with sub-sampling applied

 3) Band coordinates: coordinates relative to a specified sub-band's origin

 4) Buffer coordinates: coordinate system where all resolutions are translated
  to common origin (0,0). If each code block is translated relative to the origin of the
 resolution that **it belongs to**, the blocks are then all in buffer coordinate system

 Note: the name of any method or variable returning non canvas coordinates is appended
 with "REL", to signify relative coordinates.

 */

namespace grk
{

enum eSplitOrientation
{
  SPLIT_L,
  SPLIT_H,
  SPLIT_NUM_ORIENTATIONS
};

template<typename T>
struct TileComponentWindow;
template<typename T>
struct TileComponentWindowBase;

/**
 * ResWindow
 *
 * Manage all buffers for a single DWT resolution. This class
 * stores a buffer for the resolution (in REL coordinates),
 * and also buffers for the 4 sub-bands generated by the DWT transform
 * (in Canvas coordinates).
 *
 * Note: if highest resolution window is set, then only this window allocates
 * memory, and all other ResWindow buffers attach themselves to the highest resolution buffer
 *
 */
template<typename T>
struct ResWindow
{
  friend struct TileComponentWindowBase<T>;
  friend struct TileComponentWindow<T>;
  typedef grk_buf2d<T, AllocatorAligned> Buf2dAligned;

private:
  ResWindow(uint8_t numresolutions, uint8_t resno, Buf2dAligned* resWindowHighestResREL,
            ResSimple tileCompAtRes, ResSimple tileCompAtLowerRes, grk_rect32 resWindow,
            grk_rect32 tileCompWindowUnreduced, grk_rect32 tileCompUnreduced, uint32_t FILTER_WIDTH)
      : allocated_(false), filterWidth_(FILTER_WIDTH), tileCompAtRes_(tileCompAtRes),
        tileCompAtLowerRes_(tileCompAtLowerRes), resWindowBuffer_(new Buf2dAligned(resWindow)),
        resWindowBufferSplit_{nullptr, nullptr},
        resWindowBufferHighestResREL_(resWindowHighestResREL),
        resWindowBufferREL_(new Buf2dAligned(resWindow.width(), resWindow.height())),
        resWindowBufferSplitREL_{nullptr, nullptr}
  {
    resWindowBuffer_->setOrigin(tileCompAtRes_, true);
    uint8_t numDecomps =
        (resno == 0) ? (uint8_t)(numresolutions - 1U) : (uint8_t)(numresolutions - resno);
    grk_rect32 resWindowPadded;
    for(uint8_t orient = 0; orient < ((resno) > 0 ? BAND_NUM_ORIENTATIONS : 1); orient++)
    {
      // todo: should only need padding equal to FILTER_WIDTH, not 2*FILTER_WIDTH
      auto bandWindow = getPaddedBandWindow(numDecomps, orient, tileCompWindowUnreduced,
                                            tileCompUnreduced, 2 * FILTER_WIDTH, resWindowPadded);
      grk_rect32 band = tileCompAtRes_.tileBand[BAND_ORIENT_LL];
      if(resno > 0)
        band = orient == BAND_ORIENT_LL ? grk_rect32(tileCompAtLowerRes_)
                                        : tileCompAtRes_.tileBand[orient - 1];
      bandWindow.setOrigin(band, true);
      assert(bandWindow.intersection(band).setOrigin(bandWindow, true) == bandWindow);
      bandWindowsBoundsPadded_.push_back(bandWindow);
    }
    // windowed decompression
    if(FILTER_WIDTH)
    {
      if(tileCompAtLowerRes_.numTileBandWindows)
      {
        assert(resno > 0);
        resWindowBuffer_->setRect(resWindowPadded);
        resWindowBufferREL_->setRect(resWindowBuffer_->toRelative());
        resWindowBuffer_->toAbsolute();

        for(uint8_t orient = 0; orient < BAND_NUM_ORIENTATIONS; orient++)
        {
          auto bandWindow = bandWindowsBoundsPadded_[orient];
          bandWindowsBuffersPadded_.push_back(new Buf2dAligned(bandWindow, true));
          bandWindowsBuffersPaddedREL_.push_back(new Buf2dAligned(bandWindow.toRelative(), true));
        }
        genSplitWindowBuffers(resWindowBufferSplit_, resWindowBuffer_,
                              bandWindowsBuffersPadded_[BAND_ORIENT_LL],
                              bandWindowsBuffersPadded_[BAND_ORIENT_LH], true);
        genSplitWindowBuffers(resWindowBufferSplitREL_, resWindowBuffer_,
                              bandWindowsBuffersPadded_[BAND_ORIENT_LL],
                              bandWindowsBuffersPadded_[BAND_ORIENT_LH], false);
      }
    }
    else
    {
      assert(tileCompAtRes_.numTileBandWindows == 3 || !tileCompAtLowerRes.numTileBandWindows);

      // dummy LL band window
      if(tileCompAtLowerRes_.numTileBandWindows && tileCompAtLowerRes_.valid())
      {
        bandWindowsBuffersPadded_.push_back(new Buf2dAligned(0, 0));
        bandWindowsBuffersPaddedREL_.push_back(new Buf2dAligned(0, 0));
        for(uint32_t i = 0; i < tileCompAtRes_.numTileBandWindows; ++i)
        {
          auto tileCompBand = tileCompAtRes_.tileBand + i;

          auto band = grk_rect32(tileCompBand);
          bandWindowsBuffersPadded_.push_back(new Buf2dAligned(band));
          bandWindowsBuffersPaddedREL_.push_back(new Buf2dAligned(band.toRelative()));
        }
        for(uint8_t i = 0; i < SPLIT_NUM_ORIENTATIONS; i++)
        {
          grk_rect32 split = resWindowBuffer_;
          split.y0 =
              (resWindowBuffer_->y0 == 0 ? 0 : ceildivpow2<uint32_t>(resWindowBuffer_->y0 - i, 1));
          split.y1 =
              (resWindowBuffer_->y1 == 0 ? 0 : ceildivpow2<uint32_t>(resWindowBuffer_->y1 - i, 1));
          resWindowBufferSplit_[i] = new Buf2dAligned(split);
          resWindowBufferSplitREL_[i] = new Buf2dAligned(resWindowBufferSplit_[i]);
        }
      }
    }
  }
  ~ResWindow()
  {
    delete resWindowBufferREL_;
    for(auto& b : bandWindowsBuffersPaddedREL_)
      delete b;
    for(uint32_t i = 0; i < SPLIT_NUM_ORIENTATIONS; ++i)
      delete resWindowBufferSplitREL_[i];

    delete resWindowBuffer_;
    for(auto& b : bandWindowsBuffersPadded_)
      delete b;
    for(uint32_t i = 0; i < SPLIT_NUM_ORIENTATIONS; ++i)
      delete resWindowBufferSplit_[i];
  }
  void genSplitWindowBuffers(Buf2dAligned** resWindowBufferSplit, Buf2dAligned* resWindowBuffer,
                             Buf2dAligned* bandWindowsBuffersPaddedXL,
                             Buf2dAligned* bandWindowsBuffersPaddedXH, bool absolute)
  {
    if(!absolute)
    {
      tileCompAtLowerRes_.toRelative();
      bandWindowsBuffersPaddedXL->toRelative();
      bandWindowsBuffersPaddedXH->toRelative();
    }

    // two windows formed by horizontal pass and used as input for vertical pass
    auto splitResWindowBounds = grk_rect32(resWindowBuffer->x0, bandWindowsBuffersPaddedXL->y0,
                                           resWindowBuffer->x1, bandWindowsBuffersPaddedXL->y1);
    resWindowBufferSplit[SPLIT_L] = new Buf2dAligned(splitResWindowBounds);

    splitResWindowBounds =
        grk_rect32(resWindowBuffer->x0, tileCompAtLowerRes_.y1 + bandWindowsBuffersPaddedXH->y0,
                   resWindowBuffer->x1, tileCompAtLowerRes_.y1 + bandWindowsBuffersPaddedXH->y1);
    resWindowBufferSplit[SPLIT_H] = new Buf2dAligned(splitResWindowBounds);

    if(!absolute)
    {
      tileCompAtLowerRes_.toAbsolute();
      bandWindowsBuffersPaddedXL->toAbsolute();
      bandWindowsBuffersPaddedXH->toAbsolute();
    }
  }
  bool alloc(bool clear)
  {
    if(allocated_)
      return true;

    // if top level window is present, then all buffers attach to this window
    if(resWindowBufferHighestResREL_)
    {
      // ensure that top level window is allocated
      if(!resWindowBufferHighestResREL_->alloc2d(clear))
        return false;

      // don't allocate bandWindows for windowed decompression
      if(filterWidth_)
        return true;

      // attach to top level window
      if(resWindowBufferREL_ != resWindowBufferHighestResREL_)
        resWindowBufferREL_->attach(resWindowBufferHighestResREL_);

      // tileCompResLower_ is null for lowest resolution
      if(tileCompAtLowerRes_.numTileBandWindows)
      {
        for(uint8_t orientation = 0; orientation < bandWindowsBuffersPaddedREL_.size();
            ++orientation)
        {
          switch(orientation)
          {
            case BAND_ORIENT_HL:
              bandWindowsBuffersPaddedREL_[orientation]->attach(resWindowBufferHighestResREL_,
                                                                tileCompAtLowerRes_.width(), 0);
              break;
            case BAND_ORIENT_LH:
              bandWindowsBuffersPaddedREL_[orientation]->attach(resWindowBufferHighestResREL_, 0,
                                                                tileCompAtLowerRes_.height());
              break;
            case BAND_ORIENT_HH:
              bandWindowsBuffersPaddedREL_[orientation]->attach(resWindowBufferHighestResREL_,
                                                                tileCompAtLowerRes_.width(),
                                                                tileCompAtLowerRes_.height());
              break;
            default:
              break;
          }
        }
        resWindowBufferSplit_[SPLIT_L]->attach(resWindowBufferHighestResREL_);
        resWindowBufferSplit_[SPLIT_H]->attach(resWindowBufferHighestResREL_, 0,
                                               tileCompAtLowerRes_.height());
      }

      // attach canvas windows to relative windows
      for(uint8_t orientation = 0; orientation < bandWindowsBuffersPaddedREL_.size(); ++orientation)
      {
        bandWindowsBuffersPadded_[orientation]->attach(bandWindowsBuffersPaddedREL_[orientation]);
      }
      resWindowBuffer_->attach(resWindowBufferREL_);
      for(uint8_t i = 0; i < SPLIT_NUM_ORIENTATIONS; ++i)
      {
        if(resWindowBufferSplitREL_[i])
          resWindowBufferSplitREL_[i]->attach(resWindowBufferSplit_[i]);
      }
    }
    else
    {
      // 1. allocate resolution window
      // resolution window is always allocated
      if(!resWindowBufferREL_->alloc2d(clear))
        return false;
      resWindowBuffer_->attach(resWindowBufferREL_);

      // 2, allocate padded band windows
      // band windows are allocated if present
      for(auto& b : bandWindowsBuffersPadded_)
      {
        if(!b->alloc2d(clear))
          return false;
      }
      for(uint8_t orientation = 0; orientation < bandWindowsBuffersPaddedREL_.size(); ++orientation)
      {
        bandWindowsBuffersPaddedREL_[orientation]->attach(
            bandWindowsBuffersPaddedREL_[orientation]);
      }

      // 3. allocate split windows
      if(tileCompAtLowerRes_.numTileBandWindows)
      {
        if(resWindowBufferHighestResREL_)
        {
          resWindowBufferSplit_[SPLIT_L]->attach(resWindowBuffer_);
          resWindowBufferSplit_[SPLIT_H]->attach(resWindowBuffer_, 0, tileCompAtLowerRes_.height());
        }
        else
        {
          resWindowBufferSplit_[SPLIT_L]->alloc2d(clear);
          resWindowBufferSplit_[SPLIT_H]->alloc2d(clear);
        }
        for(uint8_t i = 0; i < SPLIT_NUM_ORIENTATIONS; ++i)
        {
          if(resWindowBufferSplitREL_[i])
            resWindowBufferSplitREL_[i]->attach(resWindowBufferSplit_[i]);
        }
      }
    }

    allocated_ = true;

    return true;
  }

  /**
   * Get band window (in tile component coordinates) for specified number
   * of decompositions (with padding)
   *
   * Note: if numDecomps is zero, then the band window (and there is only one)
   * is equal to the unreduced tile component window (with padding)
   */
  static grk_rect32 getPaddedBandWindow(uint8_t numDecomps, uint8_t orientation,
                                        grk_rect32 unreducedTileCompWindow,
                                        grk_rect32 unreducedTileComp, uint32_t padding,
                                        grk_rect32& paddedResWindow)
  {
    assert(orientation < BAND_NUM_ORIENTATIONS);
    if(numDecomps == 0)
    {
      assert(orientation == 0);
      return unreducedTileCompWindow.grow_IN_PLACE(padding).intersection(&unreducedTileComp);
    }
    paddedResWindow = unreducedTileCompWindow;
    auto oneLessDecompTile = unreducedTileComp;
    if(numDecomps > 1)
    {
      paddedResWindow = ResSimple::getBandWindow(numDecomps - 1, 0, unreducedTileCompWindow);
      oneLessDecompTile = ResSimple::getBandWindow(numDecomps - 1, 0, unreducedTileComp);
    }
    paddedResWindow.grow_IN_PLACE(2 * padding).clip_IN_PLACE(&oneLessDecompTile);
    paddedResWindow.setOrigin(oneLessDecompTile, true);

    return ResSimple::getBandWindow(1, orientation, paddedResWindow);
  }

  grk_buf2d_simple<int32_t> getResWindowBufferSimple(void) const
  {
    return resWindowBuffer_->simple();
  }
  grk_buf2d_simple<float> getResWindowBufferSimpleF(void) const
  {
    return resWindowBuffer_->simpleF();
  }
  void disableBandWindowAllocation(void)
  {
    resWindowBufferHighestResREL_ = resWindowBufferREL_;
  }
  Buf2dAligned* getResWindowBufferSplitREL(eSplitOrientation orientation) const
  {
    return resWindowBufferSplitREL_[orientation];
  }
  const grk_rect32* getBandWindowPadded(eBandOrientation orientation) const
  {
    return &bandWindowsBoundsPadded_[orientation];
  }
  const Buf2dAligned* getBandWindowBufferPaddedREL(eBandOrientation orientation) const
  {
    return bandWindowsBuffersPaddedREL_[orientation];
  }
  const grk_buf2d_simple<int32_t>
      getBandWindowBufferPaddedSimple(eBandOrientation orientation) const
  {
    return bandWindowsBuffersPadded_[orientation]->simple();
  }
  const grk_buf2d_simple<float> getBandWindowBufferPaddedSimpleF(eBandOrientation orientation) const
  {
    return bandWindowsBuffersPadded_[orientation]->simpleF();
  }
  Buf2dAligned* getResWindowBufferREL(void) const
  {
    return resWindowBufferREL_;
  }
  bool allocated_;
  uint32_t filterWidth_;

  ResSimple tileCompAtRes_; // numTileBandWindows> 0 will trigger creation of band window buffers
  ResSimple tileCompAtLowerRes_; // numTileBandWindows==0 for lowest resolution

  Buf2dAligned* resWindowBuffer_;
  Buf2dAligned* resWindowBufferSplit_[SPLIT_NUM_ORIENTATIONS];
  std::vector<Buf2dAligned*> bandWindowsBuffersPadded_;

  /*
  bandWindowsBoundsPadded_ is used for determining which precincts and code blocks overlap
  the window of interest, in each respective resolution
  */
  std::vector<grk_rect32> bandWindowsBoundsPadded_;

  Buf2dAligned* resWindowBufferHighestResREL_;
  Buf2dAligned* resWindowBufferREL_;
  Buf2dAligned* resWindowBufferSplitREL_[SPLIT_NUM_ORIENTATIONS];
  std::vector<Buf2dAligned*> bandWindowsBuffersPaddedREL_;
};

} // namespace grk
