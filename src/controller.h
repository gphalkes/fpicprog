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

#endif
