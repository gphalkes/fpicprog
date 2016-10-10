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
#include "device_db.h"

#include <regex>
#include <string>
#include <vector>

#include "interval_set.h"
#include "strings.h"

template <typename T>
static Status NumericalValue(const std::string &str, T *result) {
  char *str_end;
  long long value;
  errno = 0;
  if (str.back() == 'h') {
    value = strtoll(str.c_str(), &str_end, 16);
    if (str_end != str.c_str() + str.size() - 1) {
      return Status(PARSE_ERROR, "Invalid hex value");
    }
  } else {
    value = strtoll(str.c_str(), &str_end, 10);
    if (str_end != str.c_str() + str.size()) {
      return Status(PARSE_ERROR, "Invalid value");
    }
  }
  if (errno != 0 || value > std::numeric_limits<T>::max() ||
      value < std::numeric_limits<T>::min()) {
    return Status(PARSE_ERROR, "Value out of range");
  }
  *result = value;
  return Status::OK;
}

static Status SequenceValue(const std::string &str, Datastring16 *result) {
  std::vector<std::string> sequence = strings::Split<std::string>(str, ' ', false);
  for (const auto &value : sequence) {
    uint16_t parsed_value;
    RETURN_IF_ERROR(NumericalValue(value, &parsed_value));
    result->push_back(parsed_value);
  }
  return Status::OK;
}

static Status DurationValue(const std::string &str, Duration *result) {
  if (str.size() < 3) {
    return Status(PARSE_ERROR, "Invalid time value");
  }
  std::string unit = str.substr(str.size() - 2, std::string::npos);
  uint32_t duration;
  RETURN_IF_ERROR(NumericalValue(str.substr(0, str.size() - 2), &duration));
  if (unit == "ms") {
    *result = MilliSeconds(duration);
  } else if (unit == "us") {
    *result = MicroSeconds(duration);
  } else {
    return Status(PARSE_ERROR, "Invalid time unit");
  }
  return Status::OK;
}

static void MultiplyUnits(DeviceInfo *info, uint32_t unit_factor) {
  info->program_memory_size *= unit_factor;
  info->user_id_size *= unit_factor;
  info->user_id_offset *= unit_factor;
  info->config_size *= unit_factor;
  info->config_offset *= unit_factor;
  info->eeprom_size *= unit_factor;
  info->eeprom_offset *= unit_factor;
  info->write_block_size *= unit_factor;
  std::vector<uint32_t> missing_locations;
  for (uint32_t location : info->missing_locations) {
    for (uint32_t i = 0; i < unit_factor; ++i) {
      missing_locations.push_back(location * unit_factor + i);
    }
  }
  info->missing_locations = missing_locations;
}

Status DeviceDb::Load(const std::string &name) {
  FILE *in;
  if ((in = fopen(name.c_str(), "r")) == nullptr) {
    return Status(FILE_NOT_FOUND,
                  strings::Cat("Could not open device DB '", name, "': ", strerror(errno)));
  }

  std::string db;
  char buffer[1024];
  ssize_t bytes_read;
  while ((bytes_read = fread(buffer, 1, sizeof(buffer), in)) > 0) {
    db.append(buffer, bytes_read);
  }
  if (!feof(in) || ferror(in)) {
    return Status(PARSE_ERROR,
                  strings::Cat("Could not read device DB '", name, "': ", strerror(errno)));
  }

  std::vector<std::string> lines = strings::Split<std::string>(db, '\n', true);

  std::regex strip_regex(R"(\s*(.*?)\s*)");
  std::regex comment_regex(R"(#.*)");
  std::regex section_regex(R"(\[\s*((?:\w|/)+)\s*\]\w*)");
  std::regex key_value_regex(R"((\w+)\s*=\s*(.*))");
  DeviceInfo last_info;
  for (size_t i = 0; i < lines.size(); ++i) {
    std::smatch match_results;
    if (std::regex_match(lines[i], match_results, strip_regex) && match_results[1].matched) {
      lines[i] = match_results[1].str();
    }

    if (std::regex_match(lines[i], comment_regex) || lines[i].empty()) {
      // Skip comments
      continue;
    } else if (std::regex_match(lines[i], match_results, section_regex)) {
      if (!last_info.name.empty()) {
        RETURN_IF_ERROR(last_info.Validate());
        if (ContainsKey(device_db_, last_info.device_id)) {
          return Status(
              PARSE_ERROR,
              strings::Cat("Duplicate device ID ", HexUint16(last_info.device_id), " (",
                           last_info.name, ", ", device_db_[last_info.device_id].name, ")"));
        }
        MultiplyUnits(&last_info, unit_factor_);
        device_db_[last_info.device_id] = last_info;
      }
      last_info = DeviceInfo();
      last_info.name = match_results[1].str();
    } else if (std::regex_match(lines[i], match_results, key_value_regex)) {
      const std::string &key = match_results[1].str();
      const std::string &value = match_results[2].str();
      if (key == "device_id") {
        RETURN_IF_ERROR_WITH_APPEND(NumericalValue(value, &last_info.device_id),
                                    strings::Cat(" in device database at line ", i + 1));
      } else if (key == "program_memory_size") {
        RETURN_IF_ERROR_WITH_APPEND(NumericalValue(value, &last_info.program_memory_size),
                                    strings::Cat(" in device database at line ", i + 1));
      } else if (key == "user_id_size") {
        RETURN_IF_ERROR_WITH_APPEND(NumericalValue(value, &last_info.user_id_size),
                                    strings::Cat(" in device database at line ", i + 1));
      } else if (key == "user_id_offset") {
        RETURN_IF_ERROR_WITH_APPEND(NumericalValue(value, &last_info.user_id_offset),
                                    strings::Cat(" in device database at line ", i + 1));
      } else if (key == "config_size") {
        RETURN_IF_ERROR_WITH_APPEND(NumericalValue(value, &last_info.config_size),
                                    strings::Cat(" in device database at line ", i + 1));
      } else if (key == "config_offset") {
        RETURN_IF_ERROR_WITH_APPEND(NumericalValue(value, &last_info.config_offset),
                                    strings::Cat(" in device database at line ", i + 1));
      } else if (key == "eeprom_size") {
        RETURN_IF_ERROR_WITH_APPEND(NumericalValue(value, &last_info.eeprom_size),
                                    strings::Cat(" in device database at line ", i + 1));
      } else if (key == "eeprom_offset") {
        RETURN_IF_ERROR_WITH_APPEND(NumericalValue(value, &last_info.eeprom_offset),
                                    strings::Cat(" in device database at line ", i + 1));
      } else if (key == "write_block_size") {
        RETURN_IF_ERROR_WITH_APPEND(NumericalValue(value, &last_info.write_block_size),
                                    strings::Cat(" in device database at line ", i + 1));
      } else if (key == "chip_erase_sequence") {
        RETURN_IF_ERROR_WITH_APPEND(SequenceValue(value, &last_info.chip_erase_sequence),
                                    strings::Cat(" in device database at line ", i + 1));
      } else if (key == "flash_erase_sequence") {
        RETURN_IF_ERROR_WITH_APPEND(SequenceValue(value, &last_info.flash_erase_sequence),
                                    strings::Cat(" in device database at line ", i + 1));
      } else if (key == "user_id_erase_sequence") {
        RETURN_IF_ERROR_WITH_APPEND(SequenceValue(value, &last_info.user_id_erase_sequence),
                                    strings::Cat(" in device database at line ", i + 1));
      } else if (key == "config_erase_sequence") {
        RETURN_IF_ERROR_WITH_APPEND(SequenceValue(value, &last_info.config_erase_sequence),
                                    strings::Cat(" in device database at line ", i + 1));
      } else if (key == "eeprom_erase_sequence") {
        RETURN_IF_ERROR_WITH_APPEND(SequenceValue(value, &last_info.eeprom_erase_sequence),
                                    strings::Cat(" in device database at line ", i + 1));
      } else if (key == "bulk_erase_timing") {
        RETURN_IF_ERROR_WITH_APPEND(DurationValue(value, &last_info.bulk_erase_timing),
                                    strings::Cat(" in device database at line ", i + 1));
      } else if (key == "block_write_timing") {
        RETURN_IF_ERROR_WITH_APPEND(DurationValue(value, &last_info.block_write_timing),
                                    strings::Cat(" in device database at line ", i + 1));
      } else if (key == "missing_locations") {
        std::vector<std::string> sequence = strings::Split<std::string>(value, ' ', false);
        for (const auto &single_value : sequence) {
          uint32_t parsed_value;
          RETURN_IF_ERROR(NumericalValue(single_value, &parsed_value));
          last_info.missing_locations.push_back(parsed_value);
        }
      } else {
        return Status(PARSE_ERROR, strings::Cat("Device database has unknown key on line ", i + 1));
      }
    } else {
      return Status(PARSE_ERROR, strings::Cat("Device database read error on line ", i + 1));
    }
  }
  if (!last_info.name.empty()) {
    RETURN_IF_ERROR(last_info.Validate());
    if (ContainsKey(device_db_, last_info.device_id)) {
      return Status(PARSE_ERROR,
                    strings::Cat("Duplicate device ID ", HexUint16(last_info.device_id), " (",
                                 last_info.name, ", ", device_db_[last_info.device_id].name, ")"));
    }
    MultiplyUnits(&last_info, unit_factor_);
    device_db_[last_info.device_id] = last_info;
  }

  return Status::OK;
}

Status DeviceDb::GetDeviceInfo(uint16_t device_id, DeviceInfo *device_info) {
  if (device_db_.find(device_id) == device_db_.end()) {
    return Status(DEVICE_NOT_FOUND,
                  strings::Cat("Device with ID ", HexUint16(device_id), " not found"));
  }
  *device_info = device_db_.at(device_id);
  return Status::OK;
}

static void DumpSequence(const char *name, const Datastring16 &sequence) {
  printf("%s", name);
  for (const uint16_t elem : sequence) {
    printf(" %04Xh", elem);
  }
  printf("\n");
}

void DeviceInfo::Dump() const {
  printf("Name: %s\n", name.c_str());
  printf("Device ID: %04Xh\n", device_id);
  printf("Program memory size: %06Xh\n", program_memory_size);
  printf("User ID size: %d\n", user_id_size);
  printf("User ID offset: %06Xh\n", user_id_offset);
  printf("Config size: %d\n", config_size);
  printf("Config offset: %06Xh\n", config_offset);
  printf("EEPROM size: %d\n", eeprom_size);
  printf("EEPROM offset: %06Xh\n", eeprom_offset);
  printf("Write block size: %d\n", write_block_size);
  DumpSequence("Chip erase sequence:", chip_erase_sequence);
  DumpSequence("Flash erase sequence:", flash_erase_sequence);
  DumpSequence("User ID erase sequence:", user_id_erase_sequence);
  DumpSequence("Config erase sequence:", config_erase_sequence);
  DumpSequence("EEPROM erase sequence:", eeprom_erase_sequence);
  printf("Bulk erase timing: %ldns\n", bulk_erase_timing);
  printf("Block write timing: %ldns\n", block_write_timing);
  printf("Config write timing: %ldns\n", config_write_timing);
  printf("Missing locations:");
  for (const auto &location : missing_locations) {
    printf("%06Xh", location);
  }
  printf("\n");
}

Status DeviceInfo::Validate() const {
  if (device_id == 0) {
    return Status(PARSE_ERROR, strings::Cat(name, ": Device ID can not be 0"));
  }
  if (program_memory_size == 0) {
    return Status(PARSE_ERROR, strings::Cat(name, ": Program memory must be larger than 0"));
  }

  IntervalSet<uint32_t> used_intervals;
  used_intervals.Add(Interval<uint32_t>(0, program_memory_size));
  if (user_id_size > 0) {
    Interval<uint32_t> user_id_interval(user_id_offset, user_id_offset + user_id_size);
    if (used_intervals.Overlaps(user_id_interval)) {
      return Status(PARSE_ERROR, strings::Cat(name, ": User ID overlaps with other segments"));
    }
    used_intervals.Add(user_id_interval);
  }

  if (config_size > 0) {
    Interval<uint32_t> config_interval(config_offset, config_offset + config_size);
    if (used_intervals.Overlaps(config_interval)) {
      return Status(PARSE_ERROR,
                    strings::Cat(name, ": Configuration overlaps with other segments"));
    }
    used_intervals.Add(config_interval);
  }

  if (eeprom_size > 0) {
    Interval<uint32_t> eeprom_interval(eeprom_offset, eeprom_offset + eeprom_size);
    if (used_intervals.Overlaps(eeprom_interval)) {
      return Status(PARSE_ERROR, strings::Cat(name, ": EERPOM overlaps with other segments"));
    }
    used_intervals.Add(eeprom_interval);
  }
  return Status::OK;
}
