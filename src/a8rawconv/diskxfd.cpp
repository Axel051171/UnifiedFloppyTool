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

void read_xfd(DiskInfo& disk, const char *path, int track_select) {
	std::vector<char> image(1440 * 256 + 1);		// +1 so we can detect oversize files

	printf("Reading XFD file: %s\n", path);

	FILE *fi = fopen(path, "rb");
	if (!fi)
		fatalf("Unable to open input file: %s.\n", path);

	auto actual = fread(image.data(), 1, image.size(), fi);

	fclose(fi);

	if (actual < 0)
		fatalf("Unable to read input file: %s.\n", path);

	int sector_size = 128;
	int sectors_per_track = 18;
	int tracks = 40;
	int sides = 1;
	bool mfm = false;

	switch (actual) {
		case 720 * 128:			// single density: 40 tracks, 18 sectors/track, 1 side, 128 bytes/sector, FM encoding
			tracks = 40;
			sides = 1;
			sectors_per_track = 18;
			sector_size = 128;
			mfm = false;
			break;

		case 1040 * 128:		// enhanced density: 40 tracks, 26 sectors/track, 1 side, 128 bytes/sector, MFM encoding
			tracks = 40;
			sides = 1;
			sectors_per_track = 26;
			sector_size = 128;
			mfm = true;
			break;

		case 720 * 256:			// double density: 40 tracks, 18 sectors/track, 1 side, 256 bytes/sector, MFM encoding
			tracks = 40;
			sides = 1;
			sectors_per_track = 18;
			sector_size = 256;
			mfm = true;
			break;

		case 1440 * 256:		// DSDD: 40 tracks, 18 sectors/track, 2 sides, 256 bytes/sector, MFM encoding
			tracks = 40;
			sides = 2;
			sectors_per_track = 18;
			sector_size = 256;
			mfm = true;
			break;

		default:
			fatalf("Unsupported XFD disk geometry: %uK%s. Supported sizes: SD (90K), ED (130K), DD (180K), DSDD (360K).\n"
				, (actual + 1023) >> 10
				, (unsigned)actual >= image.size() ? "+" : ""
			);
			break;
	}

	// read in all sectors
	const int track_step = tracks > 40 ? 1 : 2;

	disk.mTrackCount = tracks;
	disk.mSideCount = sides;
	disk.mTrackStep = track_step;
	disk.mPrimarySectorSize = sector_size;
	disk.mPrimarySectorsPerTrack = sectors_per_track;

	for(int side = 0; side < sides; ++side) {
		const char *data = image.data();

		// reverse direction for side 2
		if (side)
			data += sectors_per_track * sides * tracks * sector_size;

		for(int track = 0; track < tracks; ++track) {
			if (track_select >= 0 && track != track_select)
				continue;

			TrackInfo& track_info = disk.mPhysTracks[side][track * 2];

			track_info.mSectors.reserve(sectors_per_track);
			for(int secidx = 0; secidx < sectors_per_track; ++secidx) {
				track_info.mSectors.emplace_back();
				auto& sec = track_info.mSectors.back();

				if (side)
					data -= sector_size;

				memcpy(sec.mData, data, sector_size);

				if (!side)
					data += sector_size;

				sec.mPosition = -1.0f;
				sec.mIndex = secidx + 1;
				sec.mbMFM = true;
				sec.mAddressMark = 0xFB;
				sec.mSectorSize = sector_size;
				sec.mWeakOffset = -1;
				sec.mComputedAddressCRC = ComputeAddressCRC(track, side, sec.mIndex, sector_size, true);
				sec.mRecordedAddressCRC = sec.mComputedAddressCRC;
				sec.mComputedCRC = ComputeInvertedCRC(sec.mData, sector_size, ComputeCRC(&sec.mAddressMark, 1));
				sec.mRecordedCRC = sec.mComputedCRC;
			}
		}
	}
}

void write_xfd(const char *path, DiskInfo& disk, int selected_track) {
	int sector_size = 512;
	int sectors_per_track = 9;
	int sides = disk.mSideCount;
	int tracks = disk.mTrackStep > 1 ? 40 : 80;

	// check sectors on track 0, sector 0
	bool mfm = false;

	for(const auto& sec : disk.mPhysTracks[0][selected_track >= 0 ? selected_track * disk.mTrackStep : 0].mSectors) {
		sectors_per_track = std::max<int>(sectors_per_track, sec.mIndex);

		if (sec.mbMFM)
			mfm = true;

		sector_size = sec.mSectorSize;
	}

	if (sector_size == 128 && sectors_per_track == 18 && sides == 1 && !mfm) {
		// single density
	} else if (sector_size == 128 && sectors_per_track == 26 && sides == 1 && mfm) {
		// enhanced/medium density
	} else if (sector_size == 256 && sectors_per_track == 18 && mfm) {
		// double density or DSDD
	} else {
		fatalf("Unsupported geometry for XFD: %d sectors/track, %s, %s encoding.\n"
			, sectors_per_track
			, sides > 1 ? "2 sides" : "1 side"
			, mfm ? "MFM" : "FM"
		);
	}

	printf("Writing %uK XFD file: %s\n", (sector_size * sectors_per_track * sides * tracks) >> 10, path);

	FILE *fo = fopen(path, "wb");
	if (!fo)
		fatalf("Unable to open output file: %s.\n", path);

	// write tracks
	char secbuf[256];

	std::vector<const SectorInfo *> ordered_sectors;

	for(int side=0; side<sides; ++side) {
		for(int i=0; i<tracks; ++i) {
			// must reverse direction for opposite side
			int track = side ? (tracks - 1) - i : i;
			TrackInfo& track_info = disk.mPhysTracks[side][track * disk.mTrackStep];

			// sort and sift out usable sectors from track
			std::vector<SectorInfo *> secptrs;
			sift_sectors(track_info, i, secptrs);

			// iterate over sectors
			const SectorInfo *secptrs2[26] = { 0 };

			for(auto it = secptrs.begin(), itEnd = secptrs.end();
				it != itEnd;
				++it)
			{
				const SectorInfo *secptr = *it;

				if (secptr->mIndex >= 1 && secptr->mIndex <= (int)sectors_per_track) {
					if (secptrs2[secptr->mIndex - 1])
						printf("WARNING: Variable sectors not supported by XFD format. Discarding duplicate physical sector for track %d, sector %d.\n", i, secptr->mIndex);
					else
						secptrs2[secptr->mIndex - 1] = secptr;
				}
			};

			// queue sectors for write in ATR order
			for(int j=0; j<sectors_per_track; ++j) {
				const SectorInfo *sec = secptrs2[side ? (sectors_per_track - 1) - j : j];

				ordered_sectors.push_back(sec);

				if (!sec) {
					if (selected_track < 0 || selected_track == track)
						printf("WARNING: Missing sectors not supported by XFD format. Writing out null data for track %d.%d, sector %d.\n", i, side, j+1);
				} else {
					if (sec->mSectorSize != sector_size)
						printf("WARNING: Variable sector size not supported by XFD format. Writing out truncated data for track %d, sector %d.\n", i, j+1);
					else if (sec->mRecordedCRC != sec->mComputedCRC)
						printf("WARNING: CRC error encoding not supported by XFD format. Ignoring CRC error for track %d, sector %d.\n", i, j+1);
					else if (sec->mAddressMark != 0xFB)
						printf("WARNING: Deleted sector encoding not supported by XFD format. Ignoring error for track %d, sector %d.\n", i, j+1);
					else if (sec->mWeakOffset >= 0)
						printf("WARNING: Weak sector encoding not supported by XFD format. Ignoring error for track %d, sector %d.\n", i, j+1);
				}
			}
		}
	}

	int vsec = 1;
	for(const SectorInfo *sec : ordered_sectors) {
		if (!sec)
			memset(secbuf, 0, sizeof secbuf);
		else {
			memcpy(secbuf, sec->mData, sector_size);
		}

		fwrite(secbuf, sector_size, 1, fo);

		++vsec;
	}

	fclose(fo);
}
