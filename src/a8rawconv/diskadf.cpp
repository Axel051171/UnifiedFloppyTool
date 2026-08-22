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

void read_adf(DiskInfo& disk, const char *path, int track_select) {
	printf("Reading ADF file: %s\n", path);

	FILE *fi = fopen(path, "rb");
	if (!fi)
		fatalf("Unable to open input file: %s.\n", path);

	// read in all sectors
	std::vector<uint8_t> sector_data(512 * 1760, 0);

	read_raw(sector_data.data(), sector_data.size(), fi, path);
	fclose(fi);

	for(uint32_t cyl = 0; cyl < 80; ++cyl) {
		for(uint32_t head = 0; head < 2; ++head) {
			if (track_select >= 0 && cyl != (uint32_t)track_select)
				continue;

			TrackInfo& track_info = disk.mPhysTracks[head][cyl];
			const uint8_t *data = sector_data.data() + 512 * 11 * (cyl * 2 + head);

			track_info.mSectors.reserve(11);
			for(uint32_t secidx = 0; secidx < 11; ++secidx) {
				track_info.mSectors.emplace_back();
				auto& sec = track_info.mSectors.back();

				memcpy(sec.mData, data, 512);
				data += 512;

				sec.mPosition = -1.0f;

				// Amiga sectors start from 0, not 1.
				sec.mIndex = secidx;
				sec.mbMFM = true;
				sec.mAddressMark = 0;
				sec.mSectorSize = 512;
				sec.mWeakOffset = -1;
				sec.mComputedAddressCRC = 0;
				sec.mRecordedAddressCRC = 0;
				sec.mComputedCRC = 0;
				sec.mRecordedCRC = 0;
			}
		}
	}

	disk.mTrackCount = 80;
	disk.mTrackStep = 1;
	disk.mSideCount = 2;
	disk.mPrimarySectorSize = 512;
	disk.mPrimarySectorsPerTrack = 11;
}

void write_adf(const char *path, DiskInfo& disk, int track) {
	const uint32_t sector_size = 512;
	const uint32_t sectors_per_track = 11;

	printf("Writing ADF file: %s\n", path);

	FILE *fo = fopen(path, "wb");
	if (!fo)
		fatalf("Unable to open output file: %s.\n", path);

	for(int i=0; i<80; ++i) {
		for(int head=0; head<2; ++head) {
			TrackInfo& track_info = disk.mPhysTracks[head][i];

			// sort and sift out usable sectors from track
			std::vector<SectorInfo *> secptrs;
			sift_sectors(track_info, i, secptrs);

			// iterate over sectors
			const SectorInfo *secptrs2[11] = { 0 };

			for(auto it = secptrs.begin(), itEnd = secptrs.end();
				it != itEnd;
				++it)
			{
				const SectorInfo *secptr = *it;

				if (secptr->mIndex >= 0 && secptr->mIndex < 11) {
					if (secptrs2[secptr->mIndex])
						printf("WARNING: Variable sectors not supported by ADF format. Discarding duplicate physical sector for cylinder %d, head %d, sector %d.\n", i, head, secptr->mIndex);
					else
						secptrs2[secptr->mIndex] = secptr;
				}
			};

			// write out sectors
			char secbuf[sector_size];

			for(uint32_t j=0; j<sectors_per_track; ++j) {
				const SectorInfo *sec = secptrs2[j];

				if (!sec) {
					if (track < 0 || track == i)
						printf("WARNING: Missing sectors not supported by ADF format. Writing out null data for cylinder %d, head %d, sector %d.\n", i, head, j+1);

					memset(secbuf, 0, sizeof secbuf);
				} else {
					memcpy(secbuf, sec->mData, sector_size);

					if (sec->mSectorSize != sector_size)
						printf("WARNING: Variable sector size not supported by ADF format. Writing out truncated data for cylinder %d, head %d, sector %d.\n", i, head, j+1);
					else if (sec->mRecordedCRC != sec->mComputedCRC)
						printf("WARNING: CRC error encoding not supported by ADF format. Ignoring CRC error for cylinder %d, head %d, sector %d.\n", i, head, j+1);
					else if (sec->mWeakOffset >= 0)
						printf("WARNING: Weak sector encoding not supported by ADF format. Ignoring error for cylinder %d, head %d, sector %d.\n", i, head, j+1);
				}

				fwrite(secbuf, sector_size, 1, fo);
			}
		}
	}

	fclose(fo);
}
