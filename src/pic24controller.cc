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

#define NOP 0

// Notes: The PIC24 series has an odd way of dealing with the instructions etc. That is, it stores
// 16 bits at each address, but uses separate instructions for reading the words add even and odd
// addresses (TBLRDL and TBLRDH).

Status Pic24Controller::Open() {
  RETURN_IF_ERROR(driver_->Open());
  return WriteTimedSequence(Pic24SequenceGenerator::INIT_SEQUENCE, nullptr);
}

void Pic24Controller::Close() { driver_->Close(); }

Status Pic24Controller::ReadDeviceId(uint16_t *device_id, uint16_t *revision) {
  RETURN_IF_ERROR(ResetPc());
  RETURN_IF_ERROR(LoadAddress(0xff0000));
  RETURN_IF_ERROR(LoadVisiAddress());
  // Add a NOP because we will be using the register in the next command for addressing.
  RETURN_IF_ERROR(WriteCommand(NOP));
  // TBLRDL [W6++], [W7]
  RETURN_IF_ERROR(WriteCommand(0xBA0BB6));
  RETURN_IF_ERROR(WriteCommand(NOP));
  RETURN_IF_ERROR(WriteCommand(NOP));
  RETURN_IF_ERROR(ReadVisi(device_id));

  // TBLRDL [W6++], [W7]
  RETURN_IF_ERROR(WriteCommand(0xBA0BB6));
  RETURN_IF_ERROR(WriteCommand(NOP));
  RETURN_IF_ERROR(WriteCommand(NOP));
  RETURN_IF_ERROR(ReadVisi(revision));

  return Status::OK;
}

Status Pic24Controller::Read(Section, uint32_t start_address, uint32_t end_address, const DeviceInfo &, Datastring *result) {
  RETURN_IF_ERROR(ResetPc());
  RETURN_IF_ERROR(LoadVisiAddress());
  RETURN_IF_ERROR(LoadAddress(start_address / 2));
  // Add a NOP because we will be using the register in the next command for addressing.
  RETURN_IF_ERROR(WriteCommand(NOP));

  // FIXME: reading can be sped up by using the repeated read functionality of the driver.
  uint32_t current_address = start_address;
  while (current_address < end_address) {
    if ((current_address & 0x1ffff) == 0 && current_address != start_address) {
      RETURN_IF_ERROR(ResetPc());
      RETURN_IF_ERROR(LoadAddress(current_address / 2));
      // Add a NOP because we will be using the register in the next command for addressing.
      RETURN_IF_ERROR(WriteCommand(NOP));
    }
    // TBLRDL [W6], [W7]
    // 1011     1010     0Bqq     qddd     dppp     ssss
    // 1011 [b] 1010 [a] 0000 [0] 1011 [b] 1001 [9] 0110 [6]
    RETURN_IF_ERROR(WriteCommand(0xBA0B96));
    RETURN_IF_ERROR(WriteCommand(NOP));
    RETURN_IF_ERROR(WriteCommand(NOP));
    uint16_t data;
    RETURN_IF_ERROR(ReadVisi(&data));
    result->push_back(data & 0xff);
    result->push_back((data >> 8) & 0xff);

    // TBLRDH [W6++], [W7]
    // 1011     1010     1Bqq     qddd     dppp     ssss
    // 1011 [b] 1010 [a] 1000 [8] 1011 [b] 1011 [b] 0110 [6]
    RETURN_IF_ERROR(WriteCommand(0xBA8BB6));
    RETURN_IF_ERROR(WriteCommand(NOP));
    RETURN_IF_ERROR(WriteCommand(NOP));
    RETURN_IF_ERROR(ReadVisi(&data));
    result->push_back(data & 0xff);
    result->push_back((data >> 8) & 0xff);

    current_address += 4;
  }

  return Status::OK;
}

Status Pic24Controller::Write(Section section, uint32_t address, const Datastring &data, const DeviceInfo &device_info) {
  if (section != FLASH) {
    return Status(UNIMPLEMENTED, "Writing sections other than FLASH is not yet implemented");
  }

  if (data.size() % device_info.write_block_size != 0) {
    return Status(Code::INVALID_ARGUMENT,
                  strings::Cat("Data must be a multiple of the block size (", data.size(), " / ",
                               device_info.write_block_size, ")"));
  }
  if (address % device_info.write_block_size != 0) {
    return Status(Code::INVALID_ARGUMENT,
                  strings::Cat("Write address must be a multiple of the block size (", address, " / ",
                               device_info.write_block_size, ")"));
  }
  RETURN_IF_ERROR(ResetPc());

  size_t bytes_written = 0;
  while (bytes_written < data.size()) {
    PrintProgress(bytes_written, data.size());
    // MOV #0x4004, W10
    RETURN_IF_ERROR(WriteCommand(0x24004A));
    // MOV W10, NVMCON
    RETURN_IF_ERROR(WriteCommand(0x883B0A));
    RETURN_IF_ERROR(LoadAddress((address + bytes_written) / 2));

    for (uint32_t i = 0; i < device_info.write_block_size; i += 4) {
      uint32_t datum;
      datum = data[bytes_written + 1];
      datum <<= 8;
      datum |= data[bytes_written];
      bytes_written += 2;
      // MOV <word>, W0
      RETURN_IF_ERROR(WriteCommand(0x200000 | (datum << 4)));
      // TBLWTL W0, [W6]
      // 1011     1011     0Bqq     qddd     dppp     ssss
      // 1011 [b] 1011 [b] 0000 [0] 1011 [b] 0000 [0] 0000 [0]
      RETURN_IF_ERROR(WriteCommand(0xBB0B00));
      RETURN_IF_ERROR(WriteCommand(NOP));
      RETURN_IF_ERROR(WriteCommand(NOP)); // FIXME: this one is likely not necessary

      datum = data[bytes_written + 1];
      datum <<= 8;
      datum |= data[bytes_written];
      bytes_written += 2;
      // MOV <word>, W0
      RETURN_IF_ERROR(WriteCommand(0x200000 | (datum << 4)));
      // TBLWTH W0, [W6++]
      // 1011     1011     1Bqq     qddd     dppp     ssss
      // 1011 [b] 1011 [b] 1001 [0] 1011 [b] 0000 [0] 0000 [0]
      RETURN_IF_ERROR(WriteCommand(0xBB9B00));
      RETURN_IF_ERROR(WriteCommand(NOP));
      RETURN_IF_ERROR(WriteCommand(NOP));
    }

    // BSET NVMCON, #WR
    RETURN_IF_ERROR(WriteCommand(0xA8E761));
    RETURN_IF_ERROR(WriteCommand(NOP));
    RETURN_IF_ERROR(WriteCommand(NOP));

    bool done = false;
    do {
      RETURN_IF_ERROR(ResetPc());
      // MOV NVMCON, W2
      RETURN_IF_ERROR(WriteCommand(0x803B02));
      // MOV W2, VISI
      RETURN_IF_ERROR(WriteCommand(0x883C22));
      RETURN_IF_ERROR(WriteCommand(NOP));
      uint16_t nvmcon;
      RETURN_IF_ERROR(ReadVisi(&nvmcon));
      RETURN_IF_ERROR(WriteCommand(NOP));
      done = !(nvmcon & 0x8000);
    } while (!done);
  }

  return Status::OK;
}

Status Pic24Controller::ChipErase(const DeviceInfo &) {
  RETURN_IF_ERROR(ResetPc());
  // MOV #0x4064, W10
  RETURN_IF_ERROR(WriteCommand(0x24064A));
  // MOV W10, NVMCON
  RETURN_IF_ERROR(WriteCommand(0x883B0A));
  RETURN_IF_ERROR(LoadAddress(0));
  // TBLWTL W0, [W0]
  RETURN_IF_ERROR(WriteCommand(0xBB0800));
  RETURN_IF_ERROR(WriteCommand(NOP));
  RETURN_IF_ERROR(WriteCommand(NOP));

  // BSET NVMCON, #WR
  RETURN_IF_ERROR(WriteCommand(0xA8E761));
  RETURN_IF_ERROR(WriteCommand(NOP));
  RETURN_IF_ERROR(WriteCommand(NOP));

  bool done = false;
  do {
    RETURN_IF_ERROR(ResetPc());
    // MOV NVMCON, W2
    RETURN_IF_ERROR(WriteCommand(0x803B02));
    // MOV W2, VISI
    RETURN_IF_ERROR(WriteCommand(0x883C22));
    RETURN_IF_ERROR(WriteCommand(NOP));
    uint16_t nvmcon;
    RETURN_IF_ERROR(ReadVisi(&nvmcon));
    RETURN_IF_ERROR(WriteCommand(NOP));
    done = !(nvmcon & 0x8000);
  } while (!done);
  return Status::OK;
}

Status Pic24Controller::SectionErase(Section, const DeviceInfo &) {
  return Status(UNIMPLEMENTED, "Erasing the device has not been implmeneted yet.");
}

Status Pic24Controller::WriteCommand(uint32_t payload) {
  return driver_->WriteDatastring(sequence_generator_->GetWriteCommandSequence(payload));
}

Status Pic24Controller::ReadVisi(uint16_t *result) {
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
  // MOV <first byte of address, W0
  RETURN_IF_ERROR(WriteCommand(0x200000 | ((address >> 12) & 0xff0)));
  // MOV W0, TBLPAG
  RETURN_IF_ERROR(WriteCommand(0x880190));
  // MOV <bottom two bytes of address>, W6
  return WriteCommand(0x200006 | ((address << 4) & 0xffff0));
}

Status Pic24Controller::LoadVisiAddress() {
  // MOV #VISI, W7
  return WriteCommand(0x207847);
}

Status Pic24Controller::ResetPc() {
  // GOTO 0x0200.
  RETURN_IF_ERROR(WriteCommand(0x040200));
  // NOP (with top of address).
  return WriteCommand(NOP);
}
