#ifndef SEQUENCE_GENERATOR_H_
#define SEQUENCE_GENERATOR_H_

#include <string>
#include <vector>

#include "util.h"

typedef std::basic_string<uint8_t> datastring;

struct TimedStep {
	datastring data;
	Duration sleep;
};

typedef std::vector<TimedStep> TimedSequence;

class Pic18SequenceGenerator {
public:
	enum TimedSequenceType {
		INIT_SEQUENCE,
		CHIP_ERASE_SEQUENCE,
		WRITE_SEQUENCE,
		WRITE_CONFIG_SEQUENCE,
	};

	datastring GetCommandSequence(Command command, uint16_t payload) const;
	virtual std::vector<TimedStep> GetTimedSequence(TimedSequenceType type) const;
	virtual ~Pic18SequenceGenerator() = default;

private:
	datastring GenerateBitSequence(uint32_t data, int bits) const;
};

class PgmSequenceGenerator : public Pic18SequenceGenerator {
public:
	std::vector<TimedStep> GetTimedSequence(TimedSequenceType type) const override;
};

class KeySequenceGenerator : public Pic18SequenceGenerator {
public:
	std::vector<TimedStep> GetTimedSequence(TimedSequenceType type) const override;
};

#endif
