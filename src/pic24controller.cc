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
#include "pic24controller.h"

#include <set>

#include "strings.h"
#include "util.h"

Status Pic24Controller::Open() {
  RETURN_IF_ERROR(driver_->Open());
  return WriteTimedSequence(Pic24SequenceGenerator::INIT_SEQUENCE, nullptr);
}

void Pic24Controller::Close() { driver_->Close(); }

Status Pic24Controller::ReadDeviceId(uint16_t *device_id, uint16_t *revision) {
  RETURN_IF_ERROR(LoadAddress(0xff0000));
  RETURN_IF_ERROR(LoadVisiAddress());
  // TBLRDL [W6++], [W7]
  RETURN_IF_ERROR(WriteCommand(0xba0bb6));
  RETURN_IF_ERROR(WriteCommand(0));
  RETURN_IF_ERROR(WriteCommand(0));
  RETURN_IF_ERROR(ReadWithCommand(device_id));

  // TBLRDL [W6++], [W7]
  RETURN_IF_ERROR(WriteCommand(0xba0bb6));
  RETURN_IF_ERROR(WriteCommand(0));
  RETURN_IF_ERROR(WriteCommand(0));
  RETURN_IF_ERROR(ReadWithCommand(revision));

  return Status::OK;
}

Status Pic24Controller::Read(Section, uint32_t, uint32_t, const DeviceInfo &, Datastring *) {
  return Status(UNIMPLEMENTED, "Reading the device has not been implmeneted yet.");
}

Status Pic24Controller::Write(Section, uint32_t, const Datastring &, const DeviceInfo &) {
  return Status(UNIMPLEMENTED, "Writing the device has not been implmeneted yet.");
}

Status Pic24Controller::ChipErase(const DeviceInfo &) {
  return Status(UNIMPLEMENTED, "Erasing the device has not been implmeneted yet.");
}

Status Pic24Controller::SectionErase(Section, const DeviceInfo &) {
  return Status(UNIMPLEMENTED, "Erasing the device has not been implmeneted yet.");
}

Status Pic24Controller::WriteCommand(uint32_t payload) {
  return driver_->WriteDatastring(sequence_generator_->GetWriteCommandSequence(payload));
}

Status Pic24Controller::ReadWithCommand(uint16_t *result) {
  Datastring16 data;
  RETURN_IF_ERROR(
      driver_->ReadWithSequence(sequence_generator_->GetReadCommandSequence(), 12, 16, 1, &data));
  *result = data[0];
  return Status::OK;
}

Status Pic24Controller::WriteTimedSequence(Pic24SequenceGenerator::TimedSequenceType type,
                                           const DeviceInfo *device_info) {
  return driver_->WriteTimedSequence(sequence_generator_->GetTimedSequence(type, device_info));
}

Status Pic24Controller::LoadAddress(uint32_t address) {
  // GOTO 0x0200. This is to avoid accidental resets, and by incorporating it into the LoadAddress
  // command, we don't have to be explicit about it all the time.
  RETURN_IF_ERROR(WriteCommand(0x040200));
  // NOP (with top of address).
  RETURN_IF_ERROR(WriteCommand(0));

  // MOV <first byte of address, W0
  RETURN_IF_ERROR(WriteCommand(0x200000 | ((address >> 12) & 0xff0)));
  // MOV W0, TBLPAG
  RETURN_IF_ERROR(WriteCommand(0x880190));
  // MOV <bottom two bytes of address>, W6
  return WriteCommand(0x200006 | ((address << 4) & 0xffff0));
}

Status Pic24Controller::LoadVisiAddress() {
  // MOV #VISI, W7
  RETURN_IF_ERROR(WriteCommand(0x207847));
  // NOP
  return WriteCommand(0);
}
