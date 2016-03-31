#include <cerrno>
#include <cstring>
#include <gflags/gflags.h>
#include <limits>

#include "controller.h"
#include "driver.h"
#include "sequence_generator.h"
#include "status.h"

DEFINE_bool(two_pin_programming, true, "Enable two-pin single-supply-voltage (LVP) programming.");

class IHexChecksum {
public:
	IHexChecksum &operator<<(int data) {
		checksum_ += data & 0xff;
		return *this;
	}
	int Get() {
		return (-checksum_) & 0xff;
	}
private:
	int16_t checksum_ = 0;
};

int main(int argc, char **argv) {
	google::ParseCommandLineFlags(&argc, &argv, true);

	// FIXME: set this based on the chip type.
	std::unique_ptr<SequenceGenerator> sequence_generator;
	if (FLAGS_two_pin_programming) {
		sequence_generator.reset(new KeySequenceGenerator);
	} else {
//		sequence_generator.reset(new PgmSequenceGenerator);
	}
	std::unique_ptr<Driver> driver = Driver::CreateFromFlags(std::move(sequence_generator));
	std::unique_ptr<Controller> controller(new Controller(std::move(driver)));
	auto device_db = std::make_unique<DeviceDb>();
	CHECK_OK(device_db->Load());
	HighLevelController high_level_controller(std::move(controller), std::move(device_db));

	Program program;
	CHECK_OK(high_level_controller.ReadProgram(&program));
	FILE *out = fopen("dump.hex", "w+");
	for (const auto &section : program) {
		size_t section_size = section.second.size();
		uint32_t section_offset = section.first;
		uint32_t last_address = std::numeric_limits<uint32_t>::max();
		for (size_t idx = 0; idx < section_size;) {
			uint32_t next_offset = section_offset + idx;
			if ((next_offset >> 16) != (last_address >> 16)) {
				fprintf(out, ":0200004%04X%02X\n", next_offset >> 16, (IHexChecksum() << 2 << 4 << (next_offset >> 24) << (next_offset >> 16)).Get());
			}
			uint32_t line_length = std::min<uint32_t>(32, ((next_offset + 0x10000) & 0xffff0000) - next_offset);
			if (line_length + idx > section_size) {
				line_length = section_size - idx;
			}
			IHexChecksum checksum;
			checksum << line_length << (next_offset >> 8) << next_offset;
			fprintf(out, ":%02X%04X00", line_length, next_offset & 0xffff);
			for (uint32_t i = 0; i < line_length; ++i, ++idx) {
				fprintf(out, "%02X", section.second[idx]);
				checksum << section.second[idx];
			}
			fprintf(out, "%02X\n", checksum.Get());
			last_address = next_offset;
		}
	}

/*

	CHECK_OK(controller->Open());

	uint16_t device_id;
	CHECK_OK(controller->ReadDeviceId(&device_id));
	// TODO: fetch information from the device ID database.
	printf("Device ID: %04x\n", device_id);

#if 0
	datastring program;
	for (int i = 0; i < 64; ++i) program.push_back(i);

	controller.WriteFlash(64, program);
	//controller.WriteFlash(0x7F80, program);
#else
#if 0
	controller.RowErase(0);
	controller.RowErase(64);
#else
	datastring program;
//	CHECK_OK(controller.ReadFlashMemory(0, 256, &program));
	CHECK_OK(controller->ReadFlashMemory(0, 0x8100, &program));
	FILE *out = fopen("dump.bin", "w+");
	if (!out) {
		FATAL("Could not open output file: %s", strerror(errno));
	}
	fwrite(program.data(), 1, program.size(), out);
	fclose(out);
#endif
#endif
*/
	return EXIT_SUCCESS;
}
