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
#ifndef FTDI_SB_H_
#define FTDI_SB_H_

#include <libftdi1/ftdi.h>

#include "driver.h"

// Class implementing the driver functionality using the Synchronous Bitbang mode available on
// several FTDI devices (FT232R(L) and FT2232).
class FtdiSbDriver : public Driver {
 public:
  ~FtdiSbDriver() override { Close(); }

  Status Open() override;
  void Close() override;
  Status List(std::vector<std::string> *list) const override;

  Status ReadWithSequence(const Datastring &sequence, const std::vector<int> &bit_offsets,
                          int bit_count, uint32_t count, Datastring16 *result,
                          bool lsb_first) override;

 protected:
  Status SetPins(uint8_t pins) override;
  Status FlushOutput() override;

 private:
  struct Pin {
    const char *name;
    int number;
  };

  static uint8_t PinNameToValue(const std::string &name);
  Status DrainInput(int expected_size);

  static Pin pins_[];

  uint8_t translate_pins_[32];
  ftdi_context ftdic_;
  bool write_mode_ = true;
  bool open_ = false;
  Datastring output_buffer_;
  Datastring received_data_;
  int received_data_bit_offset_ = 0;
};

#endif
