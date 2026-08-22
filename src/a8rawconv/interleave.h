// a8rawconv - A8 raw disk conversion utility
// Copyright (C) 2014-2020 Avery Lee
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.

#ifndef f_INTERLEAVE_H
#define f_INTERLEAVE_H

#include <vector>

struct DiskInfo;

enum InterleaveMode : uint8_t {
	kInterleaveMode_Auto,
	kInterleaveMode_ForceAuto,
	kInterleaveMode_None,
	kInterleaveMode_XF551_DD_HS
};

void compute_interleave(std::vector<float>& timings, bool mfm, int sector_size, int sector_count, int track, int side, InterleaveMode mode);
void update_disk_interleave(DiskInfo& disk, InterleaveMode mode);

#endif
