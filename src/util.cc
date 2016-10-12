/* Copyright (C) 2016 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "util.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gflags/gflags.h>
#include <thread>

DEFINE_int32(verbosity, 1, "Verbosity level. 0 for no output, higher number for more output.");

void fatal(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  exit(EXIT_FAILURE);
}

void Sleep(Duration duration) {
  std::this_thread::sleep_for(duration);
}

std::string HexByte(uint8_t byte) {
  static char convert[] = "0123456789ABCDEF";
  return std::string(1, convert[byte >> 4]) + convert[byte & 0xf];
}

std::string HexUint16(uint16_t word) { return HexByte((word >> 8) & 0xff) + HexByte(word & 0xff); }

std::string HexUint32(uint32_t word) {
  return HexByte(word >> 24) + HexByte((word >> 16) & 0xff) + HexByte((word >> 8) & 0xff) +
         HexByte(word & 0xff);
}

const char *SectionToName(Section section) {
  switch (section) {
    case FLASH:
      return "flash";
    case USER_ID:
      return "user ID";
    case CONFIGURATION:
      return "configuration";
    case EEPROM:
      return "EEPROM";
    default:
      return "unknown";
  }
}

void print_msg(int level, const char *fmt, ...) {
  if (level <= FLAGS_verbosity) {
    va_list args;
    va_start(args, fmt);
    vfprintf(stderr, fmt, args);
    va_end(args);
  }
}
