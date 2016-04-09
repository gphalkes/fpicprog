#ifndef SEQUENCE_GENERATOR_H_
#define SEQUENCE_GENERATOR_H_

#include <string>
#include <vector>

#include "util.h"

struct TimedStep {
  Datastring data;
  Duration sleep;
};

typedef std::vector<TimedStep> TimedSequence;

class Pic18SequenceGenerator {
 public:
  enum TimedSequenceType {
    INIT_SEQUENCE,
    BULK_ERASE_SEQUENCE,
    WRITE_SEQUENCE,
    WRITE_CONFIG_SEQUENCE,
  };

  Datastring GetCommandSequence(Pic18Command command, uint16_t payload) const;
  virtual std::vector<TimedStep> GetTimedSequence(TimedSequenceType type) const;
  virtual ~Pic18SequenceGenerator() = default;

 private:
  Datastring GenerateBitSequence(uint32_t data, int bits) const;
};

#endif
