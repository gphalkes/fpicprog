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
DEFINE_string(
    device, "",
    "Name of the device to write a test program for.");

DEFINE_string(output, "", "File to write the Intel HEX data to.");
DEFINE_string(device_db, "", "Device DB file to load. Defaults to "
#if defined(DEVICE_DB_PATH)
              DEVICE_DB_PATH "/<family>.lst.");
#else
              "<path to binary>/device_db/<family>.lst.");
#endif

// FIXME: add test data for User ID and allow to specify data for config words.

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_device.empty()) {
    fatal("--device must be specified.\n");
  }

  std::unique_ptr<DeviceDb> device_db;
  if (FLAGS_family.empty()) {
    fatal("--family must be specified\n");
  } else if (FLAGS_family == "pic18") {
    device_db = std::make_unique<DeviceDb>(1, Datastring{0xff},
                                           [](const Datastring16 &) { return Status::OK; });
  } else if (FLAGS_family == "pic10" || FLAGS_family == "pic12" || FLAGS_family == "pic16") {
    device_db =
        std::make_unique<DeviceDb>(2, Datastring{0xff, 0x3f},
                                   [](const Datastring16 &) { return Status::OK; });
  } else if (FLAGS_family == "pic10-small" || FLAGS_family == "pic12-small" ||
             FLAGS_family == "pic16-small") {
    device_db =
        std::make_unique<DeviceDb>(2, Datastring{0xff, 0x3f},
                                   [](const Datastring16 &) { return Status::OK; });
  } else if (FLAGS_family == "pic16-new") {
    device_db =
        std::make_unique<DeviceDb>(2, Datastring{0xff, 0x3f},
                                   [](const Datastring16 &) { return Status::OK; });
  } else if (FLAGS_family == "pic24") {
    device_db = std::make_unique<DeviceDb>(2, Datastring{0xff},
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

  Datastring program_memory_data(device_info.program_memory_size, 0);
  for (uint32_t i = 0; i < device_info.program_memory_size; ++i) {
    program_memory_data[i] = i & block_filler[i % block_filler.size()];
  }
  test_program[0] = std::move(program_memory_data);

  if (device_info.user_id_size > 0) {
    Datastring user_id_data(device_info.user_id_size, 0);
    for (uint32_t i = 0; i < device_info.user_id_size; ++i) {
      user_id_data[i] = i & block_filler[i % block_filler.size()];
    }
    test_program[device_info.user_id_address] = std::move(user_id_data);
  }
  if (device_info.eeprom_size > 0) {
    Datastring eeprom_memory_data(device_info.eeprom_size * device_db->GetBlockSizeMultiple(), 0);
    for (uint32_t i = 0; i < device_info.eeprom_size; ++i) {
      eeprom_memory_data[i * device_db->GetBlockSizeMultiple()] = i;
    }
    test_program[device_info.eeprom_address] = std::move(eeprom_memory_data);
  }

  FILE *out = stdout;
  if (!FLAGS_output.empty()) {
    if ((out = fopen(FLAGS_output.c_str(), "w")) == nullptr) {
      fatal("Could not open file %s for output %s\n", FLAGS_output.c_str(), strerror(errno));
    }
  }
  WriteIhex(test_program, out);
}
