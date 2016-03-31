#ifndef CONTROLLER_H_
#define CONTROLLER_H_

#include <map>
#include <memory>
#include <vector>

#include "device_db.h"
#include "driver.h"

// FIXME: put this into its own header with utils for reading and writing
typedef std::map<uint32_t, datastring> Program;

class Controller {
public:
	Controller(std::unique_ptr<Driver> driver) : driver_(std::move(driver)) {}

	Status Open();
	void Close() { driver_->Close(); }
	Status ReadDeviceId(uint16_t *device_id);
	Status ReadFlashMemory(uint32_t start_address, uint32_t end_address, datastring *result);
	Status BulkErase();
	Status WriteFlash(uint32_t address, const datastring &data);
	Status RowErase(uint32_t address);

private:
	Status LoadAddress(uint32_t address);

	std::unique_ptr<Driver> driver_;
};

class HighLevelController {
public:
	HighLevelController(std::unique_ptr<Controller> controller, std::unique_ptr<DeviceDb> device_db)
		: controller_(std::move(controller)), device_db_(std::move(device_db)) {}

	Status ReadProgram(Program *program);

private:
	class DeviceCloser {
	public:
		DeviceCloser(HighLevelController *controller) : controller_(controller) {}
		~DeviceCloser() { controller_->CloseDevice(); }
	private:
		HighLevelController *controller_;
	};
	Status InitDevice();
	void CloseDevice();
	Status ReadData(datastring *data, uint32_t base_address, uint32_t target_size);

	bool device_open_ = false;
	DeviceDb::DeviceInfo device_info_;
	std::unique_ptr<Controller> controller_;
	std::unique_ptr<DeviceDb> device_db_;
};

#endif
