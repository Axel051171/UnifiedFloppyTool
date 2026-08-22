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

#ifndef f_ANALYZE_H
#define f_ANALYZE_H

struct RawDisk;

enum AnalysisMode : uint8_t {
	kAnalysisMode_None,
	kAnalysisMode_Atari_FM,
	kAnalysisMode_Atari_MFM,
	kAnalysisMode_PC_360K,
	kAnalysisMode_PC_1_2M,
	kAnalysisMode_PC_1_44M,
	kAnalysisMode_Amiga,
	kAnalysisMode_AppleII,
	kAnalysisMode_Mac
};

int analyze_raw(const RawDisk& raw_disk, int selected_track, AnalysisMode mode);

#endif
