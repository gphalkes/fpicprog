#ifndef PROGRAM_H_
#define PROGRAM_H_

#include <cstdio>
#include <cstdint>
#include <map>

#include "util.h"

typedef std::map<uint32_t, Datastring> Program;

void WriteIhex(const Program &program, FILE *out);

#endif
