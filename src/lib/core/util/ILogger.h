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
#pragma once

#include <cstring>
#include <cstdarg>

namespace grk
{

struct ILogger
{
  virtual ~ILogger() = default; // Virtual destructor for polymorphism
  virtual void info(const char* fmt, ...) = 0;
  virtual void warn(const char* fmt, ...) = 0;
  virtual void error(const char* fmt, ...) = 0;
  virtual void debug(const char* fmt, ...) = 0; // Added debug
  virtual void trace(const char* fmt, ...) = 0; // Added trace
};

} // namespace grk