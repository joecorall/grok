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
 */
#include <climits>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include "grok.h"

extern "C" int LLVMFuzzerInitialize(int* argc, char*** argv);
extern "C" int LLVMFuzzerTestOneInput(const uint8_t* buf, size_t len);
struct Initializer
{
  Initializer()
  {
    grk_initialize(nullptr, 0);
  }
};
int LLVMFuzzerInitialize(int* argc, char*** argv)
{
  static Initializer init;
  return 0;
}
int LLVMFuzzerTestOneInput(const uint8_t* buf, size_t len)
{
  grk_image* image = nullptr;
  grk_header_info headerInfo = {};
  grk_decompress_parameters parameters = {};
  uint32_t x0, y0, width, height;
  grk_object* codec = nullptr;
  grk_stream_params stream_params = {};
  stream_params.buf = const_cast<uint8_t*>(buf);
  stream_params.buf_len = len;
  codec = grk_decompress_init(&stream_params, &parameters);
  if(!codec)
    goto cleanup;
  if(!grk_decompress_read_header(codec, &headerInfo))
    goto cleanup;
  image = grk_decompress_get_image(codec);
  width = image->x1 - image->x0;
  if(width > 1024)
    width = 1024;
  height = image->y1 - image->y0;
  if(height > 1024)
    height = 1024;
  x0 = 10;
  if(x0 >= width)
    x0 = 0;
  y0 = 10;
  if(y0 >= height)
    y0 = 0;
  if(grk_decompress_set_window(codec, x0, y0, width, height))
  {
    if(!grk_decompress(codec, nullptr))
      goto cleanup;
  }
cleanup:
  grk_object_unref(codec);

  return 0;
}
