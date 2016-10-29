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
#include "strings.h"

#include <cstdio>
#include <string>

namespace strings {

std::string CEscape(const std::string &str) {
  std::string result;
  result.reserve(str.size());
  for (const char c : str) {
    if (c < 0x20) {
      switch (c) {
        case '\t':
          result.append("\\t");
          break;
        case '\v':
          result.append("\\v");
          break;
        case '\f':
          result.append("\\f");
          break;
        case '\a':
          result.append("\\a");
          break;
        case '\r':
          result.append("\\r");
          break;
        case '\n':
          result.append("\\n");
          break;
        case '\b':
          result.append("\\b");
          break;
        default: {
          char buffer[5];
          sprintf(buffer, "\\%03o", c);
          result.append(buffer);
          break;
        }
      }
    } else if (c == '"') {
      result.append("\\\"");
    } else if (c == '\'') {
      result.append("\\'");
    } else if (c > 0x7e) {
      char buffer[5];
      sprintf(buffer, "\\%03o", c);
      result.append(buffer);
    } else {
      result.append(1, c);
    }
  }
  return result;
}

std::string HexEscape(const std::string &str) {
  std::string result;
  result.reserve(str.size());
  for (const char c : str) {
    if (c < 0x20 || c == '"' || c == '\'' || c > 0x7e) {
      char buffer[5];
      sprintf(buffer, "\\x%02X", c);
      result.append(buffer);
    } else {
      result.append(1, c);
    }
  }
  return result;
}

std::string AsciiToUpper(const std::string &str) {
  std::string result;
  result.reserve(str.size());
  for (const char c : str) {
    result.append(1, c >= 'a' && c <= 'z' ? c - 'a' + 'A' : c);
  }
  return result;
}

std::string AsciiToLower(const std::string &str) {
  std::string result;
  result.reserve(str.size());
  for (const char c : str) {
    result.append(1, c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c);
  }
  return result;
}

bool StartsWith(const std::string &str, const std::string &with) {
  return with.size() <= str.size() && with == str.substr(0, with.size());
}

int AscciToInt(int c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  } else if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

}  // namespace strings
