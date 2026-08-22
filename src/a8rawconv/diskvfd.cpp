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

void read_vfd(DiskInfo& disk, const char *path, int track_select) {
	std::vector<char> image(1680 * 1024 + 1);

	printf("Reading VFD/FLP file: %s\n", path);

	FILE *fi = fopen(path, "rb");
	if (!fi)
		fatalf("Unable to open input file: %s.\n", path);

	auto actual = fread(image.data(), 1, image.size(), fi);
	fclose(fi);

	if (actual < 0)
		fatalf("Unable to read input file: %s.\n", path);

	int sector_size = 512;
	int sectors_per_track = 9;
	int tracks = 40;
	int sides = 2;

	switch (actual) {
		case 160 * 1024:	// 160K: 40 tracks, 1 side, 8 sectors
			sectors_per_track = 8;
			tracks = 40;
			sides = 1;
			break;

		case 180 * 1024:	// 180K: 40 tracks, 1 side, 9 sectors
			sectors_per_track = 9;
			tracks = 40;
			sides = 1;
			break;

		case 360 * 1024:	// 360K: 40 tracks, 2 sides, 9 sectors
			sectors_per_track = 9;
			tracks = 40;
			sides = 2;
			break;

		case 720 * 1024:	// 720K: 80 tracks, 2 sides, 9 sectors
			sectors_per_track = 9;
			tracks = 80;
			sides = 2;
			break;

		case 1200 * 1024:	// 1.2M: 80 tracks, 2 sides, 15 sectors
			sectors_per_track = 15;
			tracks = 80;
			sides = 2;
			break;

		case 1440 * 1024:	// 1.44M: 80 tracks, 2 sides, 18 sectors
			sectors_per_track = 18;
			tracks = 80;
			sides = 2;
			break;

		case 1680 * 1024:	// 1.68M: 80 tracks, 2 sides, 21 sectors
			sectors_per_track = 21;
			tracks = 80;
			sides = 2;
			break;

		default:
			fatalf("Unsupported PC disk geometry: %uK%s. Supported sizes: 160K, 180K, 360K, 720K, 1.2M, 1.44M, 1.68M.\n"
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

	for (char& c : image)
		c = ~c;

	const char *data = image.data();

	for(int track = 0; track < tracks; ++track) {
		if (track_select >= 0 && track != track_select)
			continue;

		for(int side = 0; side < sides; ++side) {
			TrackInfo& track_info = disk.mPhysTracks[side][track * track_step];

			track_info.mSectors.reserve(sectors_per_track);
			for(int secidx = 0; secidx < sectors_per_track; ++secidx) {
				track_info.mSectors.emplace_back();
				auto& sec = track_info.mSectors.back();

				memset(sec.mData, 0, sizeof sec.mData);
				memcpy(sec.mData, data, sector_size);
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

void write_vfd(const char *path, DiskInfo& disk, int track) {
	int sector_size = 512;
	int sectors_per_track = 9;
	int sides = disk.mSideCount;
	int tracks = disk.mTrackStep > 1 ? 40 : 80;

	// check sectors on track 0, sector 0
	int maxsec = 0;

	for(const auto& sec : disk.mPhysTracks[0][track >= 0 ? track * disk.mTrackStep : 0].mSectors) {
		maxsec = std::max<int>(maxsec, sec.mIndex);
	}

	if (maxsec >= 21)
		sectors_per_track = 21;
	else if (maxsec >= 18)
		sectors_per_track = 18;
	else if (maxsec >= 15)
		sectors_per_track = 15;
	else if (maxsec >= 9)
		sectors_per_track = 9;
	else
		sectors_per_track = 8;

	printf("Writing %uK FLP/VFD file: %s\n", (sector_size * sectors_per_track * sides * tracks) >> 10, path);

	FILE *fo = fopen(path, "wb");
	if (!fo)
		fatalf("Unable to open output file: %s.\n", path);

	// write tracks
	char secbuf[512];

	std::vector<const SectorInfo *> ordered_sectors;

	for(int i=0; i<tracks; ++i) {
		for(int side=0; side<sides; ++side) {
			TrackInfo& track_info = disk.mPhysTracks[side][i * disk.mTrackStep];

			// sort and sift out usable sectors from track
			std::vector<SectorInfo *> secptrs;
			sift_sectors(track_info, i, secptrs);

			// iterate over sectors
			const SectorInfo *secptrs2[21] = { 0 };

			for(auto it = secptrs.begin(), itEnd = secptrs.end();
				it != itEnd;
				++it)
			{
				const SectorInfo *secptr = *it;

				if (secptr->mIndex >= 1 && secptr->mIndex <= (int)sectors_per_track) {
					if (secptrs2[secptr->mIndex - 1])
						printf("WARNING: Variable sectors not supported by FLP/VFD format. Discarding duplicate physical sector for track %d, sector %d.\n", i, secptr->mIndex);
					else
						secptrs2[secptr->mIndex - 1] = secptr;
				}
			};

			// queue sectors for write in ATR order
			for(int j=0; j<sectors_per_track; ++j) {
				const SectorInfo *sec = secptrs2[j];

				ordered_sectors.push_back(sec);

				if (!sec) {
					if (track < 0 || track == i)
						printf("WARNING: Missing sectors not supported by FLP/VFD format. Writing out null data for track %d.%d, sector %d.\n", i, side, j+1);
				} else {
					if (sec->mSectorSize != sector_size)
						printf("WARNING: Variable sector size not supported by FLP/VFD format. Writing out truncated data for track %d, sector %d.\n", i, j+1);
					else if (sec->mRecordedCRC != sec->mComputedCRC)
						printf("WARNING: CRC error encoding not supported by FLP/VFD format. Ignoring CRC error for track %d, sector %d.\n", i, j+1);
					else if (sec->mAddressMark != 0xFB)
						printf("WARNING: Deleted sector encoding not supported by FLP/VFD format. Ignoring error for track %d, sector %d.\n", i, j+1);
					else if (sec->mWeakOffset >= 0)
						printf("WARNING: Weak sector encoding not supported by FLP/VFD format. Ignoring error for track %d, sector %d.\n", i, j+1);
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
			for (char& c : secbuf)
				c = ~c;
		}

		fwrite(secbuf, sector_size, 1, fo);

		++vsec;
	}

	fclose(fo);
}
