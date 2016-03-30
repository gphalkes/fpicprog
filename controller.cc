#include "controller.h"

#include "util.h"

uint16_t Controller::ReadDeviceId() {
    uint16_t result;
    LoadAddress(0x3ffffe);
    datastring bytes = driver_->ReadWithCommand(TABLE_READ_post_inc, 2);
    result = bytes[0] | static_cast<uint16_t>(bytes[1]) << 8;
    return result;
}

datastring Controller::ReadFlashMemory(uint32_t start_address, uint32_t end_address) {
	LoadAddress(start_address);
	return driver_->ReadWithCommand(TABLE_READ_post_inc, end_address - start_address);
}

void Controller::BulkErase() {
	LoadAddress(0x3C0005);
	// 1100 0F 0F Write 0Fh to 3C0005h
	driver_->WriteCommand(TABLE_WRITE, 0x0F0F);
	LoadAddress(0x3C0004);
	datastring sequence;
	// 1100 8F 8F Write 8F8Fh TO 3C0004h to erase entire device.
	driver_->WriteCommand(TABLE_WRITE, 0x8F8F);
	// 0000 00 00 NOP
	driver_->WriteCommand(CORE_INST, 0x0000);
	// 0000 00 00 Hold PGD low until erase completes.
	driver_->WriteTimedSequence(SequenceGenerator::BULK_ERASE_SEQUENCE);
}

void Controller::WriteFlash(uint32_t address, const datastring &data) {
	if (address & 63) {
		FATAL("Attempting to write at a non-aligned address\n%s", "");
	}
	if (data.size() & 63)  {
		FATAL("Attempting to write less than one block of data\n%s", "");
	}
	// BSF EECON1, EEPGD
	driver_->WriteCommand(CORE_INST, 0x8EA6);
	// BCF EECON1, CFGS
	driver_->WriteCommand(CORE_INST, 0x9CA6);
	// BSF EECON1, WREN
	driver_->WriteCommand(CORE_INST, 0x84A6);
	LoadAddress(address);
	for (size_t i = 0; i < data.size(); i += 64) {
		for (size_t j = 0; j < 62; j += 2) {
			driver_->WriteCommand(TABLE_WRITE_post_inc2, (static_cast<uint16_t>(data[i + j + 1]) << 8) | data[i + j]);
		}
		driver_->WriteCommand(TABLE_WRITE_post_inc2_start_pgm, (static_cast<uint16_t>(data[i + 63]) << 8) | data[i + 62]);
		driver_->WriteTimedSequence(SequenceGenerator::WRITE_SEQUENCE);
	}
}

void Controller::RowErase(uint32_t address) {
	if (address & 63) {
		FATAL("Attempting to erase a non-aligned address\n%s", "");
	}
	// BSF EECON1, EEPGD
	driver_->WriteCommand(CORE_INST, 0x8EA6);
	// BCF EECON1, CFGS
	driver_->WriteCommand(CORE_INST, 0x9CA6);
	// BSF EECON1, WREN,
	driver_->WriteCommand(CORE_INST, 0x84A6);
	LoadAddress(address);
	// BSF EECON1, FREE
	driver_->WriteCommand(CORE_INST, 0x88A6);
	// BSR EECON1, WR
	driver_->WriteCommand(CORE_INST, 0x82A6);
	// NOP
	driver_->WriteCommand(CORE_INST, 0x0000);
	// NOP
	driver_->WriteCommand(CORE_INST, 0x0000);

	// Loop until the WR bit in EECON1 is clear.
	datastring value;
	do {
		// MOVF EECON1, W, 0
		driver_->WriteCommand(CORE_INST, 0x50A6);
		// MOVWF TABLAT
		driver_->WriteCommand(CORE_INST, 0x6EF5);
		// NOP
		driver_->WriteCommand(CORE_INST, 0x0000);
		// Read value from TABLAT
		value = driver_->ReadWithCommand(SHIFT_OUT_TABLAT, 1);
	} while (value[0] & 2);
	Sleep(MicroSeconds(200));
	// BCF EECON1, WREN
	driver_->WriteCommand(CORE_INST, 0x9AA6);
}

void Controller::LoadAddress(uint32_t address) {
	// MOVLW <first byte of address>
	driver_->WriteCommand(CORE_INST, 0x0E00 | ((address >> 16) & 0xff));
	// MOVWF TBLPTRU
	driver_->WriteCommand(CORE_INST, 0x6EF8);
	// MOVLW <second byte of address>
	driver_->WriteCommand(CORE_INST, 0x0E00 | ((address >> 8) & 0xff));
	// MOVWF TBLPTRH
	driver_->WriteCommand(CORE_INST, 0x6EF7);
	 // MOVLW <last byte of address>
	driver_->WriteCommand(CORE_INST, 0x0E00 | (address & 0xff));
	// MOVWF TBLPTRL
	driver_->WriteCommand(CORE_INST, 0x6EF6);
}

