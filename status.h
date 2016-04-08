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

#define RETURN_IF_ERROR(x) do { Status _x = (x); if (!_x.ok()) return _x; } while (0)
// FIXME: print the error code as well.
#define CHECK_OK(x) do { Status _x = (x); if (!_x.ok()) { fatal("Check failed: " #x ": %s\n", _x.message().c_str()); } } while(0)
#endif
