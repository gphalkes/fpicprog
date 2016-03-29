#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <ftdi.h>
#include <gflags/gflags.h>

//FIXME: abstract the interface such that different devices could potentially be used
//FIXME: make the flushing requirements explicit, and do automatic flushing based on the buffer size (128 bytes for FT232RL)

DEFINE_string(nMCLR, "TxD", "Pin to use for inverted MCLR.");
DEFINE_string(PGC, "DTR", "Pin to use for PGC");
DEFINE_string(PGD, "RxD", "Pin to use for PGD");

enum Command {
	CORE_INST = 0,
	SHIFT_OUT_TABLAT = 2,
	TABLE_READ = 8,
	TABLE_READ_post_inc = 9,
	TABLE_READ_post_dec = 10,
	TABLE_READ_pre_inc = 11,
	TABLE_WRITE = 12,
	TABLE_WRITE_post_inc2 = 13,
	TABLE_WRITE_post_inc2_start_pgm = 14,
	TABLE_WRITE_start_pgm = 15,
};

struct Pin {
	const char *name;
	int number;
};

typedef std::basic_string<uint8_t> datastring;

static Pin pins[] = {
	{ "TxD", 0 },
	{ "RxD", 1 },
	{ "RTS", 2 },
	{ "CTS", 3 },
	{ "DTR", 4 },
	{ "DSR", 5 },
	{ "DCD", 6 },
	{ "RI", 7 },
};

static uint8_t nMCLR;
static uint8_t PGC;
static uint8_t PGD;
static ftdi_context ftdic;


#ifdef __GNUC__
static void fatal(const char *fmt, ...) __attribute__((format (printf, 1, 2))) __attribute__((noreturn));
#else
/*@noreturn@*/ static void fatal(const char *fmt, ...);
#endif
static void fatal(const char *fmt, ...) {
	va_list args;

	va_start(args, fmt);
	vfprintf(stderr, fmt, args);
	va_end(args);
	exit(EXIT_FAILURE);
}

#define FATAL(fmt, ...) fatal("%d: " fmt, __LINE__, __VA_ARGS__)


static uint8_t PinNameToValue(const std::string &name) {
	for (const Pin &pin : pins) {
		if (name == pin.name) {
			return (1 << pin.number);
		}
	}
	FATAL("No pin named %s available.", name.c_str());
}

static void DrainInput() {
	uint8_t buffer[128];
	while (ftdi_read_data(&ftdic, buffer, sizeof(buffer)) > 0) {}
}

static void SetPinsWrite() {
	if (ftdi_set_bitmode(&ftdic, nMCLR | PGC | PGD, BITMODE_SYNCBB) < 0) {
    	FATAL("Couldn't set pins in bitbang mode for writing: %s\n", ftdi_get_error_string(&ftdic));
	}
}

static void SetPinsRead() {
	DrainInput();
	if (ftdi_set_bitmode(&ftdic, nMCLR | PGC, BITMODE_SYNCBB) < 0) {
    	FATAL("Couldn't set pins in bitbang mode for reading: %s\n", ftdi_get_error_string(&ftdic));
	}
}

static datastring GenerateKeySequence() {
	uint32_t key = 0x4D434850;
	datastring result;
	for (int i = 31; i >= 0; --i) {
		bool bit_set = (key >> i) & 1;
		result.push_back(bit_set ? PGD : 0);
		result.push_back(PGC | (bit_set ? PGD : 0));
	}
	result.push_back(0);
	for (int i = 0; i < 65; ++i) {
		result.push_back(nMCLR);
	}
	return result;
}

static void WriteByte(uint8_t byte) {
	if (ftdi_write_data(&ftdic, &byte, 1) != 1) {
    	FATAL("Couldn't write data: %s\n", ftdi_get_error_string(&ftdic));
	}
}

static void WriteData(const datastring &data) {
	if (ftdi_write_data(&ftdic, const_cast<uint8_t *>(data.data()), data.size()) != static_cast<int>(data.size())) {
    	FATAL("Couldn't write data: %s\n", ftdi_get_error_string(&ftdic));
	}
	DrainInput();
}

static void SendProgramEnable() {
	// Strobe nMCLR.
	uint8_t data[] = {
			0, nMCLR, nMCLR, 0,
	};
	if (ftdi_write_data(&ftdic, data, sizeof(data)) != sizeof(data)) {
    	FATAL("Couldn't write data: %s\n", ftdi_get_error_string(&ftdic));
	}
	// Wait for at least 1ms.
	usleep(1000);
	datastring key_sequence = GenerateKeySequence();
	WriteData(key_sequence);
}

static datastring GenerateCommand(Command command) {
	datastring result;
	for (int i = 0; i < 4; ++i) {
		bool bit_set = (command >> i) & 1;
		result.push_back(nMCLR| PGC | (bit_set ? PGD : 0));
		result.push_back(nMCLR| (bit_set ? PGD : 0));
	}
	return result;
}

static datastring GeneratePayload(uint16_t payload) {
	datastring result;
	for (int i = 0; i < 16; ++i) {
		bool bit_set = (payload >> i) & 1;
		result.push_back(nMCLR| PGC | (bit_set ? PGD : 0));
		result.push_back(nMCLR| (bit_set ? PGD : 0));
	}
	return result;
}

static datastring GenerateCommand(Command command, uint16_t payload) {
	return GenerateCommand(command) + GeneratePayload(payload);
}

static void LoadAddress(uint32_t address) {
	datastring sequence;
	// MOVLW <first byte of address>
	sequence += GenerateCommand(CORE_INST, 0x0E00 | ((address >> 16) & 0xff));
	// MOVWF TBLPTRU
	sequence += GenerateCommand(CORE_INST, 0x6EF8);
	// MOVLW <second byte of address>
	sequence += GenerateCommand(CORE_INST, 0x0E00 | ((address >> 8) & 0xff));
	// MOVWF TBLPTRH
	sequence += GenerateCommand(CORE_INST, 0x6EF7);
	 // MOVLW <last byte of address>
	sequence += GenerateCommand(CORE_INST, 0x0E00 | (address & 0xff));
	// MOVWF TBLPTRL
	sequence += GenerateCommand(CORE_INST, 0x6EF6);
	WriteData(sequence);
}

static uint8_t ReadByteUsingCommand(Command command) {
	datastring sequence = GenerateCommand(command);
	// After the command, the first byte to be clocked is nothing.
	for (int i = 0; i < 8; ++i) {
		sequence.push_back(nMCLR | PGC);
		sequence.push_back(nMCLR);
	}
	WriteData(sequence);
	SetPinsRead();
	sequence.clear();
	for (int i = 0; i < 8; ++i) {
		sequence.push_back(nMCLR | PGC);
		sequence.push_back(nMCLR);
	}
	if (ftdi_write_data(&ftdic, const_cast<uint8_t *>(sequence.data()), sequence.size()) != static_cast<int>(sequence.size())) {
    	FATAL("Couldn't write data: %s\n", ftdi_get_error_string(&ftdic));
	}
	uint8_t read_data[64];
	if (ftdi_read_data(&ftdic, read_data, sizeof(read_data)) != 16) {
    	FATAL("Couldn't read data: %s\n", ftdi_get_error_string(&ftdic));
	}
	SetPinsWrite();
	uint8_t bit = 1;
	uint8_t value = 0;
	// Convert the read data into a byte. Bytes are read just before every write.
	// This means that the first byte read contains a bogus bit value. Also, we perform
	// two writes for every read. So we read the odd bits to determine the value.
	for (int i = 1; i < 16; i += 2, bit <<= 1) {
		if (read_data[i] & PGD) {
			value |= bit;
		}
	}
	return value;
}

static void BulkErase() {
	LoadAddress(0x3C0005);
	// 1100 0F 0F Write 0Fh to 3C0005h
	WriteData(GenerateCommand(TABLE_WRITE, 0x0F0F));
	LoadAddress(0x3C0004);
	datastring sequence;
	// 1100 8F 8F Write 8F8Fh TO 3C0004h to erase entire device.
	sequence += GenerateCommand(TABLE_WRITE, 0x8F8F);
	// 0000 00 00 NOP
	sequence += GenerateCommand(CORE_INST, 0x0000);
	// 0000 00 00 Hold PGD low until erase completes.
	sequence += GenerateCommand(CORE_INST);
	WriteData(sequence);
	usleep(15000 + 200);  // FIXME: this is device dependent. 15ms is for 18f45k50
	WriteData(GeneratePayload(0x0000));
}

static void RowErasePrepare() {
	datastring sequence;
	// BSF EECON1, EEPGD
	sequence += GenerateCommand(CORE_INST, 0x8EA6);
	// BCF EECON1, CFGS
	sequence += GenerateCommand(CORE_INST, 0x9CA6);
	// BSF EECON1, WREN,
	sequence += GenerateCommand(CORE_INST, 0x84A6);
	WriteData(sequence);
}

static void RowErase(uint32_t address) {
	LoadAddress(address);
	datastring sequence;
	// BSF EECON1, FREE
	sequence += GenerateCommand(CORE_INST, 0x88A6);
	// BSR EECON1, WR
	sequence += GenerateCommand(CORE_INST, 0x82A6);
	// NOP
	sequence += GenerateCommand(CORE_INST, 0x0000);
	// NOP
	sequence += GenerateCommand(CORE_INST, 0x0000);
	WriteData(sequence);
	// Loop until the WR bit in EECON1 is clear.
	uint8_t value;
	do {
		sequence.clear();
		// MOVF EECON1, W, 0
		sequence += GenerateCommand(CORE_INST, 0x50A6);
		// MOVWF TABLAT
		sequence += GenerateCommand(CORE_INST, 0x6EF5);
		// NOP
		sequence += GenerateCommand(CORE_INST, 0x0000);
		WriteData(sequence);
		// Read value from TABLAT
		value = ReadByteUsingCommand(SHIFT_OUT_TABLAT);
	} while (value & 2);
	usleep(200);
}

static void RowEraseDone() {
	// BCF EECON1, WREN
	WriteData(GenerateCommand(CORE_INST, 0x9AA6));
}

int main(int argc, char *argv[]) {
	google::ParseCommandLineFlags(&argc, &argv, true);

	nMCLR = PinNameToValue(FLAGS_nMCLR);
	PGC = PinNameToValue(FLAGS_PGC);
	PGD = PinNameToValue(FLAGS_PGD);


    int init_result;
    if ((init_result = ftdi_init(&ftdic)) < 0) {
    	FATAL("Couldn't initialize ftdi_context struct: %d\n", init_result);
    }

    if(ftdi_usb_open(&ftdic, 0x0403, 0x6001) < 0) {
    	FATAL("Couldn't open FT232 device: %s\n", ftdi_get_error_string(&ftdic));
    }
    if (ftdi_set_baudrate(&ftdic, 9600)) {
    	FATAL("Couldn't set baud rate: %s\n", ftdi_get_error_string(&ftdic));
    }

    SetPinsWrite();
    SendProgramEnable();
    LoadAddress(0x3ffffe);
    printf("Value read: %x\n", ReadByteUsingCommand(TABLE_READ_post_inc));
    printf("Value read: %x\n", ReadByteUsingCommand(TABLE_READ_post_inc));
    return EXIT_SUCCESS;
}
