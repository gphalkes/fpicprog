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
#include "driver.h"

#include <cstring>
#include <gflags/gflags.h>

#include "ftdi_sb.h"

DEFINE_string(driver, "FtdiSb", "Driver to use for programming. One of FtdiSb");

Status Driver::WriteTimedSequence(const TimedSequence &sequence) {
  for (const auto &step : sequence) {
    RETURN_IF_ERROR(WriteDatastring(step.data));
    RETURN_IF_ERROR(FlushOutput());
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
