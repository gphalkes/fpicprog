#ifndef SEQUENCE_GENERATOR_H_
#define SEQUENCE_GENERATOR_H_

#include <string>
#include <vector>

#include "util.h"

typedef std::basic_string<uint8_t> datastring;

class SequenceGenerator {
public:
	virtual ~SequenceGenerator() = default;
	virtual datastring GenerateCommand(Command, uint16_t payload) const;
	virtual datastring GenerateBitSequence(uint32_t data, int bits) const = 0;

	virtual datastring GenerateInitSequence(int part) const = 0;
	virtual Duration GetSleepDuration(int part) const = 0;
	virtual int InitSequenceSteps() const = 0;

	struct TimedStep {
		datastring data;
		Duration sleep;
	};

	enum TimedSequenceType {
		INIT_SEQUENCE,
		BULK_ERASE_SEQUENCE,
		WRITE_SEQUENCE,
		WRITE_CONFIG_SEQUENCE,
	};

	virtual std::vector<TimedStep> GetTimedSequence(TimedSequenceType type) const = 0;
};

class Pic18SequenceGenerator : public SequenceGenerator {
public:
	~Pic18SequenceGenerator() override;

	datastring GenerateBitSequence(uint32_t data, int bits) const override;
protected:
	Pic18SequenceGenerator(bool uses_pgm) : uses_pgm_(uses_pgm) {}
private:
	bool uses_pgm_;
};

class PgmSequenceGenerator : public Pic18SequenceGenerator {
public:
	PgmSequenceGenerator() : Pic18SequenceGenerator(true) {}
//	std::vector<TimedStep> GetTimedSequence(TimedSequenceType type) const override;

	datastring GenerateInitSequence(int) const override { return datastring(); }
	virtual Duration GetSleepDuration(int) const override { return 0; }
	virtual int InitSequenceSteps() const override { return 0; }
};

class KeySequenceGenerator : public Pic18SequenceGenerator {
public:
	KeySequenceGenerator() : Pic18SequenceGenerator(false) {}
	std::vector<TimedStep> GetTimedSequence(TimedSequenceType type) const override;

	datastring GenerateInitSequence(int part) const override;
	virtual Duration GetSleepDuration(int part) const override;
	virtual int InitSequenceSteps() const override;
};

#endif
