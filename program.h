#ifndef PROGRAM_H_
#define PROGRAM_H_

#include <cstdio>
#include <cstdint>
#include <map>

#include "device_db.h"
#include "status.h"
#include "util.h"

typedef std::map<uint32_t, Datastring> Program;

Status ReadIhex(Program *program, FILE *in);
void WriteIhex(const Program &program, FILE *out);
Status MergeProgramBlocks(Program *program, const DeviceInfo &device_info);

#endif
