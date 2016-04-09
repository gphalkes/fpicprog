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

}  // namespace strings
