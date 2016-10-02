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
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <gflags/gflags.h>
#include <set>
#include <vector>

#include "controller.h"
#include "driver.h"
#include "high_level_controller.h"
#include "program.h"
#include "sequence_generator.h"
#include "status.h"
#include "strings.h"

DEFINE_string(
    action, "",
    "Action to perform. One of erase, dump-program, write-program, identify, list-programmers. "
    "When using erase, dump-program or write-program, the --sections flag can be used to "
    "indicate which sections to operate on. For write-program and dump-program an empty "
    "flag means all sections, while for erase an explicit --sections=all must be passed.");
DEFINE_string(sections, "",
              "Comma separate list of sections to operate on. Possible values: either all "
              "or a combination of flash, user-id, config, eeprom.");
DEFINE_string(family, "pic18", "Device family to use. One of pic18.");
DEFINE_string(
    device, "",
    "Exact device name. Ignored for pic18 family, required for devices in the pic16 family which "
    "don't provide a device ID. Devices which have a device ID should be detectable using the "
    "identify action.");

DEFINE_string(output, "", "File to write the Intel HEX data to.");
DEFINE_string(input, "", "Intel HEX file to read and program.");
DEFINE_string(erase_mode, "chip", "Erase mode for writing. One of chip, section, row, none.");
DEFINE_string(device_db, "",
              "Device DB file to load. Defaults to " DEVICE_DB_PATH "/<family>.lst.");

static std::vector<Section> ParseSections() {
  std::vector<Section> sections;
  if (FLAGS_sections == "all" || FLAGS_sections.empty()) {
    return {FLASH, USER_ID, CONFIGURATION, EEPROM};
  }

  auto section_names = strings::Split<std::string>(FLAGS_sections, ',', false);
  for (const auto &section_name : section_names) {
    Section section;
    if (section_name == "flash") {
      section = FLASH;
    } else if (section_name == "user-id") {
      section = USER_ID;
    } else if (section_name == "config") {
      section = CONFIGURATION;
    } else if (section_name == "eeprom") {
      section = EEPROM;
    } else {
      fatal("Unknown section name %s.\n", section_name.c_str());
    }
    if (std::find(sections.begin(), sections.end(), section) == sections.end()) {
      sections.push_back(section);
    }
  }
  return sections;
}

static EraseMode ParseEraseMode() {
  if (FLAGS_erase_mode == "chip") {
    return CHIP_ERASE;
  } else if (FLAGS_erase_mode == "section") {
    return SECTION_ERASE;
  } else if (FLAGS_erase_mode == "row") {
    return ROW_ERASE;
  } else if (FLAGS_erase_mode == "none") {
    return NO_ERASE;
  } else {
    fatal("No such erase mode '%s'\n", FLAGS_erase_mode.c_str());
  }
}

int main(int argc, char **argv) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  std::unique_ptr<Driver> driver = Driver::CreateFromFlags();
  if (FLAGS_action == "list-programmers") {
    std::vector<std::string> devices;
    CHECK_OK(driver->List(&devices));
    for (const auto &device : devices) {
      printf("Device:\n%s", device.c_str());
    }
    exit(0);
  }

  std::unique_ptr<Controller> controller;
  if (FLAGS_family == "pic18") {
    std::unique_ptr<Pic18SequenceGenerator> sequence_generator(new Pic18SequenceGenerator);
    controller.reset(new Pic18Controller(std::move(driver), std::move(sequence_generator)));
  } else if (FLAGS_family == "pic16") {
    std::unique_ptr<Pic16SequenceGenerator> sequence_generator(new Pic16SequenceGenerator);
    controller.reset(
        new Pic16Controller(std::move(driver), std::move(sequence_generator), FLAGS_device));
  } else {
    fatal("Unknown device family %s.\n", FLAGS_family.c_str());
  }
  auto device_db = std::make_unique<DeviceDb>();

  std::string filename = FLAGS_device_db;
  if (filename.empty()) {
    filename = strings::Cat(DEVICE_DB_PATH, "/", FLAGS_family, ".lst");
  }
  CHECK_OK(device_db->Load(filename, FLAGS_family == "pic18" ? 1 : 2));

  HighLevelController high_level_controller(std::move(controller), std::move(device_db));

  if (FLAGS_action.empty()) {
    fatal("No action specified\n");
  } else if (FLAGS_action == "erase") {
    if (FLAGS_sections.empty()) {
      fatal("Erase requires setting --sections\n");
    } else if (FLAGS_sections == "all") {
      CHECK_OK(high_level_controller.ChipErase());
    } else {
      CHECK_OK(high_level_controller.SectionErase(ParseSections()));
    }
  } else if (FLAGS_action == "dump-program") {
    Program program;
    CHECK_OK(high_level_controller.ReadProgram(ParseSections(), &program));
    FILE *out = fopen(FLAGS_output.c_str(), "w+b");
    if (!out) {
      fatal("Could not open file '%s': %s\n", FLAGS_output.c_str(), strerror(errno));
    }
    WriteIhex(program, out);
    fclose(out);
  } else if (FLAGS_action == "write-program") {
    if (FLAGS_input.empty()) {
      fatal("--input is required for action write-program\n");
    }
    Program program;
    FILE *in = fopen(FLAGS_input.c_str(), "rb");
    if (!in) {
      fatal("Could not open file '%s': %s\n", FLAGS_input.c_str(), strerror(errno));
    }
    CHECK_OK(ReadIhex(&program, in));
    CHECK_OK(high_level_controller.WriteProgram(ParseSections(), program, ParseEraseMode()));
  } else if (FLAGS_action == "identify") {
    CHECK_OK(high_level_controller.Identify());
  } else {
    fatal("Unknown action '%s'\n", FLAGS_action.c_str());
  }
  return EXIT_SUCCESS;
}
