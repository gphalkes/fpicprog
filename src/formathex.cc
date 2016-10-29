#include <cerrno>
#include <cstring>
#include <gflags/gflags.h>
#include <map>

#include "program.h"
#include "strings.h"
#include "util.h"

DEFINE_string(input, "", "File to read.");
DEFINE_int32(bytes_per_line, 16, "Bytes to print per line.");

int main(int argc, char *argv[]) {
  google::ParseCommandLineFlags(&argc, &argv, true);

  if (FLAGS_input.empty()) {
    fatal("--input must be set.");
  }
  if (FLAGS_bytes_per_line < 1) {
    fatal("--bytes_per_line must be >= 1.");
  }

  FILE *file;
  if ((file = fopen(FLAGS_input.c_str(), "r")) == nullptr) {
    fatal("Could not open %s: %s\n", FLAGS_input.c_str(), strerror(errno));
  }

  Program program;
  CHECK_OK(ReadIhex(&program, file));
  DeviceInfo device_info;
  // Build a fake device info which simply merges everything.
  device_info.program_memory_size = std::numeric_limits<uint32_t>::max();
  CHECK_OK(MergeProgramBlocks(&program, device_info));
  for (const auto &part : program) {
    uint32_t start = (part.first / FLAGS_bytes_per_line) * FLAGS_bytes_per_line;
    printf("%08X:", start);
    uint32_t i = start;
    for (; i < part.first; ++i) {
      printf(" ..");
    }
    for (auto byte : part.second) {
      if (i > start && i % FLAGS_bytes_per_line == 0) {
        printf("\n");
        printf("%08X:", i);
      }
      printf(" %02X", byte);
      ++i;
    }
    printf("\n--\n");
  }
}
