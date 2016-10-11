/* Copyright (C) 2016 G.P. Halkes
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License version 3, as
   published by the Free Software Foundation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/
#ifndef HIGH_LEVEL_CONTROLLER_H_
#define HIGH_LEVEL_CONTROLLER_H_

#include "controller.h"
#include "util.h"

class HighLevelController {
 public:
  HighLevelController(std::unique_ptr<Controller> controller, std::unique_ptr<DeviceDb> device_db)
      : controller_(std::move(controller)), device_db_(std::move(device_db)) {}

  void SetDevice(const std::string &device_name) { device_name_ = device_name; }

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
  uint16_t revision_ = 0;
  std::unique_ptr<Controller> controller_;
  std::unique_ptr<DeviceDb> device_db_;
  std::string device_name_;
};

#endif
