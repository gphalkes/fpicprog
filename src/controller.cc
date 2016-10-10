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
#include "controller.h"

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

Status Pic16Controller::Open() {
  RETURN_IF_ERROR(driver_->Open());
  return WriteTimedSequence(Pic16SequenceGenerator::INIT_SEQUENCE, nullptr);
}

void Pic16Controller::Close() { driver_->Close(); }

Status Pic16Controller::ReadDeviceId(uint16_t *device_id, uint16_t *revision) {
  RETURN_IF_ERROR(WriteCommand(LOAD_CONFIGURATION, 0));

  for (int i = 0; i < 5; ++i) {
    RETURN_IF_ERROR(WriteCommand(INCREMENT_ADDRESS));
  }
  // The PIC16 family has two different formats for device and revision ID. The first format stores
  // all information in configuration word 6, the second uses configuration word 5 for the revision
  // ID and word 6 for the device ID. The revision ID then starts with 10b, the device ID with 11b.
  uint16_t location5_data, location6_data;
  RETURN_IF_ERROR(ReadWithCommand(READ_PROG_MEMORY, &location5_data));
  RETURN_IF_ERROR(WriteCommand(INCREMENT_ADDRESS));
  RETURN_IF_ERROR(ReadWithCommand(READ_PROG_MEMORY, &location6_data));
  if ((location5_data & 0x3000) == 0x3000) {
    *device_id = location6_data >> 5;
    *revision = location6_data & 0x1f;
  } else {
    *device_id = location6_data;
    *revision = location5_data;
  }
  last_address_ = INT32_MAX;
  return Status::OK;
}

Status Pic16Controller::Read(Section section, uint32_t start_address, uint32_t end_address,
                             const DeviceInfo &device_info, Datastring *result) {
  RETURN_IF_ERROR(LoadAddress(section, start_address, device_info));

  // This could use a different read mode, which reads more than a single word at a time. However,
  // when sync is lost then, we can't retry immediately because the PC has already been advanced
  // beyond the point where we last read. Hence it can be slower, especially for larger flash
  // memories.
  for (int i = end_address - start_address; i > 0; i -= 2) {
    uint16_t data;
    Status status(SYNC_LOST, "FAKE STATUS");
    for (int j = 0; j < 3 && status.code() == SYNC_LOST; ++j) {
      status = ReadWithCommand(section == EEPROM ? READ_DATA_MEMORY : READ_PROG_MEMORY, &data);
    }
    RETURN_IF_ERROR(status);
    RETURN_IF_ERROR(IncrementPc(device_info));
    result->push_back(data & 0xff);
    result->push_back((data >> 8) & 0x3f);
  }

  return Status::OK;
}

Status Pic16Controller::Write(Section section, uint32_t address, const Datastring &data,
                              const DeviceInfo &device_info) {
  RETURN_IF_ERROR(LoadAddress(section, address, device_info));

  if (section == FLASH) {
    uint32_t block_size = device_info.write_block_size;
    if (address % block_size != 0) {
      return Status(INVALID_ARGUMENT, "Address is not a multiple of the write_block_size");
    }
    if (data.size() % block_size != 0) {
      return Status(INVALID_ARGUMENT, "Data size is not a multiple of the write_block_size");
    }
    for (size_t base = 0; base < data.size(); base += block_size) {
      for (uint32_t i = 0; i < block_size; i += 2) {
        uint16_t datum = data[base + i + 1];
        datum <<= 8;
        datum |= static_cast<uint8_t>(data[base + i]);
        RETURN_IF_ERROR(WriteCommand(LOAD_PROG_MEMORY, datum));
        if (i != block_size - 2) {
          RETURN_IF_ERROR(IncrementPc(device_info));
        }
      }
      RETURN_IF_ERROR(
          WriteTimedSequence(Pic16SequenceGenerator::WRITE_DATA_SEQUENCE, &device_info));
      RETURN_IF_ERROR(IncrementPc(device_info));
    }
  } else {
    for (size_t i = 0; i < data.size(); i += 2) {
      uint16_t datum = data[i + 1];
      datum <<= 8;
      datum |= static_cast<uint8_t>(data[i]);
      RETURN_IF_ERROR(WriteCommand(LOAD_PROG_MEMORY, datum));
      RETURN_IF_ERROR(
          WriteTimedSequence(Pic16SequenceGenerator::WRITE_DATA_SEQUENCE, &device_info));
      RETURN_IF_ERROR(IncrementPc(device_info));
    }
  }
  return Status::OK;
}

Status Pic16Controller::ChipErase(const DeviceInfo &device_info) {
  RETURN_IF_ERROR(WriteTimedSequence(Pic16SequenceGenerator::CHIP_ERASE_SEQUENCE, &device_info));
  return Status::OK;
}

Status Pic16Controller::SectionErase(Section section, const DeviceInfo &device_info) {
  return Status::OK;
}

Status Pic16Controller::WriteCommand(Pic16Command command, uint16_t payload) {
  return driver_->WriteDatastring(sequence_generator_->GetCommandSequence(command, payload));
}

Status Pic16Controller::WriteCommand(Pic16Command command) {
  return driver_->WriteDatastring(sequence_generator_->GetCommandSequence(command));
}

Status Pic16Controller::ReadWithCommand(Pic16Command command, uint16_t *result) {
  Datastring16 data;
  RETURN_IF_ERROR(driver_->ReadWithSequence(sequence_generator_->GetCommandSequence(command, 0), 7,
                                            14, 1, &data));
  *result = data[0];
  return Status::OK;
}

Status Pic16Controller::WriteTimedSequence(Pic16SequenceGenerator::TimedSequenceType type,
                                           const DeviceInfo *device_info) {
  return driver_->WriteTimedSequence(sequence_generator_->GetTimedSequence(type, device_info));
}

#warning FIXME: implement a different address handling for the chips that lack LOAD_CONFIGURATION
Status Pic16Controller::LoadAddress(Section section, uint32_t address,
                                    const DeviceInfo &device_info) {
  if (section == CONFIGURATION) {
    if (address < last_address_ || last_address_ < device_info.config_offset) {
      RETURN_IF_ERROR(WriteCommand(LOAD_CONFIGURATION, 0));
      last_address_ = device_info.config_offset;
    }
  } else if (section == FLASH) {
    if (address < last_address_) {
      RETURN_IF_ERROR(ResetDevice());
    }
  } else if (section == EEPROM) {
    address -= device_info.eeprom_offset;
    if (address < last_address_) {
      RETURN_IF_ERROR(ResetDevice());
    }
  }

  if (last_address_ > address) {
    fatal("INTERNAL ERROR: last_address_ (%04x) should be <= start_address (%04x)\n", last_address_,
          address);
  }
  while (last_address_ < address) {
    RETURN_IF_ERROR(IncrementPc(device_info));
  }
  return Status::OK;
}

Status Pic16Controller::ResetDevice() {
  RETURN_IF_ERROR(WriteTimedSequence(Pic16SequenceGenerator::INIT_SEQUENCE, nullptr));
  last_address_ = 0;
  return Status::OK;
}

Status Pic16Controller::IncrementPc(const DeviceInfo &device_info) {
  RETURN_IF_ERROR(WriteCommand(INCREMENT_ADDRESS));
  bool was_config = false;
  if (last_address_ >= device_info.config_offset) {
    was_config = true;
  }
  last_address_ += 2;
  if (last_address_ >= device_info.config_offset && !was_config) {
    last_address_ = 0;
  }
  return Status::OK;
}
