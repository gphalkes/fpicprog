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
