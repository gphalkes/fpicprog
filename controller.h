#ifndef CONTROLLER_H_
#define CONTROLLER_H_

#include <memory>
#include <vector>

#include "driver.h"

class Controller {
public:
	Controller(std::unique_ptr<Driver> driver) : driver_(std::move(driver)) {}

	Status ReadDeviceId(uint16_t *device_id);
	Status ReadFlashMemory(uint32_t start_address, uint32_t end_address, datastring *result);
	Status BulkErase();
	Status WriteFlash(uint32_t address, const datastring &data);
	Status RowErase(uint32_t address);

private:
	Status LoadAddress(uint32_t address);

	std::unique_ptr<Driver> driver_;
};


#endif
