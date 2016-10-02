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
#ifndef PROGRAM_H_
#define PROGRAM_H_

#include <cstdint>
#include <cstdio>
#include <map>

#include "device_db.h"
#include "status.h"
#include "util.h"

typedef std::map<uint32_t, Datastring> Program;

Status ReadIhex(Program *program, FILE *in);
void WriteIhex(const Program &program, FILE *out);
Status MergeProgramBlocks(Program *program, const DeviceInfo &device_info);
void RemoveMissingConfigBytes(Program *program, const DeviceInfo &device_info);

#endif
