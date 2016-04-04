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
	Status WriteDatastring(const Datastring &data);
	virtual Status ReadWithSequence(const Datastring &sequence, int bit_offset, int bit_count, uint32_t count, Datastring16 *result) = 0;

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
