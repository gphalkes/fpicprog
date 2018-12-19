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
#include <cerrno>
#include <cstring>
#include <gflags/gflags.h>
#include <memory>

#include "device_db.h"
#include "program.h"
#include "status.h"
#include "strings.h"

DEFINE_string(family, "",
              "Device family to use. One of pic10, pic10-small, pic12, pic12-small, pic16, "
              "pic16-small, pic18.");
DEFINE_string(device, "", "Name of the device to write a test program for.");

DEFINE_string(output, "", "File to write the Intel HEX data to.");
DEFINE_string(device_db, "",
              "Device DB file to load. Defaults to "
#if defined(DEVICE_DB_PATH)
              DEVICE_DB_PATH "/<family>.lst.");
#else
              "<path to binary>/device_db/<family>.lst.");
#endif

DEFINE_string(config_data, "",
              "Configuration data to write into the program. This can not be auto-generated "
              "because setting the wrong bits may turn on code protection etc.");
DEFINE_string(sections, "",
              "Comma separate list of sections to operate on. Possible values: either all "
              "or a combination of flash, user-id, config, eeprom.");

static bool HasSection(const std::vector<Section> &sections, Section section) {
  return std::find(sections.begin(), sections.end(), section) != sections.end();
}

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_device.empty()) {
    fatal("--device must be specified.\n");
  }

  std::vector<Section> sections = ParseSections(FLAGS_sections);

  std::unique_ptr<DeviceDb> device_db;
  if (FLAGS_family.empty()) {
    fatal("--family must be specified\n");
  } else if (FLAGS_family == "pic18") {
    device_db = std::make_unique<DeviceDb>(1, 1, Datastring{0xff},
                                           [](const Datastring16 &) { return Status::OK; });
  } else if (FLAGS_family == "pic10" || FLAGS_family == "pic12" || FLAGS_family == "pic16") {
    device_db = std::make_unique<DeviceDb>(2, 2, Datastring{0xff, 0x3f},
                                           [](const Datastring16 &) { return Status::OK; });
  } else if (FLAGS_family == "pic10-small" || FLAGS_family == "pic12-small" ||
             FLAGS_family == "pic16-small") {
    device_db = std::make_unique<DeviceDb>(2, 2, Datastring{0xff, 0x0f},
                                           [](const Datastring16 &) { return Status::OK; });
  } else if (FLAGS_family == "pic16-new") {
    device_db = std::make_unique<DeviceDb>(2, 2, Datastring{0xff, 0x3f},
                                           [](const Datastring16 &) { return Status::OK; });
  } else if (FLAGS_family == "pic24") {
    device_db = std::make_unique<DeviceDb>(4, 2, Datastring{0xff, 0xff, 0xff, 0x00},
                                           [](const Datastring16 &) { return Status::OK; });
  } else {
    fatal("Unknown device family %s.\n", FLAGS_family.c_str());
  }

  std::string filename = FLAGS_device_db;
  if (filename.empty()) {
#if defined(DEVICE_DB_PATH)
    filename = strings::Cat(DEVICE_DB_PATH, "/", FLAGS_family, ".lst");
#else
    filename = strings::Cat(Dirname(argv[0]), "/device_db/", FLAGS_family, ".lst");
#endif
  }
  CHECK_OK(device_db->Load(filename));

  DeviceInfo device_info;
  CHECK_OK(device_db->GetDeviceInfo(FLAGS_device, &device_info));

  Program test_program;
  // Get the block filler to use as a bitmask for the programming data.
  Datastring block_filler = device_db->GetBlockFillter();

  if (HasSection(sections, FLASH)) {
    Datastring program_memory_data(device_info.program_memory_size, 0);
    for (uint32_t i = 0; i < device_info.program_memory_size; ++i) {
      program_memory_data[i] = i & block_filler[i % block_filler.size()];
    }
    test_program[0] = std::move(program_memory_data);
  }

  if (HasSection(sections, USER_ID) && device_info.user_id_size > 0) {
    Datastring user_id_data(device_info.user_id_size, 0);
    for (uint32_t i = 0; i < device_info.user_id_size; ++i) {
      user_id_data[i] = i & block_filler[i % block_filler.size()];
    }
    test_program[device_info.user_id_address] = std::move(user_id_data);
  }
  if (HasSection(sections, EEPROM) && device_info.eeprom_size > 0) {
    Datastring eeprom_memory_data(device_info.eeprom_size * device_db->GetBlockSizeMultiple(), 0);
    for (uint32_t i = 0; i < device_info.eeprom_size; ++i) {
      eeprom_memory_data[i * device_db->GetBlockSizeMultiple()] = i;
    }
    test_program[device_info.eeprom_address] = std::move(eeprom_memory_data);
  }
  if (HasSection(sections, CONFIGURATION) && !FLAGS_config_data.empty()) {
    if (FLAGS_config_data.size() % (2 * device_db->GetBlockSizeMultiple())) {
      fatal("--config_data has wrong size (must be mutiple of %d bytes)\n",
            device_db->GetBlockSizeMultiple());
    }
    Datastring config_data;
    for (size_t i = 0; i < FLAGS_config_data.size(); i += 2) {
      uint8_t datum;
      int parse_result = strings::AscciToInt(FLAGS_config_data[i]);
      if (parse_result < 0) {
        fatal("Invalid character '%c' in --config_data.\n", FLAGS_config_data[i]);
      }
      datum = parse_result;
      datum <<= 4;
      parse_result = strings::AscciToInt(FLAGS_config_data[i + 1]);
      if (parse_result < 0) {
        fatal("Invalid character '%c' in --config_data.\n", FLAGS_config_data[i + 1]);
      }
      datum |= parse_result;
      config_data.push_back(datum);
    }
    if (config_data.size() > device_info.config_size) {
      fatal("--config_data is too large.");
    }
    test_program[device_info.config_address] = std::move(config_data);
  }

  FILE *out = stdout;
  if (!FLAGS_output.empty()) {
    if ((out = fopen(FLAGS_output.c_str(), "w")) == nullptr) {
      fatal("Could not open file %s for output %s\n", FLAGS_output.c_str(), strerror(errno));
    }
  }
  WriteIhex(test_program, out);
}
