#ifndef STATUS_H_
#define STATUS_H_

#include <string>

enum Code {
	OK,
	INIT_FAILED,
	SYNC_LOST,
	DEVICE_NOT_FOUND,
	USB_WRITE_ERROR,
};

class __attribute__((warn_unused_result)) Status {
public:
	Status(Code code, std::string message) : code_(code), message_(message) {}

	bool ok() const { return code_ == Code::OK; }
	void IgnoreResult() {}
	std::string message() const { return message_; }

	static const Status OK;
private:
	Code code_;
	std::string message_;
};

#define RETURN_IF_ERROR(x) do { Status _x = (x); if (!_x.ok()) return _x; } while (0)
// FIXME: print the error code as well.
#define CHECK_OK(x) do { Status _x = (x); if (!_x.ok()) { fprintf(stderr, "Check failed: " #x ": %s\n", _x.message().c_str()); abort(); } } while(0)
#endif
