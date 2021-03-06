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
#ifndef PIC24CONTROLLER_H_
#define PIC24CONTROLLER_H_

#include <map>
#include <memory>
#include <vector>

#include "controller.h"
#include "device_db.h"
#include "driver.h"
#include "program.h"

class Pic24Controller : public Controller {
 public:
  Pic24Controller(std::unique_ptr<Driver> driver,
                  std::unique_ptr<Pic24SequenceGenerator> sequence_generator)
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
  Status WriteCommand(uint32_t payload);
  Status ReadVisi(uint16_t *result);
  Status WriteTimedSequence(Pic24SequenceGenerator::TimedSequenceType type,
                            const DeviceInfo *device_info);
  // Loads TBLPAG:W6 with the address.
  Status LoadAddress(uint32_t address);
  // Resets the PC by executing a GOTO 0x0200 instruction.
  Status ResetPc();
  // Loads W7 with the address of the VISI register.
  Status LoadVisiAddress();

  // Executes an erase sequence with the given value for NVMCON.
  Status ExecuteErase(uint32_t nvmcon);
  // Waits for the WR bit of NVMCON to return to 0.
  Status WaitForWr0();

  std::unique_ptr<Driver> driver_;
  std::unique_ptr<Pic24SequenceGenerator> sequence_generator_;
};

#endif
