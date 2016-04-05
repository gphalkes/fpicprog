#ifndef CONTROLLER_H_
#define CONTROLLER_H_

#include <map>
#include <memory>
#include <vector>

#include "device_db.h"
#include "driver.h"
#include "program.h"

// FIXME: put this into its own header with utils for reading and writing

class Controller {
public:
	enum Section {
		FLASH,
		USER_ID,
		CONFIGURATION,
		EEPROM,
	};

	virtual ~Controller() = default;

	virtual Status Open() = 0;
	virtual void Close() = 0;
	virtual Status ReadDeviceId(uint16_t *device_id, uint16_t *revision) = 0;
	virtual Status Read(Section section, uint32_t start_address, uint32_t end_address, Datastring *result) = 0;
	virtual Status Write(Section section, uint32_t address, const Datastring &data, uint32_t block_size) = 0;
	virtual Status ChipErase(const DeviceDb::DeviceInfo &device_info) = 0;
	virtual Status SectionErase(Section section, const DeviceDb::DeviceInfo &device_info) = 0;
	virtual Status RowErase(uint32_t address) = 0;
};

class Pic18Controller : public Controller {
public:
	Pic18Controller(std::unique_ptr<Driver> driver, std::unique_ptr<Pic18SequenceGenerator> sequence_generator)
		: driver_(std::move(driver)), sequence_generator_(std::move(sequence_generator)) {}

	Status Open() override;
	void Close() override;
	Status ReadDeviceId(uint16_t *device_id, uint16_t *revision) override;
	Status Read(Section section, uint32_t start_address, uint32_t end_address, Datastring *result) override;
	Status Write(Section section, uint32_t address, const Datastring &data, uint32_t block_size) override;
	Status ChipErase(const DeviceDb::DeviceInfo &device_info) override;
	Status SectionErase(Section section, const DeviceDb::DeviceInfo &device_info) override;
	Status RowErase(uint32_t address) override;

private:
	Status WriteCommand(Command command, uint16_t payload);
	Status ReadWithCommand(Command command, uint32_t count, Datastring *result);
	Status WriteTimedSequence(Pic18SequenceGenerator::TimedSequenceType type);
	Status LoadAddress(uint32_t address);
	Status LoadEepromAddress(uint32_t address);
	Status ExecuteBulkErase(const Datastring16 &sequence, Duration bulk_erase_timing);

	std::unique_ptr<Driver> driver_;
	std::unique_ptr<Pic18SequenceGenerator> sequence_generator_;
};

class HighLevelController {
public:
	enum EraseMode {
		CHIP_ERASE,
		SECTION_ERASE,
		ROW_ERASE,
	};

	HighLevelController(std::unique_ptr<Controller> controller, std::unique_ptr<DeviceDb> device_db)
		: controller_(std::move(controller)), device_db_(std::move(device_db)) {}

	Status ReadProgram(Program *program);
	Status WriteProgram(const Program &program, EraseMode erase_mode);

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
	Status ReadData(Controller::Section section, Datastring *data, uint32_t base_address, uint32_t target_size);

	bool device_open_ = false;
	DeviceDb::DeviceInfo device_info_;
	std::unique_ptr<Controller> controller_;
	std::unique_ptr<DeviceDb> device_db_;
};

#endif
