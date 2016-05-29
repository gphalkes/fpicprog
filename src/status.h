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
#ifndef STATUS_H_
#define STATUS_H_

#include <string>

#include "util.h"

enum Code {
  OK,
  INIT_FAILED,
  SYNC_LOST,
  DEVICE_NOT_FOUND,
  USB_WRITE_ERROR,
  INVALID_PROGRAM,
  UNIMPLEMENTED,
  INVALID_ARGUMENT,
  PARSE_ERROR,
  VERIFICATION_ERROR,
  FILE_NOT_FOUND,
};

class __attribute__((warn_unused_result)) Status {
 public:
  Status() : Status(OK) {}
  Status(Code code, std::string message) : code_(code), message_(message) {}

  void Update(const Status &other) {
    if (code_ == Code::OK) {
      *this = other;
    }
  }

  bool ok() const { return code_ == Code::OK; }
  void IgnoreResult() {}
  Code code() const { return code_; }
  std::string message() const { return message_; }

  static const Status OK;

 private:
  Code code_;
  std::string message_;
};

#define RETURN_IF_ERROR(x)   \
  do {                       \
    Status _x = (x);         \
    if (!_x.ok()) return _x; \
  } while (0)
#define RETURN_IF_ERROR_WITH_APPEND(x, m)                     \
  do {                                                        \
    Status _x = (x);                                          \
    if (!_x.ok()) return Status(_x.code(), _x.message() + m); \
  } while (0)

#define CHECK_OK(x)                                                                   \
  do {                                                                                \
    Status _x = (x);                                                                  \
    if (!_x.ok()) {                                                                   \
      fatal("%s:%d: (%d) %s\n", __FILE__, __LINE__, _x.code(), _x.message().c_str()); \
    }                                                                                 \
  } while (0)
#endif
