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
#ifndef CONTROLLER_H_
#define CONTROLLER_H_

#include <map>
#include <memory>
#include <vector>

#include "device_db.h"
#include "driver.h"
#include "program.h"

class Controller {
 public:
  virtual ~Controller() = default;

  virtual Status Open() = 0;
  virtual void Close() = 0;
  virtual Status ReadDeviceId(uint16_t *device_id, uint16_t *revision) = 0;
  virtual Status Read(Section section, uint32_t start_address, uint32_t end_address,
                      const DeviceInfo &device_info, Datastring *result) = 0;
  virtual Status Write(Section section, uint32_t address, const Datastring &data,
                       const DeviceInfo &device_info) = 0;
  virtual Status ChipErase(const DeviceInfo &device_info) = 0;
  virtual Status SectionErase(Section section, const DeviceInfo &device_info) = 0;
};

class Pic18Controller : public Controller {
 public:
  Pic18Controller(std::unique_ptr<Driver> driver,
                  std::unique_ptr<Pic18SequenceGenerator> sequence_generator)
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

 private:
  Status WriteCommand(Pic18Command command, uint16_t payload);
  Status ReadWithCommand(Pic18Command command, uint32_t count, Datastring *result);
  Status WriteTimedSequence(Pic18SequenceGenerator::TimedSequenceType type,
                            const DeviceInfo *device_info);
  Status LoadAddress(uint32_t address);
  Status LoadEepromAddress(uint32_t address);
  Status ExecuteBulkErase(const Datastring16 &sequence, const DeviceInfo &device_info);

  std::unique_ptr<Driver> driver_;
  std::unique_ptr<Pic18SequenceGenerator> sequence_generator_;
};

class Pic16Controller : public Controller {
 public:
  Pic16Controller(std::unique_ptr<Driver> driver,
                  std::unique_ptr<Pic16SequenceGenerator> sequence_generator,
                  const std::string &device_name)
      : driver_(std::move(driver)),
        sequence_generator_(std::move(sequence_generator)),
        device_name_(device_name) {}

  Status Open() override;
  void Close() override;
  Status ReadDeviceId(uint16_t *device_id, uint16_t *revision) override;
  Status Read(Section section, uint32_t start_address, uint32_t end_address,
              const DeviceInfo &device_info, Datastring *result) override;
  Status Write(Section section, uint32_t address, const Datastring &data,
               const DeviceInfo &device_info) override;
  Status ChipErase(const DeviceInfo &device_info) override;
  Status SectionErase(Section section, const DeviceInfo &device_info) override;

 private:
  Status WriteCommand(Pic16Command command, uint16_t payload);
  Status WriteCommand(Pic16Command command);
  Status ReadWithCommand(Pic16Command command, uint16_t *data);
  Status WriteTimedSequence(Pic16SequenceGenerator::TimedSequenceType type,
                            const DeviceInfo *device_info);
  Status LoadAddress(Section section, uint32_t address, const DeviceInfo &device_info);
  Status ResetDevice();

  std::unique_ptr<Driver> driver_;
  std::unique_ptr<Pic16SequenceGenerator> sequence_generator_;
  const std::string device_name_;
  uint32_t last_address_ = 0;
};

#endif
