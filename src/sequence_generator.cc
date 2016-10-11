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

#include "strings.h"

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

Datastring Pic16SequenceGenerator::GetCommandSequence(uint8_t command) const {
  Datastring result;
  result += GenerateBitSequence(command, 6);
  return result;
}

std::vector<TimedStep> Pic16SequenceGenerator::GetTimedSequence(
    TimedSequenceType type, const DeviceInfo *device_info) const {
  switch (type) {
    case INIT_SEQUENCE:
      return GenerateInitSequence();
    case CHIP_ERASE_SEQUENCE:
      return TimedSequenceFromDatastring16(device_info->chip_erase_sequence,
                                           device_info->bulk_erase_timing);
    case ERASE_DATA_SEQUENCE:
      return TimedSequenceFromDatastring16(device_info->eeprom_erase_sequence,
                                           device_info->bulk_erase_timing);
    case WRITE_DATA_SEQUENCE:
      return TimedSequenceFromDatastring16(device_info->block_write_sequence,
                                           device_info->block_write_timing);
    default:
      FATAL("Requested unimplemented sequence %d\n", type);
  }
}

Status Pic16SequenceGenerator::ValidateSequence(const Datastring16 &sequence) {
  for (auto iter = sequence.begin(); iter != sequence.end(); ++iter) {
    Pic16Command step = static_cast<Pic16Command>(*iter);
    if (step == LOAD_CONFIGURATION || step == LOAD_PROG_MEMORY) {
      ++iter;
      if (iter == sequence.end()) {
        return Status(PARSE_ERROR, strings::Cat("Load command ", HexByte(step), " missing data"));
      }
    } else if (*iter == 0xff) {
      // Sleep. Do nothing.
    } else if (*iter > 0x3f) {
      return Status(PARSE_ERROR, strings::Cat("Invalid command value ", HexUint16(*iter)));
    }
  }
  return Status::OK;
}

std::vector<TimedStep> Pic16SequenceGenerator::TimedSequenceFromDatastring16(
    const Datastring16 &sequence, Duration timing) const {
  std::vector<TimedStep> result;
  Datastring step_string;
  for (auto iter = sequence.begin(); iter != sequence.end(); ++iter) {
    Pic16Command step = static_cast<Pic16Command>(*iter);
    if (step == LOAD_CONFIGURATION || step == LOAD_PROG_MEMORY) {
      ++iter;
      step_string += GetCommandSequence(step, *iter);
    } else if (*iter == 0xff) {
      result.push_back(TimedStep{step_string, timing});
      step_string.clear();
    } else {
      step_string += GetCommandSequence(*iter);
    }
  }
  if (!step_string.empty()) {
    result.push_back(TimedStep{step_string, MicroSeconds(0)});
  }
  return result;
}
