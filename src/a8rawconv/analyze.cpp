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

#include "stdafx.h"
#include "analyze.h"
#include "disk.h"

int analyze_raw(const RawDisk& raw_disk, int selected_track, AnalysisMode mode) {
	std::string s;

	const float secs_per_rot_288rpm = 60.0f / 288.0f;
	const float secs_per_rot_300rpm = 60.0f / 300.0f;
	const float secs_per_rot_360rpm = 60.0f / 360.0f;
	const float cells_per_sec_1us = 1000000.0f / 1.0f;
	const float cells_per_sec_2us = 1000000.0f / 2.0f;
	const float cells_per_sec_4us = 1000000.0f / 4.0f;
	float cells_per_rotation = 0;
	float cells_per_rotation_table[RawDisk::kMaxPhysTracks];
	int bitcell_label_size = 0;

	switch(mode) {
		case kAnalysisMode_Atari_FM:
			cells_per_rotation = secs_per_rot_288rpm * cells_per_sec_4us;
			bitcell_label_size = 4;
			break;

		case kAnalysisMode_Atari_MFM:
			cells_per_rotation = secs_per_rot_288rpm * cells_per_sec_2us;
			bitcell_label_size = 2;
			break;

		case kAnalysisMode_PC_360K:
			cells_per_rotation = secs_per_rot_300rpm * cells_per_sec_2us;
			bitcell_label_size = 2;
			break;

		case kAnalysisMode_PC_1_2M:
			cells_per_rotation = secs_per_rot_360rpm * cells_per_sec_1us;
			bitcell_label_size = 1;
			break;

		case kAnalysisMode_PC_1_44M:
			cells_per_rotation = secs_per_rot_300rpm * cells_per_sec_1us;
			bitcell_label_size = 1;
			break;

		case kAnalysisMode_Amiga:
			cells_per_rotation = secs_per_rot_300rpm * cells_per_sec_2us;
			bitcell_label_size = 2;
			break;

		case kAnalysisMode_AppleII:
			cells_per_rotation = secs_per_rot_300rpm * cells_per_sec_4us;
			bitcell_label_size = 4;
			break;

		case kAnalysisMode_Mac:
			for(int i=0; i<16; ++i)
				cells_per_rotation_table[i] = (60.0f / 394.0f) * cells_per_sec_2us;

			for(int i=16; i<32; ++i)
				cells_per_rotation_table[i] = (60.0f / 429.0f) * cells_per_sec_2us;

			for(int i=32; i<48; ++i)
				cells_per_rotation_table[i] = (60.0f / 472.0f) * cells_per_sec_2us;

			for(int i=48; i<64; ++i)
				cells_per_rotation_table[i] = (60.0f / 525.0f) * cells_per_sec_2us;

			for(int i=64; i<RawDisk::kMaxPhysTracks; ++i)
				cells_per_rotation_table[i] = (60.0f / 590.0f) * cells_per_sec_2us;

			bitcell_label_size = 2;
			break;

		case kAnalysisMode_None:
		default:
			A8RC_RT_ASSERT(false);
	}

	if (cells_per_rotation > 0) {
		for(auto& v : cells_per_rotation_table)
			v = cells_per_rotation;
	}

	for(int track = 0; track < raw_disk.mTrackCount; ++track) {
		if (selected_track >= 0 && track != selected_track)
			continue;

		for(int side = 0; side < raw_disk.mSideCount; ++side) {
			if (raw_disk.mSideCount > 1)
				printf("\nTrack %d.%d:\n", track, side);
			else
				printf("\nTrack %d:\n", track);

			int phys_track = track * raw_disk.mTrackStep;
			const RawTrack& track_info = raw_disk.mPhysTracks[side][phys_track];

			const int max_bin = 90;
			const int bin_count = max_bin + 1;
			int bins[bin_count] {};

			float cells_per_sample = cells_per_rotation_table[phys_track] / track_info.mSamplesPerRev;
			float bins_per_sample = cells_per_sample * (float)max_bin / 4.0f;

			const auto& transitions = track_info.mTransitions;

			size_t n = transitions.size();
			for(size_t i = 1; i < n; ++i) {
				float fbin = (float)(transitions[i] - transitions[i-1]) * bins_per_sample;
				int bin = fbin >= (float)max_bin ? max_bin : fbin <= 0 ? 0 : (int)(fbin + 0.5f);

				++bins[bin];
			}

			int maxcnt = 1;
			for(int bin : bins)
				maxcnt = std::max(maxcnt, bin);

			const int height = 30;
			int heights[bin_count] {};
			for(int i=0; i<bin_count; ++i)
				heights[i] = (int)(0.5f + (float)bins[i] * (float)height * 2.0 / (float)maxcnt);

			s.resize(bin_count);
			for(int i=height*2; i>0; i-=2) {
				for(int j=0; j<bin_count; ++j)
					s[j] = heights[j] == i+1 ? '.' : heights[j] >= i ? '|' : ' ';

				printf("%s\n", s.c_str());
			}

			std::fill(s.begin(), s.end(), '-');
			printf("%s\n", s.c_str());

			std::fill(s.begin(), s.end(), ' ');
			s[max_bin*1/8] = '.';
			s[max_bin*3/8] = '.';
			s[max_bin*5/8] = '.';
			s[max_bin*7/8] = '.';

			switch(bitcell_label_size) {
				case 1:
					s.replace(max_bin*2/8-1, 3, "1us");
					s.replace(max_bin*4/8-1, 3, "2us");
					s.replace(max_bin*6/8-1, 3, "3us");
					break;

				case 2:
					s.replace(max_bin*2/8-1, 3, "2us");
					s.replace(max_bin*4/8-1, 3, "4us");
					s.replace(max_bin*6/8-1, 3, "6us");
					break;

				case 4:
					s.replace(max_bin*2/8-1, 3, "4us");
					s.replace(max_bin*4/8-1, 3, "8us");
					s.replace(max_bin*6/8-1, 4, "12us");
					break;
			}
			printf("%s\n", s.c_str());
		}
	}

	return 0;
}
