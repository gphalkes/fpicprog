#ifndef CONTROLLER_H_
#define CONTROLLER_H_

#include <memory>
#include <vector>

#include "driver.h"

class Controller {
public:
	Controller(std::unique_ptr<Driver> driver) : driver_(std::move(driver)) {}

	uint16_t ReadDeviceId();
	datastring ReadFlashMemory(uint32_t start_address, uint32_t end_address);
	void BulkErase();
	void WriteFlash(uint32_t address, const datastring &data);
	void RowErase(uint32_t address);

private:
	void LoadAddress(uint32_t address);

	std::unique_ptr<Driver> driver_;
};


#endif
