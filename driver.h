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

	static std::unique_ptr<Driver> CreateFromFlags(std::unique_ptr<SequenceGenerator> sequence_generator);

	virtual Status Open() = 0;
	virtual void Close() = 0;

	Status WriteTimedSequence(SequenceGenerator::TimedSequenceType type);
	Status WriteCommand(Command command, uint16_t payload);
	virtual Status ReadWithCommand(Command command, uint32_t count, datastring *result) = 0;

protected:
	Driver(std::unique_ptr<SequenceGenerator> sequence_generator) : sequence_generator_(std::move(sequence_generator)) {}

	Status WriteDatastring(const datastring &data);

	virtual Status SetPins(uint8_t pins) = 0;
	virtual Status FlushOutput() = 0;

	std::unique_ptr<SequenceGenerator> sequence_generator_;

private:
	Driver(const Driver &) = delete;
	Driver(Driver &&) = delete;
	Driver &operator=(const Driver &) = delete;
	Driver &operator=(Driver &&) = delete;

};

#endif
