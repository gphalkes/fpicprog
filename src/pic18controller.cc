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
#include "pic18controller.h"

#include <set>

#include "strings.h"
#include "util.h"

Status Pic18Controller::Open() {
  RETURN_IF_ERROR(driver_->Open());
  return WriteTimedSequence(Pic18SequenceGenerator::INIT_SEQUENCE, nullptr);
}

void Pic18Controller::Close() { driver_->Close(); }

Status Pic18Controller::ReadDeviceId(uint16_t *device_id, uint16_t *revision) {
  RETURN_IF_ERROR(LoadAddress(0x3ffffe));
  Datastring bytes;
  RETURN_IF_ERROR(ReadWithCommand(TABLE_READ_post_inc, 2, &bytes));

  *device_id = bytes[0] | static_cast<uint16_t>(bytes[1]) << 8;
  *revision = *device_id & 0x1f;
  *device_id &= 0xffe0;
  return Status::OK;
}

Status Pic18Controller::Read(Section section, uint32_t start_address, uint32_t end_address,
                             const DeviceInfo &, Datastring *result) {
  if (section != EEPROM) {
    RETURN_IF_ERROR(LoadAddress(start_address));
    return ReadWithCommand(TABLE_READ_post_inc, end_address - start_address, result);
  } else {
    result->clear();
    // BCF EECON1, EEPGD
    RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x9EA6));
    // BCF EECON1, CFGS
    RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x9CA6));
    for (uint32_t address = start_address; address < end_address; ++address) {
      RETURN_IF_ERROR(LoadEepromAddress(address));
      // BSF EECON1, RD
      RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x80A6));
      // MOVF EEDATA, W, 0
      RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x50A8));
      // MOVWF TABLAT
      RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x6EF5));
      // NOP
      RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x0000));
      Datastring byte;
      RETURN_IF_ERROR(ReadWithCommand(SHIFT_OUT_TABLAT, 1, &byte));
      result->append(byte);
    }
    return Status::OK;
  }
}

Status Pic18Controller::Write(Section section, uint32_t address, const Datastring &data,
                              const DeviceInfo &device_info) {
  uint32_t block_size = 1;
  if (section == FLASH) {
    block_size = device_info.write_block_size;
  } else if (section == USER_ID) {
    block_size = device_info.user_id_size;
  }
  if (data.size() % block_size) {
    return Status(Code::INVALID_ARGUMENT,
                  strings::Cat("Data must be a multiple of the block size (", data.size(), " / ",
                               device_info.write_block_size, ")"));
  }
  if (section == FLASH || section == USER_ID) {
    if (block_size % 2 != 0 || block_size < 2) {
      return Status(Code::INVALID_ARGUMENT, "Block size for writing must be a multiple of 2");
    }
    AutoClosureRunner reset_line([] {
      fprintf(stderr, "\r");
      fflush(stderr);
    });
    for (size_t i = 0; i < data.size(); i += block_size) {
      print_msg(1, "\r%.0f%%", 100.0 * i / data.size());
      fflush(stderr);

      // BSF EECON1, EEPGD
      RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x8EA6));
      // BCF EECON1, CFGS
      RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x9CA6));
      // BSF EECON1, WREN
      RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x84A6));
      RETURN_IF_ERROR(LoadAddress(address + i));
      for (size_t j = 0; j < block_size - 2; j += 2) {
        RETURN_IF_ERROR(WriteCommand(TABLE_WRITE_post_inc2,
                                     (static_cast<uint16_t>(data[i + j + 1]) << 8) | data[i + j]));
      }
      RETURN_IF_ERROR(WriteCommand(
          TABLE_WRITE_post_inc2_start_pgm,
          (static_cast<uint16_t>(data[i + block_size - 1]) << 8) | data[i + block_size - 2]));
      RETURN_IF_ERROR(WriteTimedSequence(Pic18SequenceGenerator::WRITE_SEQUENCE, &device_info));
    }
  } else if (section == CONFIGURATION) {
    for (const uint8_t byte : data) {
      // BSF EECON1, EEPGD
      RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x8EA6));
      // BSF EECON1, CFGS
      RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x8CA6));
      // BSF EECON1, WREN
      RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x84A6));
      RETURN_IF_ERROR(LoadAddress(address));
      // Only one of the two copies of byte is actually used. Which one depends on whether address
      // is odd or even. The other byte is ignored.
      RETURN_IF_ERROR(
          WriteCommand(TABLE_WRITE_post_inc2_start_pgm, (static_cast<uint16_t>(byte) << 8) | byte));
      RETURN_IF_ERROR(WriteTimedSequence(Pic18SequenceGenerator::WRITE_SEQUENCE, &device_info));
      ++address;
    }
  } else if (section == EEPROM) {
    for (const uint8_t byte : data) {
      // BCF EECON1, EEPGD
      RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x9EA6));
      // BCF EECON1, CFGS
      RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x9CA6));
      RETURN_IF_ERROR(LoadEepromAddress(address));
      // MOVLW <data>
      RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x0E00 | byte));
      // BSF EECON1, WREN
      RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x84A6));
      // BSF EECON1, WREN
      RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x82A6));
      // NOP
      RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x0000));
      // NOP
      RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x0000));

      Datastring value;
      do {
        // MOVF EEDATA, W, 0
        RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x50A8));
        // MOVWF TABLAT
        RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x6EF5));
        // NOP
        RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x0000));
        RETURN_IF_ERROR(ReadWithCommand(SHIFT_OUT_TABLAT, 1, &value));
      } while (value[0] & 2);
      // 200us is the minimum requirement for the PIC18s I've seen. However, for safety we add a
      // bit of margin.
      Sleep(MicroSeconds(500));
      ++address;
    }
    // BSF EECON1, WREN
    RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x94A6));
  }
  return Status::OK;
}

Status Pic18Controller::ChipErase(const DeviceInfo &device_info) {
  return ExecuteBulkErase(device_info.chip_erase_sequence, device_info);
}

Status Pic18Controller::SectionErase(Section section, const DeviceInfo &device_info) {
  switch (section) {
    case FLASH:
      return ExecuteBulkErase(device_info.flash_erase_sequence, device_info);
    case USER_ID:
      return ExecuteBulkErase(device_info.user_id_erase_sequence, device_info);
    case CONFIGURATION:
      return ExecuteBulkErase(device_info.config_erase_sequence, device_info);
    case EEPROM:
      return ExecuteBulkErase(device_info.eeprom_erase_sequence, device_info);
    default:
      return Status(Code::UNIMPLEMENTED,
                    strings::Cat("Section erase not implemented for section type ", section));
  }
}

Status Pic18Controller::WriteCommand(Pic18Command command, uint16_t payload) {
  return driver_->WriteDatastring(sequence_generator_->GetCommandSequence(command, payload));
}

Status Pic18Controller::ReadWithCommand(Pic18Command command, uint32_t count, Datastring *result) {
  Datastring16 data;
  RETURN_IF_ERROR(driver_->ReadWithSequence(sequence_generator_->GetCommandSequence(command, 0), 12,
                                            8, count, &data));
  result->clear();
  for (const uint16_t c : data) {
    result->push_back(c);
  }
  return Status::OK;
}

Status Pic18Controller::WriteTimedSequence(Pic18SequenceGenerator::TimedSequenceType type,
                                           const DeviceInfo *device_info) {
  return driver_->WriteTimedSequence(sequence_generator_->GetTimedSequence(type, device_info));
}

Status Pic18Controller::LoadAddress(uint32_t address) {
  // MOVLW <first byte of address>
  RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x0E00 | ((address >> 16) & 0xff)));
  // MOVWF TBLPTRU
  RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x6EF8));
  // MOVLW <second byte of address>
  RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x0E00 | ((address >> 8) & 0xff)));
  // MOVWF TBLPTRH
  RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x6EF7));
  // MOVLW <last byte of address>
  RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x0E00 | (address & 0xff)));
  // MOVWF TBLPTRL
  return WriteCommand(CORE_INST, 0x6EF6);
}

Status Pic18Controller::LoadEepromAddress(uint32_t address) {
  // MOVLW <address low byte>
  RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x0E00 | (address & 0xff)));
  // MOVWF EEARD
  RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x6EA9));
  // MOVLW <address low byte>
  RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x0E00 | ((address >> 8) & 0xff)));
  // MOVWF EEARD
  return WriteCommand(CORE_INST, 0x6EAA);
}

Status Pic18Controller::ExecuteBulkErase(const Datastring16 &sequence,
                                         const DeviceInfo &device_info) {
  auto timed_sequence = sequence_generator_->GetTimedSequence(
      Pic18SequenceGenerator::BULK_ERASE_SEQUENCE, &device_info);
  for (uint16_t value : sequence) {
    RETURN_IF_ERROR(LoadAddress(0x3C0005));
    // 1100 HH HH Write HHh to 3C0005h
    uint16_t upper = value & 0xff00;
    upper |= upper >> 8;
    RETURN_IF_ERROR(WriteCommand(TABLE_WRITE, upper));
    RETURN_IF_ERROR(LoadAddress(0x3C0004));
    // 1100 LL LL Write LLh TO 3C0004h to erase entire device.
    uint16_t lower = value & 0xff;
    lower |= lower << 8;
    RETURN_IF_ERROR(WriteCommand(TABLE_WRITE, lower));
    // 0000 00 00 NOP
    RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x0000));
    // 0000 00 00 Hold PGD low until erase completes.
    RETURN_IF_ERROR(driver_->WriteTimedSequence(timed_sequence));
  }
  return Status::OK;
}
