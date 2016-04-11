#include "high_level_controller.h"

#include "status.h"

Status HighLevelController::ReadProgram(const std::vector<Section> &sections, Program *program) {
  DeviceCloser closer(this);
  RETURN_IF_ERROR(InitDevice());
  printf("Initialized device [%s]\n", device_info_.name.c_str());

  std::set<Section> sections_set(sections.begin(), sections.end());
  if (ContainsKey(sections_set, FLASH)) {
    RETURN_IF_ERROR(ReadData(FLASH, &(*program)[0], 0, device_info_.program_memory_size));
  }
  if (ContainsKey(sections_set, USER_ID) && device_info_.user_id_size > 0) {
    RETURN_IF_ERROR(ReadData(USER_ID, &(*program)[device_info_.user_id_offset],
                             device_info_.user_id_offset, device_info_.user_id_size));
  }
  if (ContainsKey(sections_set, CONFIGURATION) && device_info_.config_size > 0) {
    RETURN_IF_ERROR(ReadData(CONFIGURATION, &(*program)[device_info_.config_offset],
                             device_info_.config_offset, device_info_.config_size));
  }
  if (ContainsKey(sections_set, EEPROM) && device_info_.eeprom_size > 0) {
    RETURN_IF_ERROR(ReadData(EEPROM, &(*program)[device_info_.eeprom_offset],
                             device_info_.eeprom_offset, device_info_.eeprom_size));
  }
  return Status::OK;
}

Status HighLevelController::WriteProgram(const std::vector<Section> &sections,
                                         const Program &program, EraseMode erase_mode) {
  std::set<Section> write_sections(sections.begin(), sections.end());
  DeviceCloser closer(this);
  RETURN_IF_ERROR(InitDevice());

  Program block_aligned_program = program;

  std::vector<std::pair<uint32_t, uint32_t>> missing_ranges;
  uint32_t last_end = 0;
  for (const auto &section : program) {
    if (section.first >= device_info_.program_memory_size) {
      break;
    }
    if (last_end != section.first) {
      if (last_end > section.first) {
        fatal("Program has overlapping sections\n");
      }
      missing_ranges.emplace_back(last_end, section.first);
    }
    last_end = section.first + section.second.size();
  }
  if (last_end < device_info_.program_memory_size) {
    missing_ranges.emplace_back(last_end, device_info_.program_memory_size);
  }

  uint32_t block_size =
      erase_mode == ROW_ERASE ? device_info_.erase_block_size : device_info_.write_block_size;
  for (const auto &range : missing_ranges) {
    uint32_t lower = ((range.first + block_size - 1) / block_size) * block_size;
    uint32_t higher = (range.second / block_size) * block_size;
    if (lower < higher) {
      if (erase_mode == ROW_ERASE) {
        Datastring data;
        if (range.first != lower) {
          RETURN_IF_ERROR(ReadData(FLASH, &data, range.first, lower - range.first));
          block_aligned_program[range.first] = data;
        }
        if (range.second != higher) {
          RETURN_IF_ERROR(ReadData(FLASH, &data, higher, range.second - higher));
          block_aligned_program[higher] = data;
        }
      } else {
        if (range.first != lower) {
          block_aligned_program[range.first].assign(lower - range.first, 0xff);
        }
        if (range.second != higher) {
          block_aligned_program[higher].assign(range.second - higher, 0xff);
        }
      }
    } else {
      if (erase_mode == ROW_ERASE) {
        Datastring data;
        RETURN_IF_ERROR(ReadData(FLASH, &data, range.first, range.second - range.first));
        block_aligned_program[range.first] = data;
      } else {
        block_aligned_program[range.first].assign(range.second - range.first, 0xff);
      }
    }
  }
  RETURN_IF_ERROR(MergeProgramBlocks(&block_aligned_program, device_info_));

  printf("Program section addresses + sizes dump\n");
  for (const auto &section : block_aligned_program) {
    printf("Section: %06x-%06zx\n", section.first, section.first + section.second.size());
  }

  std::set<Section> erase_sections;
  for (const auto &section : block_aligned_program) {
    if (section.first < device_info_.program_memory_size) {
      erase_sections.insert(FLASH);
    } else if (section.first >= device_info_.user_id_offset &&
               section.first < device_info_.user_id_offset + device_info_.user_id_size) {
      erase_sections.insert(USER_ID);
    } else if (section.first >= device_info_.config_offset &&
               section.first < device_info_.config_offset + device_info_.config_size) {
      erase_sections.insert(CONFIGURATION);
    } else if (section.first >= device_info_.eeprom_offset &&
               section.first < device_info_.eeprom_offset + device_info_.eeprom_size) {
      erase_sections.insert(EEPROM);
    }
  }
  set_subtract(&erase_sections, write_sections);

  switch (erase_mode) {
    case CHIP_ERASE:
      printf("Starting chip erase\n");
      RETURN_IF_ERROR(controller_->ChipErase(device_info_));
      break;
    case ROW_ERASE:
      erase_sections.erase(FLASH);
      erase_sections.erase(EEPROM);
    // FALLTHROUGH
    case SECTION_ERASE:
      if (ContainsKey(erase_sections, FLASH)) {
        printf("Starting flash erase\n");
        RETURN_IF_ERROR(controller_->SectionErase(FLASH, device_info_));
      }
      if (ContainsKey(erase_sections, USER_ID)) {
        printf("Starting user ID erase\n");
        RETURN_IF_ERROR(controller_->SectionErase(USER_ID, device_info_));
      }
      if (ContainsKey(erase_sections, CONFIGURATION)) {
        printf("Starting configuration bits erase\n");
        RETURN_IF_ERROR(controller_->SectionErase(CONFIGURATION, device_info_));
      }
      if (ContainsKey(erase_sections, EEPROM)) {
        printf("Starting EEPROM erase\n");
        RETURN_IF_ERROR(controller_->SectionErase(EEPROM, device_info_));
      }
      break;
    case NO_ERASE:
      break;
    default:
      fatal("Unsupported erase mode\n");
  }

  for (const auto &section : block_aligned_program) {
    printf("Writing section %06x-%06lx\n", section.first, section.first + section.second.size());
    if (ContainsKey(write_sections, FLASH) && section.first < device_info_.program_memory_size) {
      if (erase_mode == ROW_ERASE) {
        printf("Erasing flash rows\n");
        // Erase the relevant blocks. The program sections have been aligned and sized to be
        // a multiple of the erase block size.
        for (uint32_t address = section.first; address < section.first + section.second.size();
             address += device_info_.erase_block_size) {
          RETURN_IF_ERROR(controller_->RowErase(address));
        }
      }
      printf("Writing flash data\n");
      RETURN_IF_ERROR(
          controller_->Write(FLASH, section.first, section.second, device_info_.write_block_size));
      printf("Verifying written flash data\n");
      RETURN_IF_ERROR(VerifyData(FLASH, section.second, section.first));
    } else if (ContainsKey(write_sections, USER_ID) &&
               section.first >= device_info_.user_id_offset &&
               section.first < device_info_.user_id_offset + device_info_.user_id_size) {
      RETURN_IF_ERROR(
          controller_->Write(USER_ID, section.first, section.second, device_info_.user_id_size));
      printf("Verifying written user ID data\n");
      RETURN_IF_ERROR(VerifyData(USER_ID, section.second, section.first));
    } else if (ContainsKey(write_sections, CONFIGURATION) &&
               section.first >= device_info_.config_offset &&
               section.first < device_info_.config_offset + device_info_.config_size) {
      RETURN_IF_ERROR(controller_->Write(CONFIGURATION, section.first, section.second, 1));
      printf("Verifying written configuration data\n");
      RETURN_IF_ERROR(VerifyData(CONFIGURATION, section.second, section.first));
    } else if (ContainsKey(write_sections, EEPROM) && section.first >= device_info_.eeprom_offset &&
               section.first < device_info_.eeprom_offset + device_info_.eeprom_size) {
      RETURN_IF_ERROR(controller_->Write(EEPROM, section.first, section.second, 1));
      printf("Verifying written EEPROM data\n");
      RETURN_IF_ERROR(VerifyData(EEPROM, section.second, section.first));
    }
  }

  return Status::OK;
}

Status HighLevelController::ChipErase() {
  DeviceCloser closer(this);
  RETURN_IF_ERROR(InitDevice());

  return controller_->ChipErase(device_info_);
}

Status HighLevelController::SectionErase(const std::vector<Section> &sections) {
  DeviceCloser closer(this);
  RETURN_IF_ERROR(InitDevice());

  for (auto section : sections) {
    RETURN_IF_ERROR(controller_->SectionErase(section, device_info_));
  }
  return Status::OK;
}

Status HighLevelController::Identify() {
  DeviceCloser closer(this);
  RETURN_IF_ERROR(InitDevice());
  printf("Initialized device [%s]\n", device_info_.name.c_str());
  return Status::OK;
}

Status HighLevelController::InitDevice() {
  if (device_open_) {
    return Status::OK;
  }
  Status status;
  uint16_t device_id, revision;
  for (int attempts = 0; attempts < 10; ++attempts) {
    status = controller_->Open(lvp_);
    if (!status.ok()) {
      controller_->Close();
      continue;
    }
    status = controller_->ReadDeviceId(&device_id, &revision);
    if (!status.ok() || device_id == 0) {
      controller_->Close();
      continue;
    }
    status = device_db_->GetDeviceInfo(device_id, &device_info_);
    if (status.ok()) {
      device_open_ = true;
    }
    return status;
  }
  return status;
}

void HighLevelController::CloseDevice() {
  if (!device_open_) {
    return;
  }
  device_open_ = false;
  controller_->Close();
}

Status HighLevelController::ReadData(Section section, Datastring *data, uint32_t base_address,
                                     uint32_t target_size) {
  data->reserve(target_size);
  printf("Starting read at address %06lX to read %06X bytes\n", base_address + data->size(),
         target_size);
  while (data->size() < target_size) {
    Datastring buffer;
    uint32_t start_address = base_address + data->size();
    Status status = controller_->Read(
        section, start_address, start_address + std::min<uint32_t>(128, target_size - data->size()),
        &buffer);
    if (status.ok()) {
      data->append(buffer);
    } else if (status.code() == Code::SYNC_LOST) {
      uint16_t device_id, revision;
      Status device_id_read_status;
      // Attempt to read the device ID. As this may also fail with SYNC_LOST,
      // 10 attempts are made. If all of those fail, then something is really wrong.
      for (int i = 0; i < 10; ++i) {
        device_id_read_status = controller_->ReadDeviceId(&device_id, &revision);
        if (device_id_read_status.code() != Code::SYNC_LOST) {
          break;
        }
      }
      RETURN_IF_ERROR(device_id_read_status);
      if (device_id != device_info_.device_id) {
        return status;
      }
    } else {
      return status;
    }
  }
  return Status::OK;
}

Status HighLevelController::VerifyData(Section, const Datastring &data, uint32_t base_address) {
  Datastring written_data;
  RETURN_IF_ERROR(
      controller_->Read(FLASH, base_address, base_address + data.size(), &written_data));
  if (written_data != data) {
    std::string data_as_bytes;
    for (const uint8_t byte : data) {
      data_as_bytes += HexByte(byte);
    }
    printf("Data        : %s\n", data_as_bytes.c_str());
    data_as_bytes.clear();
    for (const uint8_t byte : written_data) {
      data_as_bytes += HexByte(byte);
    }
    printf("Written data: %s\n", data_as_bytes.c_str());

    return Status(Code::VERIFICATION_ERROR, "Data read back is not what was written");
  }
  return Status::OK;
}
