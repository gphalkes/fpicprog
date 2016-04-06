#include "controller.h"

#include <set>

#include "strings.h"
#include "util.h"

Status Pic18Controller::Open() {
	RETURN_IF_ERROR(driver_->Open());
	return WriteTimedSequence(Pic18SequenceGenerator::INIT_SEQUENCE);
}

void Pic18Controller::Close() {
	//FIXME: use the correct sequence to shutdown the device
	driver_->Close();
}

Status Pic18Controller::ReadDeviceId(uint16_t *device_id, uint16_t *revision) {
    RETURN_IF_ERROR(LoadAddress(0x3ffffe));
    Datastring bytes;
	RETURN_IF_ERROR(ReadWithCommand(TABLE_READ_post_inc, 2, &bytes));
    *device_id = bytes[0] | static_cast<uint16_t>(bytes[1]) << 8;
    *revision = *device_id & 0x1f;
    *device_id &= 0xffe0;
    return Status::OK;
}

Status Pic18Controller::Read(Section section, uint32_t start_address, uint32_t end_address, Datastring *result) {
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

Status Pic18Controller::Write(Section section, uint32_t address, const Datastring &data, uint32_t block_size) {
	if (section == FLASH || section == USER_ID) {
		if (block_size % 2 != 0 || block_size < 2) {
			return Status(Code::INVALID_ARGUMENT, "Block size for writing must be a multiple of 2");
		}
		// BSF EECON1, EEPGD
		RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x8EA6));
		// BCF EECON1, CFGS
		RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x9CA6));
		// BSF EECON1, WREN
		RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x84A6));
		RETURN_IF_ERROR(LoadAddress(address));
		for (size_t i = 0; i < data.size(); i += block_size) {
			for (size_t j = 0; j < block_size - 2; j += 2) {
				RETURN_IF_ERROR(WriteCommand(TABLE_WRITE_post_inc2, (static_cast<uint16_t>(data[i + j + 1]) << 8) | data[i + j]));
			}
			RETURN_IF_ERROR(WriteCommand(TABLE_WRITE_post_inc2_start_pgm, (static_cast<uint16_t>(data[i + 63]) << 8) | data[i + 62]));
			RETURN_IF_ERROR(WriteTimedSequence(Pic18SequenceGenerator::WRITE_SEQUENCE));
		}
		return Status::OK;
	} else {
		return Status(Code::UNIMPLEMENTED, strings::Cat("Writing of section ", section, " not implemented"));
	}
}

Status Pic18Controller::ChipErase(const DeviceDb::DeviceInfo &device_info) {
	return ExecuteBulkErase(device_info.chip_erase_sequence, device_info.bulk_erase_timing);
}

Status Pic18Controller::SectionErase(Section section, const DeviceDb::DeviceInfo &device_info) {
	switch (section) {
		case FLASH:
			return ExecuteBulkErase(device_info.flash_erase_sequence, device_info.bulk_erase_timing);
		case USER_ID:
			return ExecuteBulkErase(device_info.user_id_erase_sequence, device_info.bulk_erase_timing);
		case CONFIGURATION:
			return ExecuteBulkErase(device_info.config_erase_sequence, device_info.bulk_erase_timing);
		case EEPROM:
			return ExecuteBulkErase(device_info.eeprom_erase_sequence, device_info.bulk_erase_timing);
		default:
			return Status(Code::UNIMPLEMENTED, strings::Cat("Section erase not implemented for section type ", section));
	}
}

Status Pic18Controller::RowErase(uint32_t address) {
	// BSF EECON1, EEPGD
	RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x8EA6));
	// BCF EECON1, CFGS
	RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x9CA6));
	// BSF EECON1, WREN,
	RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x84A6));
	RETURN_IF_ERROR(LoadAddress(address));
	// BSF EECON1, FREE
	RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x88A6));
	// BSR EECON1, WR
	RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x82A6));
	// NOP
	RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x0000));
	// NOP
	RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x0000));

	// Loop until the WR bit in EECON1 is clear.
	Datastring value;
	do {
		// MOVF EECON1, W, 0
		RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x50A6));
		// MOVWF TABLAT
		RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x6EF5));
		// NOP
		RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x0000));
		// Read value from TABLAT
		RETURN_IF_ERROR(ReadWithCommand(SHIFT_OUT_TABLAT, 1, &value));
	} while (value[0] & 2);
	Sleep(MicroSeconds(200));
	// BCF EECON1, WREN
	return WriteCommand(CORE_INST, 0x9AA6);
}

Status Pic18Controller::WriteCommand(Command command, uint16_t payload) {
	return driver_->WriteDatastring(sequence_generator_->GetCommandSequence(command, payload));
}

Status Pic18Controller::ReadWithCommand(Command command, uint32_t count, Datastring *result) {
	Datastring16 data;
	RETURN_IF_ERROR(driver_->ReadWithSequence(sequence_generator_->GetCommandSequence(command, 0), 12, 8, count, &data));
	result->clear();
	for (const uint16_t c : data) {
		result->push_back(c);
	}
	return Status::OK;
}

Status Pic18Controller::WriteTimedSequence(Pic18SequenceGenerator::TimedSequenceType type) {
	return driver_->WriteTimedSequence(sequence_generator_->GetTimedSequence(type));
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

Status Pic18Controller::ExecuteBulkErase(const Datastring16 &sequence, Duration bulk_erase_timing) {
	auto timed_sequence = sequence_generator_->GetTimedSequence(Pic18SequenceGenerator::BULK_ERASE_SEQUENCE);
	timed_sequence.back().sleep = bulk_erase_timing;
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

Status HighLevelController::WriteProgram(const std::vector<Section> &sections, const Program &program, EraseMode erase_mode) {
	DeviceCloser closer(this);
	RETURN_IF_ERROR(InitDevice());

	Program block_aligned_program;

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

	uint32_t block_size = erase_mode == ROW_ERASE ? device_info_.erase_block_size : device_info_.write_block_size;
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
	// FIXME: pad user ID

	bool flash_erase = false;
	bool user_id_erase = false;
	bool config_erase = false;
	bool eeprom_erase = false;
	for (const auto &section : program) {
		block_aligned_program[section.first] = section.second;
		if (section.first < device_info_.program_memory_size) {
			flash_erase = true;
		} else if (section.first >= device_info_.user_id_offset &&
				section.first < device_info_.user_id_offset + device_info_.user_id_size) {
			user_id_erase = true;
		} else if (section.first >= device_info_.config_offset &&
				section.first < device_info_.config_offset + device_info_.config_size) {
			config_erase = true;
		} else if (section.first >= device_info_.eeprom_offset &&
				section.first < device_info_.eeprom_offset + device_info_.eeprom_size) {
			eeprom_erase = true;
		}
	}
	RETURN_IF_ERROR(MergeProgramBlocks(&block_aligned_program, device_info_));

	printf("Program section addresses + sizes dump\n");
	for (const auto &section : block_aligned_program) {
		printf("Section: %06x-%06zx", section.first, section.first + section.second.size());
	}

	printf("Not actually writing :-)\n");
	return Status::OK;

	switch (erase_mode) {
		case CHIP_ERASE:
			printf("Starting chip erase\n");
			RETURN_IF_ERROR(controller_->ChipErase(device_info_));
			break;
		case ROW_ERASE:
			flash_erase = false;
			eeprom_erase = false;
			// FALLTHROUGH
		case SECTION_ERASE:
			if (flash_erase) {
				printf("Starting flash erase\n");
				RETURN_IF_ERROR(controller_->SectionErase(FLASH, device_info_));
			}
			if (user_id_erase) {
				printf("Starting user ID erase\n");
				RETURN_IF_ERROR(controller_->SectionErase(USER_ID, device_info_));
			}
			if (config_erase) {
				printf("Starting configuration bits erase\n");
				RETURN_IF_ERROR(controller_->SectionErase(CONFIGURATION, device_info_));
			}
			if (eeprom_erase) {
				printf("Starting EEPROM erase\n");
				RETURN_IF_ERROR(controller_->SectionErase(EEPROM, device_info_));
			}
			break;
		case NONE:
			break;
		default:
			fatal("Unsupported erase mode\n");
	}

	for (const auto &section : program) {
		if (section.first < device_info_.program_memory_size) {
			if (erase_mode == ROW_ERASE) {
				// Erase the relevant blocks. The program sections have been aligned and sized to be
				// a multiple of the erase block size.
				for (uint32_t address = section.first; address < section.first + section.second.size();
						address += device_info_.erase_block_size) {
					RETURN_IF_ERROR(controller_->RowErase(address));
				}
			}
			RETURN_IF_ERROR(controller_->Write(FLASH, section.first, section.second, device_info_.write_block_size));
		} else if (section.first >= device_info_.user_id_offset &&
				section.first < device_info_.user_id_offset + device_info_.user_id_size) {
			RETURN_IF_ERROR(controller_->Write(USER_ID, section.first, section.second, device_info_.user_id_size));
		} else if (section.first >= device_info_.config_offset &&
				section.first < device_info_.config_offset + device_info_.config_size) {
			RETURN_IF_ERROR(controller_->Write(CONFIGURATION, section.first, section.second, 1));
		} else if (section.first >= device_info_.eeprom_offset &&
				section.first < device_info_.eeprom_offset + device_info_.eeprom_size) {
			RETURN_IF_ERROR(controller_->Write(EEPROM, section.first, section.second, 1));
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
		status = controller_->Open();
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

Status HighLevelController::ReadData(Section section, Datastring *data, uint32_t base_address, uint32_t target_size) {
	data->reserve(target_size);
	printf("Starting read at address %06lX to read %06X bytes\n", base_address + data->size(), target_size);
	while (data->size() < target_size) {
		Datastring buffer;
		uint32_t start_address = base_address + data->size();
		Status status = controller_->Read(
				section, start_address, start_address + std::min<uint32_t>(128, target_size - data->size()), &buffer);
		if (status.ok()) {
			data->append(buffer);
		} else if (status.code() == Code::SYNC_LOST) {
			uint16_t device_id, revision;
			RETURN_IF_ERROR(controller_->ReadDeviceId(&device_id, &revision));
			if (device_id != device_info_.device_id) {
				return status;
			}
		} else {
			return status;
		}
	}
	return Status::OK;
}
