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
#include "compensation.h"

void postcomp_track_mac800k(RawTrack& track) {
	size_t n = track.mTransitions.size();
	if (n < 3)
		return;

	uint32_t *transitions = track.mTransitions.data();
	uint32_t t0 = transitions[0];
	uint32_t t1 = transitions[1];

	// We begin applying correction at approximately 1/45000th of a rotation. For reference, standard
	// 2us MFM encodings have a minimum spacing of 4us at 300 RPM, or 1/50000th of a rotation. Tracks 0-15
	// on a Mac 800K disk, OTOH, have a minimum spacing of 2us at 394 RPM, or 1/76142th of a rotation.
	// The higher density makes them more prone to peak shift effects. We try to combat that here by
	// pushing transitions apart a bit when they are below the threshold, with increasing effect as
	// they are closer together.
	//
	// The latter part of the expression is a correction term for smaller track circumferences on inner
	// tracks. The simple linear mappings we're using here would overcorrect on inner tracks, so it's
	// clamped after the third zone.
	//
	int thresh = (int)(0.5 + track.mSamplesPerRev / 30000.0 * (float)(160 + std::min<int>(track.mPhysTrack, 47)) / 240.0f);

	for(size_t i=2; i<n; ++i) {
		// shift in next transition time
		uint32_t t2 = transitions[i];

		// compute deltas between each pair
		int32_t t01 = (int32_t)(t1 - t0);
		int32_t t12 = (int32_t)(t2 - t1);

		// compute anti peak shift delta for any pair that is narrower than the threshold
		int32_t delta1 = std::max<int32_t>(0, thresh - t01);
		int32_t delta2 = std::max<int32_t>(0, thresh - t12);

		// apply correction shift, limited to no more than half the distance rounded down
		transitions[i-1] = t1 + std::min(std::max<int32_t>(((delta2 - delta1) * 5) / 12, -t01 / 2), t12 / 2);

		t0 = t1;
		t1 = t2;
	}
}

void postcomp_disk(RawDisk& raw_disk, PostCompensationMode mode) {
	if (mode == kPostComp_None || mode == kPostComp_Auto)
		return;

	for(auto& tracks : raw_disk.mPhysTracks) {
		for(auto& track : tracks) {
			switch(mode) {
				case kPostComp_Mac800K:
					postcomp_track_mac800k(track);
					break;
			}
		}
	}
}
