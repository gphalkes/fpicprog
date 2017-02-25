#include "picnew8bitcontroller.h"

Status PicNew8BitController::Open() {
  RETURN_IF_ERROR(driver_->Open());
  return WriteTimedSequence(PicNew8BitSequenceGenerator::INIT_SEQUENCE, nullptr);
}

void PicNew8BitController::Close() { driver_->Close(); }

Status PicNew8BitController::ReadDeviceId(uint16_t *device_id, uint16_t *revision) {
  uint32_t address = 0x8005;
  if (device_type_ == PIC18NEW) {
    address = 0x3ffffc;
  }
  RETURN_IF_ERROR(WriteCommand(PicNew8BitCommand::LOAD_PC, address));
  Datastring16 words;
  RETURN_IF_ERROR(ReadWithCommand(PicNew8BitCommand::READ_DATA_INC, 2, &words));

  *device_id = words[1];
  *revision = words[0] & 0xfff;
  return Status::OK;
}

Status PicNew8BitController::Read(Section section, uint32_t start_address, uint32_t end_address,
                                  const DeviceInfo &, Datastring *result) {
  uint32_t pc = start_address;
  if (device_type_ == PIC16NEW) {
    pc /= 2;
  }

  RETURN_IF_ERROR(WriteCommand(PicNew8BitCommand::LOAD_PC, pc));
  Datastring16 words;
  if (device_type_ == PIC18NEW && section == EEPROM) {
    RETURN_IF_ERROR(
        ReadWithCommand(PicNew8BitCommand::READ_DATA_INC, start_address - end_address, &words));
    for (uint16_t word : words) {
      result->push_back(word & 0xff);
    }
  } else {
    RETURN_IF_ERROR(ReadWithCommand(PicNew8BitCommand::READ_DATA_INC,
                                    (start_address - end_address) / 2, &words));
    for (uint16_t word : words) {
      result->push_back(word & 0xff);
      result->push_back(word >> 8);
    }
  }
  return Status::OK;
}

Status PicNew8BitController::Write(Section section, uint32_t address, const Datastring &data,
                                   const DeviceInfo &device_info) {
  // PIC18 EEPROMs are written one byte at a time.
  if (device_type_ == PIC18NEW && section == EEPROM) {
    RETURN_IF_ERROR(WriteCommand(PicNew8BitCommand::LOAD_PC, address));
    for (size_t write_count = 0; write_count < data.size(); ++write_count) {
      PrintProgress(write_count, data.size());
      RETURN_IF_ERROR(WriteCommand(PicNew8BitCommand::LOAD_DATA_INC, data[write_count]));
      RETURN_IF_ERROR(
          WriteTimedSequence(PicNew8BitSequenceGenerator::WRITE_SEQUENCE, &device_info));
    }
    return Status::OK;
  }

  uint32_t block_size = 2;
  if (section == FLASH) {
    block_size = device_info.write_block_size;
  }
  if (address % block_size != 0) {
    return Status(INVALID_ARGUMENT, "Address is not a multiple of the write_block_size");
  }
  if (data.size() % block_size != 0) {
    return Status(INVALID_ARGUMENT, "Data size is not a multiple of the write_block_size");
  }

  uint32_t pc = address;
  if (device_type_ == PIC16NEW) {
    pc /= 2;
  }
  RETURN_IF_ERROR(WriteCommand(PicNew8BitCommand::LOAD_PC, pc));

  for (size_t write_count = 0; write_count < data.size(); write_count += block_size) {
    PrintProgress(write_count, data.size());
    for (uint32_t step = 0; step < block_size; step += 2) {
      uint16_t datum = data[write_count + step + 1];
      datum <<= 8;
      datum |= data[write_count + step];
      RETURN_IF_ERROR(WriteCommand(PicNew8BitCommand::LOAD_DATA_INC, datum));
    }
    RETURN_IF_ERROR(WriteTimedSequence(PicNew8BitSequenceGenerator::WRITE_SEQUENCE, &device_info));
  }
  return Status::OK;
}

Status PicNew8BitController::ChipErase(const DeviceInfo &device_info) {
  return WriteTimedSequence(PicNew8BitSequenceGenerator::CHIP_ERASE_SEQUENCE, &device_info);
}

Status PicNew8BitController::SectionErase(Section, const DeviceInfo &) {
  return Status(UNIMPLEMENTED, "Section erase not implemented");
}

Status PicNew8BitController::WriteCommand(PicNew8BitCommand command, uint16_t payload) {
  return driver_->WriteDatastring(sequence_generator_->GetCommandSequence(command, payload));
}

Status PicNew8BitController::ReadWithCommand(PicNew8BitCommand command, uint32_t count,
                                             Datastring16 *result) {
  result->clear();
  RETURN_IF_ERROR(driver_->ReadWithSequence(sequence_generator_->GetCommandSequence(command, 0),
                                            {12}, 8, count, result, /* lsb_first = */ false));
  return Status::OK;
}

Status PicNew8BitController::WriteTimedSequence(PicNew8BitSequenceGenerator::TimedSequenceType type,
                                                const DeviceInfo *device_info) {
  return driver_->WriteTimedSequence(sequence_generator_->GetTimedSequence(type, device_info));
}
