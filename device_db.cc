#include "device_db.h"

#include "strings.h"

Status DeviceDb::Load() {
	// FIXME: this should be loaded from a CSV file for ease of use.
	// Device IDs have revision numbers in the low 5 bits. These are assumed to be 0.
	device_db_[0x5c60] = DeviceInfo{0x4000, 8, 0x200000, 14, 0x300000}; // 18F24K50
	device_db_[0x5ce0] = DeviceInfo{0x4000, 8, 0x200000, 14, 0x300000}; // 18LF24K50

	device_db_[0x5c20] = DeviceInfo{0x8000, 8, 0x200000, 14, 0x300000}; // 18F25K50
	device_db_[0x5ca0] = DeviceInfo{0x8000, 8, 0x200000, 14, 0x300000}; // 18LF25K50
	device_db_[0x5c00] = DeviceInfo{0x8000, 8, 0x200000, 14, 0x300000}; // 18F45K50
	device_db_[0x5c80] = DeviceInfo{0x8000, 8, 0x200000, 14, 0x300000}; // 18LF45K50

	device_db_[0x5d20] = DeviceInfo{0x8000, 8, 0x200000, 14, 0x300000}; // 18F26K50
	device_db_[0x5d60] = DeviceInfo{0x8000, 8, 0x200000, 14, 0x300000}; // 18LF26K50
	device_db_[0x5d00] = DeviceInfo{0x8000, 8, 0x200000, 14, 0x300000}; // 18F46K50
	device_db_[0x5d40] = DeviceInfo{0x8000, 8, 0x200000, 14, 0x300000}; // 18LF46K50
	return Status::OK;
}

Status DeviceDb::GetDeviceInfo(uint16_t device_id, DeviceInfo *device_info) {
	device_id &= 0xffe0;
	if (device_db_.find(device_id) == device_db_.end()) {
		// FIXME: Print hex device ID
		return Status(Code::DEVICE_NOT_FOUND, strings::Cat("Device with id ", device_id, " not found"));
	}
	*device_info = device_db_.at(device_id);
	return Status::OK;
}
