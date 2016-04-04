#include "program.h"

#include <limits>

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

void WriteIhex(const Program &program, FILE *out) {
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
}
