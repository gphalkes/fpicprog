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
	virtual ~Controller() = default;

	virtual Status Open() = 0;
	virtual void Close() = 0;
	virtual Status ReadDeviceId(uint16_t *device_id, uint16_t *revision) = 0;
	virtual Status ReadFlashMemory(uint32_t start_address, uint32_t end_address, datastring *result) = 0;
	virtual Status ChipErase() = 0;
	virtual Status WriteFlash(uint32_t address, const datastring &data) = 0;
	virtual Status RowErase(uint32_t address) = 0;
};

class Pic18Controller : public Controller {
public:
	Pic18Controller(std::unique_ptr<Driver> driver, std::unique_ptr<Pic18SequenceGenerator> sequence_generator)
		: driver_(std::move(driver)), sequence_generator_(std::move(sequence_generator)) {}

	Status Open() override;
	void Close() override;
	Status ReadDeviceId(uint16_t *device_id, uint16_t *revision) override;
	Status ReadFlashMemory(uint32_t start_address, uint32_t end_address, datastring *result) override;
	Status ChipErase() override;
	Status WriteFlash(uint32_t address, const datastring &data) override;
	Status RowErase(uint32_t address) override;

private:
	Status WriteCommand(Command command, uint16_t payload);
	Status ReadWithCommand(Command command, uint32_t count, datastring *result);
	Status WriteTimedSequence(Pic18SequenceGenerator::TimedSequenceType type);
	Status LoadAddress(uint32_t address);

	std::unique_ptr<Driver> driver_;
	std::unique_ptr<Pic18SequenceGenerator> sequence_generator_;
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
