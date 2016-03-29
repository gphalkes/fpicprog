#include "sequence_generator.h"

datastring SequenceGenerator::GenerateCommand(Command command, uint16_t payload) const {
	datastring result;
	result += GenerateBitSequence(command, 4);
	result += GenerateBitSequence(payload, 16);
	return result;
}

Pic18SequenceGenerator::~Pic18SequenceGenerator() {}

datastring Pic18SequenceGenerator::GenerateBitSequence(uint32_t data, int bits) const {
	datastring result;
	for (int i = 0; i < bits; ++i) {
		bool bit_set = (data >> i) & 1;
		uint8_t base = nMCLR;
		if (uses_pgm_) base |= PGM;
		result.push_back(base | PGC | (bit_set ? PGD : 0));
		result.push_back(base | (bit_set ? PGD : 0));
	}
	return result;
}

std::vector<SequenceGenerator::TimedStep> KeySequenceGenerator::GetTimedSequence(TimedSequenceType type) const {
	std::vector<TimedStep> result;
	constexpr int base = nMCLR | PGM;
//	if (uses_pgm_) base |= PGM;

	switch (type) {
		case INIT_SEQUENCE:
			result.push_back(TimedStep{{0, nMCLR, 0}, MilliSeconds(1)});
			{
				datastring magic;
				uint32_t key = 0x4D434850;
				for (int i = 31; i >= 0; --i) {
					bool bit_set = (key >> i) & 1;
					magic.push_back(bit_set ? PGD : 0);
					magic.push_back(PGC | (bit_set ? PGD : 0));
				}
				// Needs to be held for 40ns. Even at 25M symbols per second, this is only a single symbol.
				magic.push_back(0);
				magic.push_back(nMCLR);
				result.push_back(TimedStep{magic, MicroSeconds(400)});
			}
			break;
		case BULK_ERASE_SEQUENCE:
			result.push_back(TimedStep{{base | PGC, base, base | PGC, base, base | PGC, base, base | PGC, base}, MilliSeconds(16)}); // FIXME: this is device dependent. 15ms is for 18f45k50
			result.push_back(TimedStep{GenerateBitSequence(0, 16), base});
			break;
		case WRITE_SEQUENCE:
			result.push_back(TimedStep{{base | PGC, base, base | PGC, base, base | PGC, base, base | PGC}, MilliSeconds(1)});
			result.push_back(TimedStep{{0}, MicroSeconds(200)});
			result.push_back(TimedStep{GenerateBitSequence(0, 16), base});
			break;
		case WRITE_CONFIG_SEQUENCE:
			result.push_back(TimedStep{{base | PGC, base, base | PGC, base, base | PGC, base, base | PGC}, MilliSeconds(5)});
			result.push_back(TimedStep{{0}, MicroSeconds(200)});
			result.push_back(TimedStep{GenerateBitSequence(0, 16), base});
			break;
	}
	return result;
}


datastring KeySequenceGenerator::GenerateInitSequence(int part) const {
	switch (part) {
		case 0:
			return {0, nMCLR, 0};
		case 1: {
			datastring result;
			uint32_t key = 0x4D434850;
			for (int i = 31; i >= 0; --i) {
				bool bit_set = (key >> i) & 1;
				result.push_back(bit_set ? PGD : 0);
				result.push_back(PGC | (bit_set ? PGD : 0));
			}
			// Needs to be held for 40ns. Even at 25M symbols per second, this is only a single symbol.
			result.push_back(0);
			result.push_back(nMCLR);
			return result;
		}
		default:
			return datastring();
	}
}

Duration KeySequenceGenerator::GetSleepDuration(int part) const {
	switch (part) {
		case 0:
			return MilliSeconds(1);
		case 1:
			return MicroSeconds(400);
		default:
			return 0;
	}
}

int KeySequenceGenerator::InitSequenceSteps() const {
	return 2;
}
