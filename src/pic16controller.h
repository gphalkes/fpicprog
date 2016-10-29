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
#ifndef PIC16CONTROLLER_H_
#define PIC16CONTROLLER_H_

#include <map>
#include <memory>
#include <vector>

#include "controller.h"
#include "device_db.h"
#include "driver.h"
#include "program.h"

class Pic16ControllerBase : public Controller {
 public:
  Pic16ControllerBase(std::unique_ptr<Driver> driver,
                      std::unique_ptr<Pic16SequenceGenerator> sequence_generator)
      : driver_(std::move(driver)), sequence_generator_(std::move(sequence_generator)) {}

  Status Open() override;
  void Close() override;
  Status ReadDeviceId(uint16_t *device_id, uint16_t *revision) override;
  Status Read(Section section, uint32_t start_address, uint32_t end_address,
              const DeviceInfo &device_info, Datastring *result) override;
  Status Write(Section section, uint32_t address, const Datastring &data,
               const DeviceInfo &device_info) override;
  Status ChipErase(const DeviceInfo &device_info) override;
  Status SectionErase(Section section, const DeviceInfo &device_info) override;

 protected:
  virtual Status LoadAddress(Section section, uint32_t address, const DeviceInfo &device_info) = 0;
  virtual Status ResetDevice() = 0;
  virtual Status IncrementPc(const DeviceInfo &device_info) = 0;

  Status WriteCommand(Pic16Command command, uint16_t payload);
  Status WriteCommand(Pic16Command command);
  Status ReadWithCommand(Pic16Command command, uint16_t *data);
  Status WriteTimedSequence(Pic16SequenceGenerator::TimedSequenceType type,
                            const DeviceInfo *device_info);

 private:
  std::unique_ptr<Driver> driver_;
  std::unique_ptr<Pic16SequenceGenerator> sequence_generator_;
};

class Pic16MidrangeController : public Pic16ControllerBase {
 protected:
  using Pic16ControllerBase::Pic16ControllerBase;

  Status LoadAddress(Section section, uint32_t address, const DeviceInfo &device_info) override;
  Status IncrementPc(const DeviceInfo &device_info) override;
  Status ResetDevice() override;

  uint32_t last_address_ = 0;
};

class Pic16BaselineController : public Pic16ControllerBase {
 protected:
  using Pic16ControllerBase::Pic16ControllerBase;

  Status LoadAddress(Section section, uint32_t address, const DeviceInfo &device_info) override;
  Status IncrementPc(const DeviceInfo &device_info) override;
  Status ResetDevice() override;

  static constexpr uint32_t kConfigurationAddress = std::numeric_limits<uint32_t>::max() - 1;

  uint32_t last_address_ = kConfigurationAddress;
};

#endif
