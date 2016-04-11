#ifndef HIGH_LEVEL_CONTROLLER_H_
#define HIGH_LEVEL_CONTROLLER_H_

#include "controller.h"
#include "util.h"

class HighLevelController {
 public:
  HighLevelController(std::unique_ptr<Controller> controller, std::unique_ptr<DeviceDb> device_db,
                      bool lvp)
      : controller_(std::move(controller)), device_db_(std::move(device_db)), lvp_(lvp) {}

  Status ReadProgram(const std::vector<Section> &sections, Program *program);
  Status WriteProgram(const std::vector<Section> &sections, const Program &program,
                      EraseMode erase_mode);
  Status ChipErase();
  Status SectionErase(const std::vector<Section> &sections);
  Status Identify();

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
  Status ReadData(Section section, Datastring *data, uint32_t base_address, uint32_t target_size);
  Status VerifyData(Section, const Datastring &data, uint32_t base_address);

  bool device_open_ = false;
  DeviceInfo device_info_;
  std::unique_ptr<Controller> controller_;
  std::unique_ptr<DeviceDb> device_db_;
  bool lvp_;
};

#endif
