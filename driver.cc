#include "driver.h"

#include <cstring>
#include <gflags/gflags.h>

#include "ftdi_sb.h"

DEFINE_string(driver, "FtdiSb", "Driver to use for programming. One of FtdiSb");

Status Driver::WriteTimedSequence(const TimedSequence &sequence) {
  for (const auto &step : sequence) {
    RETURN_IF_ERROR(WriteDatastring(step.data));
    RETURN_IF_ERROR(FlushOutput());
    Sleep(step.sleep);
  }
  return Status::OK;
}

Status Driver::WriteDatastring(const Datastring &data) {
  for (const uint8_t pins : data) {
    RETURN_IF_ERROR(SetPins(pins));
  }
  return Status::OK;
}

std::unique_ptr<Driver> Driver::CreateFromFlags() {
  if (FLAGS_driver == "FtdiSb") {
    return std::make_unique<FtdiSbDriver>();
  }
  FATAL("Unknown driver: %s\n", FLAGS_driver.c_str());
}
