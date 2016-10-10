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
#include "program.h"

#include <gflags/gflags.h>
#include <limits>

#include "interval_set.h"
#include "strings.h"

DEFINE_int32(ihex_bytes_per_line, 16, "Number of bytes to write per line in an Intel HEX file.");

class IHexChecksum {
 public:
  IHexChecksum &operator<<(int data) {
    checksum_ += data & 0xff;
    return *this;
  }

  int Get() { return (-checksum_) & 0xff; }

 private:
  int32_t checksum_ = 0;
};

static int AscciToInt(int c) {
  if (c >= '0' && c <= '9') {
    return c - '0';
  } else if (c >= 'a' && c <= 'f') {
    return c - 'a' + 10;
  } else if (c >= 'A' && c <= 'F') {
    return c - 'A' + 10;
  }
  return -1;
}

static Status ReadAsciiByte(int line_number, FILE *in, uint8_t *byte) {
  int c = fgetc(in);
  if (c == EOF) {
    if (ferror(in)) {
      return Status(Code::PARSE_ERROR, strings::Cat("Error reading file at line ", line_number,
                                                    ": ", strerror(errno)));
    }
    return Status(Code::PARSE_ERROR, strings::Cat("Unexpected end-of-file at line ", line_number));
  }
  int nibble = AscciToInt(c);
  if (nibble < 0) {
    return Status(Code::PARSE_ERROR, strings::Cat("Unexpected character ", std::string(1, c),
                                                  " at line ", line_number));
  }
  *byte = nibble << 4;

  c = fgetc(in);
  if (c == EOF) {
    if (ferror(in)) {
      return Status(Code::PARSE_ERROR, strings::Cat("Error reading file at line ", line_number,
                                                    ": ", strerror(errno)));
    }
    return Status(Code::PARSE_ERROR, strings::Cat("Unexpected end-of-file at line ", line_number));
  }
  nibble = AscciToInt(c);
  if (nibble < 0) {
    return Status(Code::PARSE_ERROR,
                  strings::Cat("Unexpected character ", strings::CEscape(std::string(1, c)),
                               " at line ", line_number));
  }
  *byte |= nibble;
  return Status::OK;
}

Status ReadIhex(Program *program, FILE *in) {
  uint32_t high_address = 0;
  for (int line_number = 1;; ++line_number) {
    int c;
    c = fgetc(in);
    if (c == EOF) {
      if (ferror(in)) {
        return Status(Code::PARSE_ERROR, strings::Cat("Error reading file at line ", line_number,
                                                      ": ", strerror(errno)));
      }
      return Status(Code::PARSE_ERROR,
                    strings::Cat("Unexpected end-of-file at line ", line_number));
    } else if (c != ':') {
      return Status(Code::PARSE_ERROR,
                    strings::Cat("Did not find : at start of line ", line_number));
    }

    IHexChecksum running_checksum;
    uint8_t byte_count;
    RETURN_IF_ERROR(ReadAsciiByte(line_number, in, &byte_count));
    running_checksum << byte_count;
    uint8_t data;
    RETURN_IF_ERROR(ReadAsciiByte(line_number, in, &data));
    running_checksum << data;
    uint16_t offset = data;
    offset <<= 8;
    RETURN_IF_ERROR(ReadAsciiByte(line_number, in, &data));
    running_checksum << data;
    offset |= data;
    uint8_t record_type;
    RETURN_IF_ERROR(ReadAsciiByte(line_number, in, &record_type));
    running_checksum << record_type;
    Datastring bytes;
    bytes.reserve(byte_count);
    for (uint8_t i = 0; i < byte_count; ++i) {
      RETURN_IF_ERROR(ReadAsciiByte(line_number, in, &data));
      bytes.push_back(data);
      running_checksum << data;
    }
    uint8_t checksum;
    RETURN_IF_ERROR(ReadAsciiByte(line_number, in, &checksum));
    if (running_checksum.Get() != checksum) {
      return Status(Code::PARSE_ERROR, strings::Cat("Checksum incorrect at line ", line_number,
                                                    " (Found ", HexByte(checksum), ", calculated ",
                                                    HexByte(running_checksum.Get()), ")"));
    }
    switch (record_type) {
      case 0x00:
        (*program)[high_address + offset] = bytes;
        break;
      case 0x01: {
        while (true) {
          c = fgetc(in);
          if (c == EOF) {
            break;
          } else if (c != '\n' && c != ' ') {
            fprintf(stderr, "Warning: extra data after EOF record in IHEX file.");
            break;
          }
        }

        uint32_t last_start = 0;
        uint32_t last_end = 0;
        for (const auto &section : *program) {
          if (last_end > section.first) {
            return Status(
                Code::PARSE_ERROR,
                strings::Cat("Overlapping program parts in IHEX file (", HexUint32(last_start), "-",
                             HexUint32(last_end), " and ", HexUint32(section.first), "-",
                             HexUint32(section.first + section.second.size()), ")"));
          }
          last_start = section.first;
          last_end = section.first + section.second.size();
        }
        return Status::OK;
      }
      case 0x04:
        if (bytes.size() != 2) {
          return Status(Code::PARSE_ERROR,
                        strings::Cat("Invalid size for type 04 (extended linear address) at line ",
                                     line_number, " (", bytes.size(), " instead of 2)"));
        }
        high_address = bytes[0];
        high_address <<= 8;
        high_address |= bytes[1];
        high_address <<= 16;
        break;
      default:
        return Status(Code::PARSE_ERROR,
                      strings::Cat("Unsupported record type ", HexByte(record_type), " at line ",
                                   line_number));
    }
    c = fgetc(in);
    if (c == EOF) {
      return Status(Code::PARSE_ERROR,
                    strings::Cat("Unexpected end-of-file at line ", line_number));
    } else if (c != '\n') {
      return Status(Code::PARSE_ERROR,
                    strings::Cat("Unexpected character ", strings::CEscape(std::string(1, c)),
                                 " at line ", line_number));
    }
  }
}

void WriteIhex(const Program &program, FILE *out) {
  int bytes_per_line = std::max(1, std::min<int>(FLAGS_ihex_bytes_per_line, 255));

  uint32_t last_address = std::numeric_limits<uint32_t>::max();
  for (const auto &section : program) {
    size_t section_size = section.second.size();
    uint32_t section_offset = section.first;
    for (size_t idx = 0; idx < section_size;) {
      uint32_t next_offset = section_offset + idx;
      if ((next_offset >> 16) != (last_address >> 16)) {
        fprintf(out, ":02000004%04X%02X\n", next_offset >> 16,
                (IHexChecksum() << 2 << 4 << (next_offset >> 24) << (next_offset >> 16)).Get());
      }
      uint32_t line_length =
          std::min<uint32_t>(bytes_per_line, ((next_offset + 0x10000) & 0xffff0000) - next_offset);
      if (line_length + idx > section_size) {
        line_length = section_size - idx;
      }
      IHexChecksum checksum;
      checksum << line_length << (next_offset >> 8) << next_offset;
      fprintf(out, ":%02X%04X00", line_length, next_offset & 0xffff);
      for (uint32_t i = 0; i < line_length; ++i, ++idx) {
        fprintf(out, "%02X", section.second[idx]);
        checksum << section.second[idx];
      }
      fprintf(out, "%02X\n", checksum.Get());
      last_address = next_offset;
    }
  }
  fprintf(out, ":00000001FF\n");
}

Status MergeProgramBlocks(Program *program, const DeviceInfo &device_info) {
  std::set<uint32_t> section_boundaries;
  section_boundaries.insert(device_info.user_id_offset);
  section_boundaries.insert(device_info.config_offset);
  section_boundaries.insert(device_info.eeprom_offset);

  auto last_section = program->begin();
  auto iter = last_section;
  for (++iter; iter != program->end();) {
    uint32_t last_section_end = last_section->first + last_section->second.size();

    if (last_section_end < iter->first) {
      last_section = iter;
      ++iter;
      continue;
    } else if (last_section_end == iter->first &&
               !ContainsKey(section_boundaries, last_section_end)) {
      last_section->second.append(iter->second);
      iter = program->erase(iter);
    } else if (last_section_end > iter->first) {
      return Status(Code::INVALID_PROGRAM, "Overlapping sections in program");
    }
  }

  std::vector<Interval<uint32_t>> device_sections;
  device_sections.emplace_back(0, device_info.program_memory_size);
  device_sections.emplace_back(device_info.user_id_offset,
                               device_info.user_id_offset + device_info.user_id_size);
  device_sections.emplace_back(device_info.config_offset,
                               device_info.config_offset + device_info.config_size);
  device_sections.emplace_back(device_info.eeprom_offset,
                               device_info.eeprom_offset + device_info.eeprom_size);

  for (const auto &section : *program) {
    bool contained = false;
    for (const auto &interval : device_sections) {
      if (interval.Contains(
              Interval<uint32_t>(section.first, section.first + section.second.size()))) {
        contained = true;
        break;
      }
    }
    if (!contained) {
      return Status(Code::INVALID_PROGRAM,
                    strings::Cat("Data outside device memory or crossing section boundaries: ",
                                 HexUint32(section.first), "-",
                                 HexUint32(section.first + section.second.size())));
    }
  }
  return Status::OK;
}

void RemoveMissingConfigBytes(Program *program, const DeviceInfo &device_info) {
  for (auto missing_address : device_info.missing_locations) {
    auto iter = std::upper_bound(program->begin(), program->end(), missing_address,
                                 [](uint32_t address, const auto &section) {
                                   return address < section.first + section.second.size();
                                 });
    if (iter == program->end() || iter->first > missing_address) {
      continue;
    }
    if (missing_address >= iter->first && missing_address < iter->first + iter->second.size()) {
      Datastring first = iter->second.substr(0, missing_address - iter->first);
      Datastring second = iter->second.substr(missing_address + 1 - iter->first, Datastring::npos);
      if (!second.empty()) {
        (*program)[missing_address + 1] = second;
      }
      if (first.empty()) {
        program->erase(iter);
      } else {
        iter->second = first;
      }
    }
  }
}
