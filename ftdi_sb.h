#ifndef FTDI_SB_H_
#define FTDI_SB_H_

#include <ftdi.h>

#include "driver.h"

// Class implementing the driver functionality using the Synchronous Bitbang mode available on
// several FTDI devices (FT232R(L) and FT2232).
class FtdiSbDriver : public Driver {
 public:
  ~FtdiSbDriver() override { Close(); }

  Status Open() override;
  void Close() override;
  Status List(std::vector<std::string> *list) const override;

  Status ReadWithSequence(const Datastring &sequence, int bit_offset, int bit_count, uint32_t count,
                          Datastring16 *result) override;

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

  uint8_t translate_pins_[16];
  ftdi_context ftdic_;
  bool write_mode_ = true;
  bool open_ = false;
  Datastring output_buffer_;
  Datastring received_data_;
  int received_data_bit_offset_ = 0;
};

#endif
