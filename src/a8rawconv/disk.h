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

//==========================================================================
//
// Physical vs. logical tracks
// ---------------------------
// a8rawconv now has a distinction between 'physical' and 'logical' track
// numbers in order to resolve confusion between 48 tpi and 96 tpi disk
// formats. Physical track numbers are used for data structures and always
// store tracks with 96 tpi spacing -- that is, a 40 track disk uses every
// other entry in the array.
//
// Logical track numbers are the ones used by the UI and in sector headers.
// For a 40-track disk format, this is always 0-39 even when a 96 tpi drive
// is in use.

#ifndef f_DISK_H
#define f_DISK_H

struct RawTrack {
	int mPhysTrack;		// Physical track number (always in 96tpi spacing).
	int mSide;
	float mSamplesPerRev;

	int32_t mSpliceStart;
	int32_t mSpliceEnd;

	std::vector<uint32_t> mTransitions;
	std::vector<uint32_t> mIndexTimes;
};

struct RawDisk {
	enum : int { kMaxPhysTracks = 84 };

	// Physical tracks, 96 tpi density. 48 tpi formats double-step this array.
	RawTrack mPhysTracks[2][kMaxPhysTracks];

	// Logical disk geometry. A track step of 2 means that the logical tracks have 48 tpi spacing
	// and are matched to every other physical track for a 96 tpi drive.
	int mTrackCount = 40;
	int mTrackStep = 2;
	int mSideCount = 1;

	// true if this flux image was synthesized from decoded data instead of
	// sourced from a recording medium
	bool mSynthesized = false;

	RawDisk();
};

struct SectorInfo {
	uint32_t mRawStart;
	uint32_t mRawEnd;
	float mPosition;
	float mEndingPosition;

	// Sector number in physical format.
	// FM/MFM: 1..N for N sectors
	// A2GCR: 0..N-1 for N sectors, physical order (differs from DOS 3.3 and ProDOS logical order)
	int mIndex;

	// First byte offset of weak data. -1 = none
	int mWeakOffset;

	// Sector data payload size, in bytes (128, 256, 512, 1024).
	uint32_t mSectorSize;

	bool mbMFM;

	// FM/MFM: Data address mark (F8-FB), or 0 for no data field
	// A2GCR: Volume number (01-FF)
	uint8_t mAddressMark;

	uint16_t mRecordedAddressCRC;
	uint16_t mComputedAddressCRC;
	uint32_t mRecordedCRC;
	uint32_t mComputedCRC;
	uint8_t mData[1024];

	uint32_t ComputeContentHash() const;
	bool HasSameContents(const SectorInfo& other) const;
};

struct TrackInfo {
	std::vector<SectorInfo> mSectors;
	std::vector<uint8_t> mGCRData;
};

struct DiskInfo {
	static constexpr int kMaxPhysTracks = 84;

	TrackInfo mPhysTracks[2][kMaxPhysTracks];
	int mTrackCount = 40;
	int mTrackStep = 2;
	int mSideCount = 1;

	// Information to aid in matching the disk geometry. These may
	// be 0 for unknown.
	int mPrimarySectorSize = 0;
	int mPrimarySectorsPerTrack = 0;
};

void reverse_tracks(RawDisk& raw_disk);
void find_splice_points(RawDisk& raw_disk, const DiskInfo& decoded_disk);
void sift_sectors(TrackInfo& track_info, int track_num, std::vector<SectorInfo *>& secptrs);

#endif
