#ifndef DEVICE_DB_H_
#define DEVICE_DB_H_

#include <map>

#include "status.h"
#include "util.h"

struct DeviceInfo {
  std::string name;
  uint16_t device_id;
  uint32_t program_memory_size;
  uint32_t user_id_size;
  uint32_t user_id_offset;
  uint32_t config_size;
  uint32_t config_offset;
  uint32_t eeprom_size;
  uint32_t eeprom_offset;
  uint16_t write_block_size;
  uint16_t erase_block_size;
  Datastring16 chip_erase_sequence;
  Datastring16 user_id_erase_sequence;
  Datastring16 eeprom_erase_sequence;
  Datastring16 config_erase_sequence;
  Datastring16 flash_erase_sequence;
  Duration bulk_erase_timing;
};

class DeviceDb {
 public:
  Status Load();

  Status GetDeviceInfo(uint16_t device_id, DeviceInfo *device_info);

 private:
  std::map<uint16_t, DeviceInfo> device_db_;
};

#endif
