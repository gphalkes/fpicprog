#ifndef DEVICE_DB_H_
#define DEVICE_DB_H_

#include <map>

#include "status.h"

class DeviceDb {
public:
	struct DeviceInfo {
		std::string name;
		uint16_t device_id;
		uint32_t program_memory_size;
		uint32_t user_id_size;
		uint32_t user_id_offset;
		uint32_t config_size;
		uint32_t config_offset;
	};

	Status Load();

	Status GetDeviceInfo(uint16_t device_id, DeviceInfo *device_info);

private:
	std::map<uint16_t, DeviceInfo> device_db_;
};

#endif
