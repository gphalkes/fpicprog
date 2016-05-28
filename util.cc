#include "util.h"

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <gflags/gflags.h>
#include <time.h>

DEFINE_int32(verbosity, 1, "Verbosity level. 0 for no output, higher number for more output.");

void fatal(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  exit(EXIT_FAILURE);
}

void Sleep(Duration duration) {
  if (duration <= 0) return;
  struct timespec to_sleep;
  to_sleep.tv_sec = duration / 1000000000;
  to_sleep.tv_nsec = duration % 1000000000;
  while (nanosleep(&to_sleep, &to_sleep)) {
    if (errno != EINTR) FATAL("nanosleep failed: %s\n", strerror(errno));
  }
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
