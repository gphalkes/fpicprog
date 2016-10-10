/* Copyright (C) 2016 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#include "sequence_generator.h"

Datastring PicSequenceGenerator::GenerateBitSequence(uint32_t data, int bits) const {
  Datastring result;
  const uint8_t base = nMCLR | PGM;
  for (int i = 0; i < bits; ++i) {
    bool bit_set = (data >> i) & 1;
    result.push_back(base | PGC | (bit_set ? PGD : 0));
    result.push_back(base | (bit_set ? PGD : 0));
  }
  return result;
}

std::vector<TimedStep> PicSequenceGenerator::GenerateInitSequence() const {
  std::vector<TimedStep> result;
  // This sequence combines the requirements for both the two and the three pin programming.
  // It inserts a little extra wait, but that is a small price to pay for the extra convenience
  // of having only a single sequence.
  result.push_back(TimedStep{{0, nMCLR, 0}, MilliSeconds(10)});
  {
    Datastring magic;
    uint32_t key = 0x4D434850;  // MCHP
    for (int i = 31; i >= 0; --i) {
      bool bit_set = (key >> i) & 1;
      magic.push_back(bit_set ? PGD : 0);
      magic.push_back(PGC | (bit_set ? PGD : 0));
    }
    // Needs to be held for 40ns for the three-pin sequence, but for several microseconds for
    // the two-pin version.
    magic.push_back(PGM);
    result.push_back(TimedStep{magic, MicroSeconds(20)});
  }
  result.push_back(TimedStep{{PGM | nMCLR}, MicroSeconds(400)});
  return result;
}

Datastring Pic18SequenceGenerator::GetCommandSequence(Pic18Command command,
                                                      uint16_t payload) const {
  Datastring result;
  result += GenerateBitSequence(command, 4);
  result += GenerateBitSequence(payload, 16);
  return result;
}

std::vector<TimedStep> Pic18SequenceGenerator::GetTimedSequence(
    TimedSequenceType type, const DeviceInfo *device_info) const {
  std::vector<TimedStep> result;
  constexpr int base = nMCLR | PGM;

  switch (type) {
    case INIT_SEQUENCE:
      result = GenerateInitSequence();
      break;
    case BULK_ERASE_SEQUENCE:
      result.push_back(
          TimedStep{{base | PGC, base, base | PGC, base, base | PGC, base, base | PGC, base},
                    device_info ? device_info->bulk_erase_timing : MilliSeconds(500)});
      result.push_back(TimedStep{GenerateBitSequence(0, 16), 0});
      break;
    case WRITE_SEQUENCE:
      result.push_back(TimedStep{{base | PGC, base, base | PGC, base, base | PGC, base, base | PGC},
                                 device_info ? device_info->block_write_timing : MilliSeconds(1)});
      result.push_back(TimedStep{{base}, MicroSeconds(200)});
      result.push_back(TimedStep{GenerateBitSequence(0, 16), 0});
      break;
    case WRITE_CONFIG_SEQUENCE:
      result.push_back(TimedStep{{base | PGC, base, base | PGC, base, base | PGC, base, base | PGC},
                                 device_info ? device_info->config_write_timing : MilliSeconds(1)});
      result.push_back(TimedStep{{base}, MicroSeconds(200)});
      result.push_back(TimedStep{GenerateBitSequence(0, 16), 0});
      break;
    default:
      FATAL("Requested unimplemented sequence %d\n", type);
  }
  return result;
}

Datastring Pic16SequenceGenerator::GetCommandSequence(Pic16Command command,
                                                      uint16_t payload) const {
  Datastring result;
  result += GenerateBitSequence(command, 6);
  result += GenerateBitSequence(0, 1);
  result += GenerateBitSequence(payload, 14);
  result += GenerateBitSequence(0, 1);
  return result;
}

Datastring Pic16SequenceGenerator::GetCommandSequence(Pic16Command command) const {
  Datastring result;
  result += GenerateBitSequence(command, 6);
  return result;
}

std::vector<TimedStep> Pic16SequenceGenerator::GetTimedSequence(
    TimedSequenceType type, const DeviceInfo *device_info) const {
  std::vector<TimedStep> result;

  switch (type) {
    case INIT_SEQUENCE:
      result = GenerateInitSequence();
      break;
    case CHIP_ERASE:
// FIXME: make this generic!
#warning FIX THIS!
      result.push_back(
          TimedStep{GetCommandSequence(::BULK_ERASE_PROGRAM), device_info->bulk_erase_timing});
      result.push_back(
          TimedStep{GetCommandSequence(::BULK_ERASE_DATA), device_info->bulk_erase_timing});
      break;
    case BULK_ERASE_DATA:
      result.push_back(
          TimedStep{GetCommandSequence(::BULK_ERASE_DATA), device_info->bulk_erase_timing});
      break;
    case WRITE_DATA:
// FIXME: make this generic!
#warning FIX THIS!
      result.push_back(TimedStep{GetCommandSequence(::BEGIN_PROGRAMMING_INT), MilliSeconds(6)});
      break;
    default:
      FATAL("Requested unimplemented sequence %d\n", type);
  }
  return result;
}
