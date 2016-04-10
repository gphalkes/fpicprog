#include "device_db.h"

#include <regex>
#include <string>
#include <vector>

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
  std::regex section_regex(R"(\[\s*(\w+)\s*\]\w*)");
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
        // FIXME: validate device info.
        device_db_[last_info.device_id] = last_info;
      }
      last_info = DeviceInfo();
      last_info.name = match_results[1].str();
    } else if (std::regex_match(lines[i], match_results, key_value_regex)) {
      const std::string &key = match_results[1].str();
      const std::string &value = match_results[2].str();
      if (key == "device_id") {
        RETURN_IF_ERROR(NumericalValue(value, &last_info.device_id));
      } else if (key == "program_memory_size") {
        RETURN_IF_ERROR(NumericalValue(value, &last_info.program_memory_size));
      } else if (key == "user_id_size") {
        RETURN_IF_ERROR(NumericalValue(value, &last_info.user_id_size));
      } else if (key == "user_id_offset") {
        RETURN_IF_ERROR(NumericalValue(value, &last_info.user_id_offset));
      } else if (key == "config_size") {
        RETURN_IF_ERROR(NumericalValue(value, &last_info.config_size));
      } else if (key == "config_offset") {
        RETURN_IF_ERROR(NumericalValue(value, &last_info.config_offset));
      } else if (key == "eeprom_size") {
        RETURN_IF_ERROR(NumericalValue(value, &last_info.eeprom_size));
      } else if (key == "eeprom_offset") {
        RETURN_IF_ERROR(NumericalValue(value, &last_info.eeprom_offset));
      } else if (key == "write_block_size") {
        RETURN_IF_ERROR(NumericalValue(value, &last_info.write_block_size));
      } else if (key == "erase_block_size") {
        RETURN_IF_ERROR(NumericalValue(value, &last_info.erase_block_size));
      } else if (key == "chip_erase_sequence") {
        RETURN_IF_ERROR(SequenceValue(value, &last_info.chip_erase_sequence));
      } else if (key == "flash_erase_sequence") {
        RETURN_IF_ERROR(SequenceValue(value, &last_info.flash_erase_sequence));
      } else if (key == "user_id_erase_sequence") {
        RETURN_IF_ERROR(SequenceValue(value, &last_info.user_id_erase_sequence));
      } else if (key == "config_erase_sequence") {
        RETURN_IF_ERROR(SequenceValue(value, &last_info.config_erase_sequence));
      } else if (key == "eeprom_erase_sequence") {
        RETURN_IF_ERROR(SequenceValue(value, &last_info.eeprom_erase_sequence));
      } else if (key == "bulk_erase_timing") {
        RETURN_IF_ERROR(DurationValue(value, &last_info.bulk_erase_timing));
      } else {
        return Status(PARSE_ERROR, strings::Cat("Device database has unknown key on line ", i + 1));
      }
    } else {
      return Status(PARSE_ERROR, strings::Cat("Device database read error on line ", i + 1));
    }
  }
  if (!last_info.name.empty()) {
    // FIXME: validate device info.
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
  printf("Erase block size: %d\n", erase_block_size);
  DumpSequence("Chip erase sequence:", chip_erase_sequence);
  DumpSequence("Flash erase sequence:", flash_erase_sequence);
  DumpSequence("User ID erase sequence:", user_id_erase_sequence);
  DumpSequence("Config erase sequence:", config_erase_sequence);
  DumpSequence("EEPROM erase sequence:", eeprom_erase_sequence);
  printf("Bulk erase timing: %ldns\n", bulk_erase_timing);
}
