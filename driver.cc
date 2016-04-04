#include "driver.h"

#include <cstring>
#include <ftdi.h>
#include <gflags/gflags.h>

#include "strings.h"

DEFINE_string(driver, "FT232R", "Driver to use for programming.");
DEFINE_string(nMCLR, "TxD", "Pin to use for inverted MCLR.");
DEFINE_string(PGC, "DTR", "Pin to use for PGC");
DEFINE_string(PGD, "RxD", "Pin to use for PGD");
DEFINE_string(PGM, "CTS", "Pin to use for PGM");

Status Driver::WriteTimedSequence(const TimedSequence &sequence) {
	for (const auto &step : sequence) {
		RETURN_IF_ERROR(WriteDatastring(step.data));
		RETURN_IF_ERROR(FlushOutput());
		Sleep(step.sleep);
	}
	return Status::OK;
}

Status Driver::WriteDatastring(const datastring &data) {
	for (const uint8_t pins : data) {
		RETURN_IF_ERROR(SetPins(pins));
	}
	return Status::OK;
}

// ======================= FT232RDriver ===========================
class FT232RDriver : public Driver {
public:
	~FT232RDriver() override {
		Close();
	}

	Status Open() override;
	void Close() override;
	Status ReadWithSequence(const datastring &sequence, int bit_offset, int bit_count, uint32_t count, datastring *result) override;

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
	datastring output_buffer_;
	datastring received_data_;
	int received_data_bit_offset_ = 0;
};

FT232RDriver::Pin FT232RDriver::pins_[] = {
	{ "TxD", 0 },
	{ "RxD", 1 },
	{ "RTS", 2 },
	{ "CTS", 3 },
	{ "DTR", 4 },
	{ "DSR", 5 },
	{ "DCD", 6 },
	{ "RI", 7 },
};

Status FT232RDriver::Open() {
	if (open_) return Status(INIT_FAILED, "Device already open");
	int init_result;
	if ((init_result = ftdi_init(&ftdic_)) < 0) {
		return Status(Code::INIT_FAILED, strings::Cat("Couldn't initialize ftdi_context struct: ", ftdi_get_error_string(&ftdic_)));
	}
	// TODO: allow more specification of which device to open.
	if(ftdi_usb_open(&ftdic_, 0x0403, 0x6001) < 0) {
		return Status(Code::INIT_FAILED, strings::Cat("Couldn't open FT232 device: ", ftdi_get_error_string(&ftdic_)));
	}
	if (ftdi_set_baudrate(&ftdic_, 100000)) {
		AutoClosureRunner deinit([this] {ftdi_deinit(&ftdic_);});
		return Status(Code::INIT_FAILED, strings::Cat("Couldn't set baud rate: ", ftdi_get_error_string(&ftdic_)));
	}
	if (ftdi_usb_purge_buffers(&ftdic_) < 0) {
		AutoClosureRunner deinit([this] {ftdi_deinit(&ftdic_);});
		return Status(Code::INIT_FAILED, strings::Cat("Could not purge USB buffers: ", ftdi_get_error_string(&ftdic_)));
	}
	memset(translate_pins_, 0, sizeof(translate_pins_));
	translate_pins_[nMCLR] = PinNameToValue(FLAGS_nMCLR);
	translate_pins_[PGC] = PinNameToValue(FLAGS_PGC);
	translate_pins_[PGD] = PinNameToValue(FLAGS_PGD);
	translate_pins_[PGM] = FLAGS_PGM == "NC" ? 0 : PinNameToValue(FLAGS_PGM);
	for (int i = 0; i < 16; ++i) {
		for (int j : {nMCLR, PGC, PGD, PGM}) {
			if (i & j) {
				translate_pins_[i] |= translate_pins_[j];
			}
		}
	}
	if (ftdi_set_bitmode(&ftdic_, translate_pins_[nMCLR | PGC | PGD], BITMODE_SYNCBB) < 0) {
		AutoClosureRunner deinit([this] {ftdi_deinit(&ftdic_);});
		return Status(INIT_FAILED, strings::Cat("Couldn't set bitbang mode: ", ftdi_get_error_string(&ftdic_)));
	}
	open_ = true;
	return Status::OK;
}

void FT232RDriver::Close() {
	if (!open_) return;
	SetPins(0).IgnoreResult();
	Sleep(MilliSeconds(100));
	// Turn all pins into inputs
	ftdi_set_bitmode(&ftdic_, 0, BITMODE_SYNCBB);
	ftdi_deinit(&ftdic_);
	open_ = false;
}

Status FT232RDriver::SetPins(uint8_t pins) {
  output_buffer_ += translate_pins_[pins];
  return Status::OK;
}

Status FT232RDriver::FlushOutput() {
	Status status;
	while (!output_buffer_.empty()) {
		// In theory we should be able to push this up to 128. However, reading becomes unreliable when
		// we write more than 64 bytes at a time.
		// TODO: test if writing does work reliably at 128. If so, then we can speed up writing, as the
		// speed is mostly determined by this value. Also, maybe resetting the device occasionally may be
		// faster than trying to get the device to work reliably
		int size = std::min<int>(64, output_buffer_.size());
		if (ftdi_write_data(&ftdic_, const_cast<uint8_t *>(output_buffer_.data()), size) != size) {
			// FIXME: use strings::Cat to complete the error message
			return Status(Code::USB_WRITE_ERROR, "Write failed");
			//FATAL("Wrote fewer bytes than requested: %s\n", ftdi_get_error_string(&ftdic_));
		}
		output_buffer_.erase(0, size);
		status.Update(DrainInput(size));
	}
	return status;
}

uint8_t ReverseBits(uint8_t data) {
	uint8_t result = 0;
	for (int i = 0; i < 8; ++i) {
		if (data & (1<<i)) {
			result |= (1<<(7-i));
		}
	}
	return result;
}

Status FT232RDriver::ReadWithSequence(const datastring &sequence, int bit_offset, int bit_count, uint32_t count, datastring *result) {
	//FIXME: start using bit_offset and bit_count!
	result->clear();
	RETURN_IF_ERROR(FlushOutput());
	received_data_.clear();
	received_data_.push_back(0);
	received_data_bit_offset_ = 0;
	write_mode_ = false;
	for (uint32_t i = 0; i < count; ++i) {
		RETURN_IF_ERROR(WriteDatastring(sequence));
	}
	RETURN_IF_ERROR(FlushOutput());
	write_mode_ = true;

	for (uint32_t i = 0; i < count; ++i) {
		uint16_t bits = received_data_[i * 5 + 3] | (static_cast<uint16_t>(received_data_[i * 5 + 4]) << 8);
		uint8_t byte = 0;
		for (int j = 0; j < 8; ++j) {
			if (bits & (1 << (j * 2 + 1))) {
				byte |= 1 << j;
			}
		}
		*result += byte;
	}
	return Status::OK;
}

uint8_t FT232RDriver::PinNameToValue(const std::string &name) {
	for (const Pin &pin : pins_) {
		if (name == pin.name) {
			return (1 << pin.number);
		}
	}
	FATAL("No pin named %s available.", name.c_str());
}


Status FT232RDriver::DrainInput(int expected_size) {
	uint8_t buffer[128];
	int total_bytes_read = 0;
	int bytes_read;
	int retries = 0;
	while (total_bytes_read < expected_size &&
			(bytes_read = ftdi_read_data(&ftdic_, buffer, std::min<int>(expected_size - total_bytes_read, sizeof(buffer)))) >= 0) {
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
		return Status(Code::SYNC_LOST, strings::Cat("Did not receive the expected number of bytes (",
				total_bytes_read, " instead of ", expected_size, ")"));
	}
	return Status::OK;
}

std::unique_ptr<Driver> Driver::CreateFromFlags() {
	if (FLAGS_driver == "FT232R") {
		return std::make_unique<FT232RDriver>();
	}
	FATAL("Unknown driver: %s\n", FLAGS_driver.c_str());
}
