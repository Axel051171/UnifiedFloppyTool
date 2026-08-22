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
#include "interleave.h"

void compute_interleave(std::vector<float>& timings, bool mfm, int sector_size, int sector_count, int track, int side, InterleaveMode mode) {
	timings.clear();
	timings.resize(sector_count, -1.0f);

	// assume 8% track-to-track skew (~16.7ms).
	float t0 = 0.08f * track;
	float spacing = 0.98f / sector_count;

	// For 128-byte formats, use 9:1 interleave if SD (18spt), 13:1 interleave
	// for ED (26spt).
	// For 256-byte formats, use 15:1 interleave.
	// For 512-byte formats, use 1:1 interleave.
	int interleave = 1;

	switch(mode) {
		case kInterleaveMode_Auto:
		case kInterleaveMode_ForceAuto:
			if (sector_size == 128) {
				interleave = (sector_count + 1) / 2;
			} else if (sector_size == 256) {
				interleave = (sector_count * 15 + 17) / 18;
			} else
				t0 = 0;
			break;

		case kInterleaveMode_None:
			interleave = 1;
			t0 = 0;
			break;

		case kInterleaveMode_XF551_DD_HS:
			// 9:1 interleave with 18 DD sectors
			interleave = (sector_count + 1) / 2;
			break;
	}

	std::vector<bool> occupied(sector_count, false);
	int slot_idx = 0;
	for (int i=0; i<sector_count; ++i) {
		while(occupied[slot_idx]) {
			if (++slot_idx >= sector_count)
				slot_idx = 0;
		}

		occupied[slot_idx] = true;

		float t = t0 + spacing * (float)slot_idx;
		timings[i] = t - floorf(t);

		slot_idx += interleave;
		if (slot_idx >= sector_count)
			slot_idx -= sector_count;
	}
}

void update_disk_interleave(DiskInfo& disk, InterleaveMode mode) {
	int min_sectors_per_track = disk.mPrimarySectorsPerTrack;
	int sector_size = disk.mPrimarySectorSize;

	if (!min_sectors_per_track)
		min_sectors_per_track = 18;

	if (!sector_size)
		sector_size = 128;

	std::vector<float> sector_timings;
	std::vector<SectorInfo *> sector_ptrs;
	for(int side = 0; side < 2; ++side) {
		for(int track = 0; track < DiskInfo::kMaxPhysTracks; ++track) {
			TrackInfo& track_info = disk.mPhysTracks[side][track];

			if (track_info.mSectors.empty())
				continue;

			// use the max of the normal number of sectors and the actual number of
			// sectors in this track
			int num_secs = (int)track_info.mSectors.size();
			int sectors_per_track = std::max<int>(num_secs, min_sectors_per_track);

			// sort sectors by index, preserving relative order for any duplicates
			sector_ptrs.resize(num_secs);
			std::transform(
				track_info.mSectors.begin(),
				track_info.mSectors.end(),
				sector_ptrs.begin(),
				[](SectorInfo& si) { return &si; });

			std::stable_sort(sector_ptrs.begin(), sector_ptrs.end(),
				[](const SectorInfo *a, const SectorInfo *b) {
					return a->mIndex < b->mIndex;
				}
			);

			bool track_mfm = std::any_of(sector_ptrs.begin(), sector_ptrs.end(),
				[](const SectorInfo *a) { return a->mbMFM; });

			compute_interleave(sector_timings, track_mfm, sector_size, sectors_per_track, track, side, mode);

			// apply the timings to any sectors missing them
			for(int i=0; i<num_secs; ++i) {
				if (sector_ptrs[i]->mPosition < 0 || mode != kInterleaveMode_Auto)
					sector_ptrs[i]->mPosition = sector_timings[i];
			}
		}
	}
}
