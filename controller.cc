#include "controller.h"

#include "util.h"

//FIXME: put the following in program.{h,cc}
Status MergeProgramBlocks(Program *program) {
	auto last_section = program->begin();
	for (auto iter = last_section + 1; iter != program->end();) {
		uint32_t last_section_end = last_section->first + last_section->second.size();

		if (last_section_end < iter->first) {
			last_section = iter;
			++iter;
			continue;
		} else if (last_section_end == iter->first) {
			last_section->second.append(iter->second);
			iter = program->erase(iter);
		} else if (last_section_end > iter->first) {
			return Status(Code::INVALID_PROGRAM, "Overlapping sections in program");
		}
	}
	return Status::OK;
}

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

Status Pic18Controller::ReadFlashMemory(uint32_t start_address, uint32_t end_address, Datastring *result) {
	RETURN_IF_ERROR(LoadAddress(start_address));
	return ReadWithCommand(TABLE_READ_post_inc, end_address - start_address, result);
}

Status Pic18Controller::ChipErase() {
	RETURN_IF_ERROR(LoadAddress(0x3C0005));
	// 1100 0F 0F Write 0Fh to 3C0005h
	RETURN_IF_ERROR(WriteCommand(TABLE_WRITE, 0x0F0F));
	RETURN_IF_ERROR(LoadAddress(0x3C0004));
	Datastring sequence;
	// 1100 8F 8F Write 8F8Fh TO 3C0004h to erase entire device.
	RETURN_IF_ERROR(WriteCommand(TABLE_WRITE, 0x8F8F));
	// 0000 00 00 NOP
	RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x0000));
	// 0000 00 00 Hold PGD low until erase completes.
	return WriteTimedSequence(Pic18SequenceGenerator::CHIP_ERASE_SEQUENCE);
}

Status Pic18Controller::WriteFlash(uint32_t address, const Datastring &data, uint32_t block_size) {
	if (address % block_size) {
		FATAL("Attempting to write at a non-aligned address\n%s", "");
	}
	if (data.size() % block_size)  {
		FATAL("Attempting to write incomplete block\n%s", "");
	}
	// BSF EECON1, EEPGD
	RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x8EA6));
	// BCF EECON1, CFGS
	RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x9CA6));
	// BSF EECON1, WREN
	RETURN_IF_ERROR(WriteCommand(CORE_INST, 0x84A6));
	RETURN_IF_ERROR(LoadAddress(address));
	for (size_t i = 0; i < data.size(); i += 64) {
		for (size_t j = 0; j < 62; j += 2) {
			RETURN_IF_ERROR(WriteCommand(TABLE_WRITE_post_inc2, (static_cast<uint16_t>(data[i + j + 1]) << 8) | data[i + j]));
		}
		RETURN_IF_ERROR(WriteCommand(TABLE_WRITE_post_inc2_start_pgm, (static_cast<uint16_t>(data[i + 63]) << 8) | data[i + 62]));
		RETURN_IF_ERROR(WriteTimedSequence(Pic18SequenceGenerator::WRITE_SEQUENCE));
	}
	return Status::OK;
}

Status Pic18Controller::RowErase(uint32_t address) {
	if (address & 63) {
		FATAL("Attempting to erase a non-aligned address\n%s", "");
	}
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

Status HighLevelController::ReadProgram(Program *program) {
	DeviceCloser closer(this);
	RETURN_IF_ERROR(InitDevice());
	printf("Initialized device [%s]\n", device_info_.name.c_str());

	RETURN_IF_ERROR(ReadData(&(*program)[0], 0, device_info_.program_memory_size));
	if (device_info_.user_id_size > 0) {
		RETURN_IF_ERROR(ReadData(&(*program)[device_info_.user_id_offset], device_info_.user_id_offset, device_info_.user_id_size));
	}
	if (device_info_.config_size > 0) {
		RETURN_IF_ERROR(ReadData(&(*program)[device_info_.config_offset], device_info_.config_offset, device_info_.config_size));
	}
	return Status::OK;
}

Status HighLevelController::WriteProgram(const Program &program, EraseMode erase_mode) {
	Program block_aligned_program;
	if (erase_mode == CHIP_ERASE) {
		printf("Starting chip erase\n");
		controller_->ChipErase();
		for (const auto &section : program) {
			if (section.first % device_info_.write_block_size == 0 && section.second.size() % device_info_.write_block_size == 0) {
				block_aligned_program[section.first] = section.second;
				continue;
			}
			// Calculate new aligned offset.
			uint32_t new_offset = (section.first / device_info_.write_block_size) * device_info_.write_block_size;
			Datastring new_data = section.second;
			new_data.insert(0, section.first - new_offset, 0xff);
			new_data.append((device_info_.write_block_size - (new_data.size() % device_info_.write_block_size)) % device_info_.write_block_size, 0xff);
			block_aligned_program[new_offset] = std::move(new_data);
		}
		MergeProgramBlocks(&block_aligned_program);
	}
	//FIXME: for row erase mode, read the incomplete blocks from flash and merge with incomplete blocks. Then erase the relevant rows

	for (const auto &section : program) {
		if (section.first < device_info_.program_memory_size) {
			controller_->WriteFlash(section.first, section.second, device_info_.write_block_size);
		}
	}

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

Status HighLevelController::ReadData(Datastring *data, uint32_t base_address, uint32_t target_size) {
	data->reserve(target_size);
	printf("Starting read at address %06lX to read %06X bytes\n", base_address + data->size(), target_size);
	while (data->size() < target_size) {
		Datastring buffer;
		uint32_t start_address = base_address + data->size();
		Status status = controller_->ReadFlashMemory(start_address, start_address + std::min<uint32_t>(128, target_size - data->size()), &buffer);
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
