#ifndef DRIVER_H_
#define DRIVER_H_

#include <cstdint>
#include <memory>

#include "sequence_generator.h"
#include "util.h"

class SequenceGenerator;

class Driver {
public:
	virtual ~Driver() = default;

	static std::unique_ptr<Driver> CreateFromFlags(std::unique_ptr<SequenceGenerator> sequence_generator);

	void WriteTimedSequence(SequenceGenerator::TimedSequenceType type);
	void WriteCommand(Command command, uint16_t payload);
	virtual datastring ReadWithCommand(Command command, uint32_t count);

protected:
	Driver(std::unique_ptr<SequenceGenerator> sequence_generator) : sequence_generator_(std::move(sequence_generator)) {}

	virtual void EnableDataWrite() = 0;
	virtual void EnableDataRead() = 0;
	virtual void SetPins(uint8_t pins) = 0;
	virtual void FlushOutput() = 0;
	virtual uint8_t GetValue() { FATAL("GetValue not implemented%s\n", ""); }

	std::unique_ptr<SequenceGenerator> sequence_generator_;

private:
	Driver(const Driver &) = delete;
	Driver(Driver &&) = delete;
	Driver &operator=(const Driver &) = delete;
	Driver &operator=(Driver &&) = delete;

	void WriteDatastring(const datastring &data);
};

#endif
