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
#if defined(_WIN32)
#include <windows.h>
#else
#include <thread>
#endif

DEFINE_int32(verbosity, 1, "Verbosity level. 0 for no output, higher number for more output.");

void fatal(const char *fmt, ...) {
  va_list args;

  va_start(args, fmt);
  vfprintf(stderr, fmt, args);
  va_end(args);
  exit(EXIT_FAILURE);
}

// Unfortunately the sleep functions on Windows are not particularly accurate, and may even sleep
// _less_ than the requested amount. Given that we want to do timing accurate to within a
// millisecond or so, this is not accurate enough. Hence we provide our own timing routines, which
// partially rely on busy waiting to make them accurate.

#ifdef _WIN32
static LARGE_INTEGER GetTimestamp() {
  LARGE_INTEGER result;
  if (!QueryPerformanceCounter(&result)) {
    fatal("Error getting performance counter\n");
  }
  return result;
}

void Sleep(Duration duration) {
  static LARGE_INTEGER frequency;
  static UINT timer_period;
  if (frequency.QuadPart == 0) {
    // We need the performance counters to do any sort of remotely accurate timing.
    if (!QueryPerformanceFrequency(&frequency)) {
      fatal("System does not provide performance counter\n");
    }
    // Set the timer frequency to 1ms or the best the system can provide. This prevents having to
    // do busy-waiting for extended periods.
    TIMECAPS tc;
    if (timeGetDevCaps(&tc, sizeof(TIMECAPS)) != TIMERR_NOERROR) {
      fatal("System does not provide timer information\n");
    }
    timer_period = std::max(tc.wPeriodMin, 1u);
    timeBeginPeriod(timer_period);
  }

  LARGE_INTEGER start = GetTimestamp();
  // Use Sleep to do as much of the work as possible. However, this needs to take into account
  // that Sleep may sleep a full timer_period too long.
  int milliseconds_to_sleep = duration.count() / 1000000 - (int) timer_period;
  if (milliseconds_to_sleep > 0) {
    Sleep(milliseconds_to_sleep);
  }

  LONGLONG nanoseconds_passed = 0;
  // Do busy-waiting for the remainder.
  while (nanoseconds_passed < duration.count()) {
    LARGE_INTEGER end = GetTimestamp();
    nanoseconds_passed = (double)(end.QuadPart - start.QuadPart) * 1000000000 / frequency.QuadPart;
  }
}
#else
void Sleep(Duration duration) {
  std::this_thread::sleep_for(duration);
}
#endif

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
