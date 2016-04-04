#ifndef DRIVER_H_
#define DRIVER_H_

#include <cstdint>
#include <memory>

#include "sequence_generator.h"
#include "status.h"

class SequenceGenerator;

class Driver {
public:
	virtual ~Driver() = default;

	static std::unique_ptr<Driver> CreateFromFlags();

	virtual Status Open() = 0;
	virtual void Close() = 0;

	Status WriteTimedSequence(const TimedSequence &sequence);
	Status WriteDatastring(const datastring &data);
	virtual Status ReadWithSequence(const datastring &sequence, int bit_offset, int bit_count, uint32_t count, datastring *result) = 0;

protected:
	Driver() = default;
	virtual Status SetPins(uint8_t pins) = 0;
	virtual Status FlushOutput() = 0;

private:
	Driver(const Driver &) = delete;
	Driver(Driver &&) = delete;
	Driver &operator=(const Driver &) = delete;
	Driver &operator=(Driver &&) = delete;

};

#endif
