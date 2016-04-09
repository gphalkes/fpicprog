#include "sequence_generator.h"

Datastring Pic18SequenceGenerator::GetCommandSequence(Pic18Command command,
                                                      uint16_t payload) const {
  Datastring result;
  result += GenerateBitSequence(command, 4);
  result += GenerateBitSequence(payload, 16);
  return result;
}

std::vector<TimedStep> Pic18SequenceGenerator::GetTimedSequence(TimedSequenceType type) const {
  std::vector<TimedStep> result;
  constexpr int base = nMCLR | PGM;

  switch (type) {
    case BULK_ERASE_SEQUENCE:
      result.push_back(
          TimedStep{{base | PGC, base, base | PGC, base, base | PGC, base, base | PGC, base},
                    MilliSeconds(16)});  // FIXME: this is device dependent. 15ms is for 18f45k50
      result.push_back(TimedStep{GenerateBitSequence(0, 16), base});
      break;
    case WRITE_SEQUENCE:
      result.push_back(TimedStep{{base | PGC, base, base | PGC, base, base | PGC, base, base | PGC},
                                 MilliSeconds(1)});
      result.push_back(TimedStep{{base}, MicroSeconds(200)});
      result.push_back(TimedStep{GenerateBitSequence(0, 16), base});
      break;
    case WRITE_CONFIG_SEQUENCE:
      result.push_back(TimedStep{{base | PGC, base, base | PGC, base, base | PGC, base, base | PGC},
                                 MilliSeconds(5)});
      result.push_back(TimedStep{{base}, MicroSeconds(200)});
      result.push_back(TimedStep{GenerateBitSequence(0, 16), base});
      break;
    default:
      FATAL("Requested unimplemented sequence %d\n", type);
  }
  return result;
}

Datastring Pic18SequenceGenerator::GenerateBitSequence(uint32_t data, int bits) const {
  Datastring result;
  for (int i = 0; i < bits; ++i) {
    bool bit_set = (data >> i) & 1;
    uint8_t base = nMCLR | PGM;
    result.push_back(base | PGC | (bit_set ? PGD : 0));
    result.push_back(base | (bit_set ? PGD : 0));
  }
  return result;
}

std::vector<TimedStep> PgmSequenceGenerator::GetTimedSequence(TimedSequenceType type) const {
  if (type == INIT_SEQUENCE) {
    std::vector<TimedStep> result;
    result.push_back(TimedStep{{0, PGM, 0}, MicroSeconds(2)});
    result.push_back(TimedStep{{0, nMCLR | PGM, 0}, MicroSeconds(2)});
    return result;
  } else {
    return Pic18SequenceGenerator::GetTimedSequence(type);
  }
}

std::vector<TimedStep> KeySequenceGenerator::GetTimedSequence(TimedSequenceType type) const {
  if (type == INIT_SEQUENCE) {
    std::vector<TimedStep> result;
    result.push_back(TimedStep{{0, nMCLR, 0}, MilliSeconds(1)});
    {
      Datastring magic;
      uint32_t key = 0x4D434850;  // MCHP
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
    return result;
  } else {
    return Pic18SequenceGenerator::GetTimedSequence(type);
  }
}
