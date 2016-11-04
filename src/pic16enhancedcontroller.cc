#include "pic16enhancedcontroller.h"

Status Pic16EnhancedController::Open() {
  RETURN_IF_ERROR(driver_->Open());
  return WriteTimedSequence(Pic16NewSequenceGenerator::INIT_SEQUENCE, nullptr);
}

void Pic16EnhancedController::Close() { driver_->Close(); }

Status Pic16EnhancedController::ReadDeviceId(uint16_t *device_id, uint16_t *revision) {
  RETURN_IF_ERROR(WriteCommand(Pic16NewCommand::LOAD_PC, 0x8005));
  Datastring16 words;
  RETURN_IF_ERROR(ReadWithCommand(Pic16NewCommand::READ_DATA_INC, 2, &words));

  *device_id = words[1];
  *revision = words[0] & 0xfff;
  return Status::OK;
}

Status Pic16EnhancedController::Read(Section, uint32_t start_address, uint32_t end_address,
                                     const DeviceInfo &, Datastring *result) {
  RETURN_IF_ERROR(WriteCommand(Pic16NewCommand::LOAD_PC, start_address / 2));
  Datastring16 words;
  RETURN_IF_ERROR(
      ReadWithCommand(Pic16NewCommand::READ_DATA_INC, (start_address - end_address) / 2, &words));
  for (uint16_t word : words) {
    result->push_back(word & 0xff);
    result->push_back(word >> 8);
  }
  return Status::OK;
}

Status Pic16EnhancedController::Write(Section section, uint32_t address, const Datastring &data,
                                      const DeviceInfo &device_info) {
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
  RETURN_IF_ERROR(WriteCommand(Pic16NewCommand::LOAD_PC, address / 2));

  for (size_t write_count = 0; write_count < data.size(); write_count += block_size) {
    PrintProgress(write_count, data.size());
    for (uint32_t step = 0; step < block_size; step += 2) {
      uint16_t datum = data[write_count + step + 1];
      datum <<= 8;
      datum |= data[write_count + step];
      RETURN_IF_ERROR(WriteCommand(Pic16NewCommand::LOAD_DATA_INC, datum));
    }
    RETURN_IF_ERROR(WriteTimedSequence(Pic16NewSequenceGenerator::WRITE_SEQUENCE, &device_info));
  }
  return Status::OK;
}

Status Pic16EnhancedController::ChipErase(const DeviceInfo &device_info) {
  return WriteTimedSequence(Pic16NewSequenceGenerator::CHIP_ERASE_SEQUENCE, &device_info);
}

Status Pic16EnhancedController::SectionErase(Section, const DeviceInfo &) {
  return Status(UNIMPLEMENTED, "Section erase not implemented");
}

Status Pic16EnhancedController::WriteCommand(Pic16NewCommand command, uint16_t payload) {
  return driver_->WriteDatastring(sequence_generator_->GetCommandSequence(command, payload));
}

Status Pic16EnhancedController::ReadWithCommand(Pic16NewCommand command, uint32_t count,
                                                Datastring16 *result) {
  result->clear();
  RETURN_IF_ERROR(driver_->ReadWithSequence(sequence_generator_->GetCommandSequence(command, 0), {12},
                                            8, count, result, /* lsb_first = */ false));
  return Status::OK;
}

Status Pic16EnhancedController::WriteTimedSequence(
    Pic16NewSequenceGenerator::TimedSequenceType type, const DeviceInfo *device_info) {
  return driver_->WriteTimedSequence(sequence_generator_->GetTimedSequence(type, device_info));
}
