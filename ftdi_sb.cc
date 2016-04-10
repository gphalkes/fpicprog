#include "ftdi_sb.h"

#include <gflags/gflags.h>

#include "status.h"
#include "strings.h"

DEFINE_string(ftdi_nMCLR, "TxD", "Pin to use for inverted MCLR.");
DEFINE_string(ftdi_PGC, "DTR", "Pin to use for PGC");
DEFINE_string(ftdi_PGD, "RxD", "Pin to use for PGD");
DEFINE_string(ftdi_PGM, "CTS", "Pin to use for PGM");
DEFINE_int32(ftdi_vendor_id, 0, "Vendor ID of the device to open. Defaults to FTDI vendor ID.");
DEFINE_int32(ftdi_product_id, 0, "Product ID of the device to open. Defaults to FT232 product ID.");
DEFINE_string(ftdi_description, "", "Product description to select which FTDI device to use.");
DEFINE_string(ftdi_serial, "", "Serial number to select which FTDI device to use.");

FtdiSbDriver::Pin FtdiSbDriver::pins_[] = {
    {"TxD", 0}, {"RxD", 1}, {"RTS", 2}, {"CTS", 3}, {"DTR", 4}, {"DSR", 5}, {"DCD", 6}, {"RI", 7},
};

Status FtdiSbDriver::Open() {
  if (open_) return Status(INIT_FAILED, "Device already open");
  int init_result;
  if ((init_result = ftdi_init(&ftdic_)) < 0) {
    return Status(Code::INIT_FAILED, strings::Cat("Couldn't initialize ftdi_context struct: ",
                                                  ftdi_get_error_string(&ftdic_)));
  }
  // TODO: allow more specification of which device to open.
  if (ftdi_usb_open_desc(&ftdic_, FLAGS_ftdi_vendor_id == 0 ? 0x0403 : FLAGS_ftdi_vendor_id,
                         FLAGS_ftdi_product_id == 0 ? 0x6001 : FLAGS_ftdi_product_id,
                         FLAGS_ftdi_description.empty() ? nullptr : FLAGS_ftdi_description.c_str(),
                         FLAGS_ftdi_serial.empty() ? nullptr : FLAGS_ftdi_serial.c_str()) < 0) {
    return Status(Code::INIT_FAILED,
                  strings::Cat("Couldn't open FTDI device: ", ftdi_get_error_string(&ftdic_)));
  }
  if (ftdi_set_baudrate(&ftdic_, 100000)) {
    AutoClosureRunner deinit([this] { ftdi_deinit(&ftdic_); });
    return Status(Code::INIT_FAILED,
                  strings::Cat("Couldn't set baud rate: ", ftdi_get_error_string(&ftdic_)));
  }
  if (ftdi_usb_purge_buffers(&ftdic_) < 0) {
    AutoClosureRunner deinit([this] { ftdi_deinit(&ftdic_); });
    return Status(Code::INIT_FAILED,
                  strings::Cat("Could not purge USB buffers: ", ftdi_get_error_string(&ftdic_)));
  }
  memset(translate_pins_, 0, sizeof(translate_pins_));
  translate_pins_[nMCLR] = FLAGS_ftdi_nMCLR == "NC" ? 0 : PinNameToValue(FLAGS_ftdi_nMCLR);
  translate_pins_[PGC] = PinNameToValue(FLAGS_ftdi_PGC);
  translate_pins_[PGD] = PinNameToValue(FLAGS_ftdi_PGD);
  translate_pins_[PGM] = FLAGS_ftdi_PGM == "NC" ? 0 : PinNameToValue(FLAGS_ftdi_PGM);
  for (int i = 0; i < 16; ++i) {
    for (int j : {nMCLR, PGC, PGD, PGM}) {
      if (i & j) {
        translate_pins_[i] |= translate_pins_[j];
      }
    }
  }
  if (ftdi_set_bitmode(&ftdic_, translate_pins_[nMCLR | PGC | PGD], BITMODE_SYNCBB) < 0) {
    AutoClosureRunner deinit([this] { ftdi_deinit(&ftdic_); });
    return Status(INIT_FAILED,
                  strings::Cat("Couldn't set bitbang mode: ", ftdi_get_error_string(&ftdic_)));
  }
  open_ = true;
  return Status::OK;
}

void FtdiSbDriver::Close() {
  if (!open_) return;
  SetPins(0).IgnoreResult();
  FlushOutput().IgnoreResult();
  Sleep(MilliSeconds(100));
  // Turn all pins into inputs
  ftdi_set_bitmode(&ftdic_, 0, BITMODE_SYNCBB);
  ftdi_deinit(&ftdic_);
  open_ = false;
}

Status FtdiSbDriver::List(std::vector<std::string> *list) const {
  int init_result;
  ftdi_context ftdic;
  if ((init_result = ftdi_init(&ftdic)) < 0) {
    return Status(Code::INIT_FAILED, strings::Cat("Couldn't initialize ftdi_context struct: ",
                                                  ftdi_get_error_string(&ftdic)));
  }
  ftdi_device_list *device_list = nullptr;
  int num_devices;
  int vendor_id = 0x0403;
  for (int product_id : {0x6001, 0x6010, 0x6011, 0x6014, 0x6015}) {
    if ((num_devices = ftdi_usb_find_all(&ftdic, &device_list, vendor_id, product_id)) < 0) {
      return Status(Code::INIT_FAILED,
                    strings::Cat("Could not list devices: ", ftdi_get_error_string(&ftdic)));
    }
    for (ftdi_device_list *ptr = device_list; ptr != nullptr; ptr = ptr->next) {
      char description[1024];
      char serial[1024];
      if (ftdi_usb_get_strings(&ftdic, ptr->dev, nullptr, 0, description, sizeof(description),
                               serial, sizeof(serial)) < 0) {
        return Status(Code::INIT_FAILED, strings::Cat("Error getting device strings: ",
                                                      ftdi_get_error_string(&ftdic)));
      }
      list->push_back(strings::Cat("Vendor ID: 0x", HexUint16(vendor_id), "\nProduct ID: 0x",
                                   HexUint16(product_id), "\nDescription: ", description,
                                   "\nSerial: ", serial, "\n"));
    }
    ftdi_list_free(&device_list);
  }
  return Status::OK;
}

Status FtdiSbDriver::SetPins(uint8_t pins) {
  output_buffer_ += translate_pins_[pins];
  return Status::OK;
}

Status FtdiSbDriver::FlushOutput() {
  Status status;
  while (!output_buffer_.empty()) {
    // In theory we should be able to push this up to 128. However, reading becomes unreliable when
    // we write more than 64 bytes at a time.
    int size = std::min<int>(64, output_buffer_.size());
    if (ftdi_write_data(&ftdic_, const_cast<uint8_t *>(output_buffer_.data()), size) != size) {
      return Status(Code::USB_WRITE_ERROR,
                    strings::Cat("Write failed: ", ftdi_get_error_string(&ftdic_)));
    }
    output_buffer_.erase(0, size);
    status.Update(DrainInput(size));
  }
  return status;
}

uint8_t ReverseBits(uint8_t data) {
  uint8_t result = 0;
  for (int i = 0; i < 8; ++i) {
    if (data & (1 << i)) {
      result |= (1 << (7 - i));
    }
  }
  return result;
}

Status FtdiSbDriver::ReadWithSequence(const Datastring &sequence, int bit_offset, int bit_count,
                                      uint32_t count, Datastring16 *result) {
  result->clear();
  RETURN_IF_ERROR(FlushOutput());
  received_data_.clear();
  received_data_.push_back(0);
  received_data_bit_offset_ = 0;
  write_mode_ = false;
  AutoClosureRunner reset_write_mode([this] { write_mode_ = true; });
  for (uint32_t i = 0; i < count; ++i) {
    RETURN_IF_ERROR(WriteDatastring(sequence));
  }
  RETURN_IF_ERROR(FlushOutput());

  BitStreamWrapper bit_stream(&received_data_);
  for (uint32_t i = 0; i < count; ++i) {
    uint16_t datum = 0;
    for (int j = 0; j < bit_count; ++j) {
      datum |= bit_stream.GetBit(i * sequence.size() + (bit_offset + j) * 2 + 1) << j;
    }
    *result += datum;
  }
  return Status::OK;
}

uint8_t FtdiSbDriver::PinNameToValue(const std::string &name) {
  for (const Pin &pin : pins_) {
    if (name == pin.name) {
      return (1 << pin.number);
    }
  }
  FATAL("No pin named %s available.", name.c_str());
}

Status FtdiSbDriver::DrainInput(int expected_size) {
  uint8_t buffer[128];
  int total_bytes_read = 0;
  int bytes_read;
  int retries = 0;
  while (total_bytes_read < expected_size &&
         (bytes_read = ftdi_read_data(
              &ftdic_, buffer, std::min<int>(expected_size - total_bytes_read, sizeof(buffer)))) >=
             0) {
    if (bytes_read == 0) {
      if (++retries < 10) {
        continue;
      }
      break;
    }
    if (!write_mode_) {
      for (int i = 0; i < bytes_read; ++i, ++received_data_bit_offset_) {
        if (received_data_bit_offset_ == 8) {
          received_data_.push_back(0);
          received_data_bit_offset_ = 0;
        }
        int bit = (buffer[i] & translate_pins_[PGD]) ? 1 : 0;
        bit <<= received_data_bit_offset_;
        received_data_.back() |= bit;
      }
    }
    total_bytes_read += bytes_read;
  }
  // In read mode it is vital we receive all the bytes. In write mode, we don't really care.
  // It appears to be a problem with read bytes not being reported to the USB host, rather
  // than a complete loss of data.
  if (total_bytes_read < expected_size && !write_mode_) {
    return Status(Code::SYNC_LOST,
                  strings::Cat("Did not receive the expected number of bytes (", total_bytes_read,
                               " instead of ", expected_size, ")"));
  }
  return Status::OK;
}
