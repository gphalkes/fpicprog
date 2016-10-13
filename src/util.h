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
#ifndef UTIL_H_
#define UTIL_H_

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <functional>
#include <set>
#include <type_traits>

typedef std::basic_string<uint8_t> Datastring;
typedef std::basic_string<uint16_t> Datastring16;

enum Pic18Command {
  CORE_INST = 0,
  SHIFT_OUT_TABLAT = 2,
  TABLE_READ = 8,
  TABLE_READ_post_inc = 9,
  TABLE_READ_post_dec = 10,
  TABLE_READ_pre_inc = 11,
  TABLE_WRITE = 12,
  TABLE_WRITE_post_inc2 = 13,
  TABLE_WRITE_post_inc2_start_pgm = 14,
  TABLE_WRITE_start_pgm = 15,
};

// PIC16 command definitions. Commands up to and including INCREMENT_ADDRESS are the same across
// devices (if the command is supported). Other commands have varying semantics and codes.
enum Pic16Command {
  LOAD_CONFIGURATION = 0,
  LOAD_PROG_MEMORY = 2,
  LOAD_DATA_MEMORY = 3,
  READ_PROG_MEMORY = 4,
  READ_DATA_MEMORY = 5,
  INCREMENT_ADDRESS = 6,
};

enum Section {
  FLASH,
  USER_ID,
  CONFIGURATION,
  EEPROM,
};

enum EraseMode {
  CHIP_ERASE,
  SECTION_ERASE,
  ROW_ERASE,
  NO_ERASE,
};

enum Pins {
  nMCLR = (1 << 0),
  PGM = (1 << 1),
  PGC = (1 << 2),
  PGD = (1 << 3),
};

typedef std::chrono::nanoseconds Duration;
constexpr Duration ZeroDuration = std::chrono::nanoseconds(0);
static constexpr inline Duration MilliSeconds(uint64_t x) { return std::chrono::milliseconds(x); }
static constexpr inline Duration MicroSeconds(uint64_t x) { return std::chrono::microseconds(x); }
static constexpr inline Duration NanoSeconds(uint64_t x) { return std::chrono::nanoseconds(x); }

void Sleep(Duration duration);

#ifdef __GNUC__
void fatal(const char *fmt, ...) __attribute__((format(printf, 1, 2))) __attribute__((noreturn));
#else
/*@noreturn@*/ void fatal(const char *fmt, ...);
#endif
#define FATAL(fmt, ...) fatal("%s:%d: " fmt, __FILE__, __LINE__, __VA_ARGS__)

class AutoClosureRunner {
 public:
  AutoClosureRunner(std::function<void()> func) : func_(func) {}
  ~AutoClosureRunner() { func_(); }

 private:
  std::function<void()> func_;
};

template <class C, class T>
typename std::enable_if<!std::is_pod<T>::value, bool>::type ContainsKey(const C &c, const T &t) {
  return c.find(t) != c.end();
}
template <class C, class T>
typename std::enable_if<std::is_pod<T>::value, bool>::type ContainsKey(const C &c, T t) {
  return c.find(t) != c.end();
}

template <class T>
void set_subtract(std::set<T> *a, const std::set<T> &b) {
  for (const T &item : b) {
    a->erase(item);
  }
}

template <class T>
void set_intersect(std::set<T> *a, const std::set<T> &b) {
  std::vector<T> output;
  std::set_intersection(a->begin(), a->end(), b.begin(), b.end(), std::back_inserter(output));
  a->clear();
  a->insert(output.begin(), output.end());
}

const char *SectionToName(Section section);

std::string HexByte(uint8_t byte);
std::string HexUint16(uint16_t word);
std::string HexUint32(uint32_t address);

void print_msg(int level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

std::string Dirname(const std::string &str);

#endif
