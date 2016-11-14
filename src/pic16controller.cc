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
#include "pic16controller.h"

#include <set>

#include "strings.h"
#include "util.h"

Status Pic16ControllerBase::Open() {
  RETURN_IF_ERROR(driver_->Open());
  return ResetDevice();
}

void Pic16ControllerBase::Close() { driver_->Close(); }

Status Pic16ControllerBase::ReadDeviceId(uint16_t *device_id, uint16_t *revision) {
  RETURN_IF_ERROR(WriteCommand(Pic16Command::LOAD_CONFIGURATION, 0));

  for (int i = 0; i < 5; ++i) {
    RETURN_IF_ERROR(WriteCommand(Pic16Command::INCREMENT_ADDRESS));
  }
  // The PIC16 family has two different formats for device and revision ID. The first format stores
  // all information in configuration word 6, the second uses configuration word 5 for the revision
  // ID and word 6 for the device ID. The revision ID then starts with 10b, the device ID with 11b.
  uint16_t location5_data, location6_data;
  RETURN_IF_ERROR(ReadWithCommand(Pic16Command::READ_PROG_MEMORY, &location5_data));
  RETURN_IF_ERROR(WriteCommand(Pic16Command::INCREMENT_ADDRESS));
  RETURN_IF_ERROR(ReadWithCommand(Pic16Command::READ_PROG_MEMORY, &location6_data));
  print_msg(9, "Device ID words: %04X %04X\n", location5_data, location6_data);
  if ((location6_data & 0x3000) == 0x3000 && (location5_data & 0x3000) == 0x2000) {
    *device_id = location6_data;
    *revision = location5_data;
  } else {
    *device_id = location6_data >> 5;
    *revision = location6_data & 0x1f;
  }
  return ResetDevice();
}

Status Pic16ControllerBase::Read(Section section, uint32_t start_address, uint32_t end_address,
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
      status = ReadWithCommand(
          section == EEPROM ? Pic16Command::READ_DATA_MEMORY : Pic16Command::READ_PROG_MEMORY,
          &data);
    }
    RETURN_IF_ERROR(status);
    RETURN_IF_ERROR(IncrementPc(device_info));
    result->push_back(data & 0xff);
    result->push_back((data >> 8) & 0x3f);
  }

  return Status::OK;
}

Status Pic16ControllerBase::Write(Section section, uint32_t address, const Datastring &data,
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
      PrintProgress(base, data.size());
      for (uint32_t i = 0; i < block_size; i += 2) {
        uint16_t datum = data[base + i + 1];
        datum <<= 8;
        datum |= static_cast<uint8_t>(data[base + i]);
        RETURN_IF_ERROR(WriteCommand(Pic16Command::LOAD_PROG_MEMORY, datum));
        if (i != block_size - 2) {
          RETURN_IF_ERROR(IncrementPc(device_info));
        }
      }
      RETURN_IF_ERROR(
          WriteTimedSequence(Pic16SequenceGenerator::WRITE_DATA_SEQUENCE, &device_info));
      RETURN_IF_ERROR(IncrementPc(device_info));
    }
  } else {
    if (address % 2 != 0) {
      return Status(INVALID_ARGUMENT, "Address is not a multiple of the write_block_size");
    }
    if (data.size() % 2 != 0) {
      return Status(INVALID_ARGUMENT, "Data size is not a multiple of the write_block_size");
    }
    for (size_t i = 0; i < data.size(); i += 2) {
      PrintProgress(data.size(), i);
      uint16_t datum = data[i + 1];
      datum <<= 8;
      datum |= static_cast<uint8_t>(data[i]);
      RETURN_IF_ERROR(WriteCommand(
          section == EEPROM ? Pic16Command::LOAD_DATA_MEMORY : Pic16Command::LOAD_PROG_MEMORY,
          datum));
      RETURN_IF_ERROR(
          WriteTimedSequence(Pic16SequenceGenerator::WRITE_DATA_SEQUENCE, &device_info));
      RETURN_IF_ERROR(IncrementPc(device_info));
    }
  }
  return Status::OK;
}

Status Pic16ControllerBase::ChipErase(const DeviceInfo &device_info) {
  Datastring calibration_word;
  if (device_info.calibration_word_address != 0) {
    RETURN_IF_ERROR(Read(USER_ID, device_info.calibration_word_address,
                         device_info.calibration_word_address + 2, device_info, &calibration_word));
  }
  RETURN_IF_ERROR(ResetDevice());
  RETURN_IF_ERROR(WriteTimedSequence(Pic16SequenceGenerator::CHIP_ERASE_SEQUENCE, &device_info));
  RETURN_IF_ERROR(ResetDevice());
  if (device_info.calibration_word_address != 0) {
    RETURN_IF_ERROR(
        Write(USER_ID, device_info.calibration_word_address, calibration_word, device_info));
  }
  return Status::OK;
}

Status Pic16ControllerBase::SectionErase(Section, const DeviceInfo &) {
  return Status(UNIMPLEMENTED, "Section erase not implemented");
}

Status Pic16ControllerBase::WriteCommand(Pic16Command command, uint16_t payload) {
  return driver_->WriteDatastring(sequence_generator_->GetCommandSequence(command, payload));
}

Status Pic16ControllerBase::WriteCommand(Pic16Command command) {
  return driver_->WriteDatastring(
      sequence_generator_->GetCommandSequence(static_cast<uint8_t>(command)));
}

Status Pic16ControllerBase::ReadWithCommand(Pic16Command command, uint16_t *result) {
  Datastring16 data;
  RETURN_IF_ERROR(driver_->ReadWithSequence(sequence_generator_->GetCommandSequence(command, 0),
                                            {7}, 14, 1, &data));
  *result = data[0];
  return Status::OK;
}

Status Pic16ControllerBase::WriteTimedSequence(Pic16SequenceGenerator::TimedSequenceType type,
                                               const DeviceInfo *device_info) {
  return driver_->WriteTimedSequence(sequence_generator_->GetTimedSequence(type, device_info));
}

//==================================================================================================

Status Pic16MidrangeController::LoadAddress(Section section, uint32_t address,
                                            const DeviceInfo &device_info) {
  if (section == CONFIGURATION) {
    if (address < last_address_ || last_address_ < device_info.config_address) {
      RETURN_IF_ERROR(WriteCommand(Pic16Command::LOAD_CONFIGURATION, 0));
      last_address_ = device_info.config_address;
    }
  } else if (section == FLASH) {
    if (address < last_address_) {
      RETURN_IF_ERROR(ResetDevice());
    }
  } else if (section == EEPROM) {
    address -= device_info.eeprom_address;
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

Status Pic16MidrangeController::IncrementPc(const DeviceInfo &device_info) {
  RETURN_IF_ERROR(WriteCommand(Pic16Command::INCREMENT_ADDRESS));
  bool was_config = false;
  if (last_address_ >= device_info.config_address) {
    was_config = true;
  }
  last_address_ += 2;
  if (last_address_ >= device_info.config_address && !was_config) {
    // Force a reset of the PC if an overflow into the config area was detected.
    last_address_ = std::numeric_limits<uint32_t>::max();
  }
  return Status::OK;
}

Status Pic16MidrangeController::ResetDevice() {
  RETURN_IF_ERROR(WriteTimedSequence(Pic16SequenceGenerator::INIT_SEQUENCE, nullptr));
  last_address_ = 0;
  return Status::OK;
}

//==================================================================================================

Status Pic16BaselineController::LoadAddress(Section section, uint32_t address,
                                            const DeviceInfo &device_info) {
  if (section == CONFIGURATION) {
    if (last_address_ != kConfigurationAddress) {
      RETURN_IF_ERROR(ResetDevice());
      last_address_ = kConfigurationAddress;
      return Status::OK;
    }
  } else if (section == FLASH || section == USER_ID) {
    if (address < last_address_) {
      RETURN_IF_ERROR(ResetDevice());
      RETURN_IF_ERROR(IncrementPc(device_info));
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

Status Pic16BaselineController::IncrementPc(const DeviceInfo &) {
  RETURN_IF_ERROR(WriteCommand(Pic16Command::INCREMENT_ADDRESS));
  // This will wrap around to 0 if the address is the configuration location.
  last_address_ += 2;
  return Status::OK;
}

Status Pic16BaselineController::ResetDevice() {
  RETURN_IF_ERROR(WriteTimedSequence(Pic16SequenceGenerator::INIT_SEQUENCE, nullptr));
  last_address_ = kConfigurationAddress;
  return Status::OK;
}
