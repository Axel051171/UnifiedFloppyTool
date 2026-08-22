#include "stdafx.h"

void read_atr(DiskInfo& disk, const char *path, int track_select) {
	uint8_t header[16];

	printf("Reading ATR file: %s\n", path);

	FILE *fi = fopen(path, "rb");
	if (!fi)
		fatalf("Unable to open input file: %s.\n", path);

	read_raw(header, 16, fi, path);

	// check signature
	if (header[0] != 0x96 || header[1] != 0x02)
		fatal("Incorrect ATR file signature (possibly not ATR file?).\n");

	const uint32_t para_count = read_u16_le(&header[2]) + ((uint32_t)read_u16_le(&header[6]) << 16);
	const uint32_t sector_size = read_u16_le(&header[4]);

	if (sector_size != 128 && sector_size != 256)
		fatalf("Unsupported ATR sector size: %u bytes.\n", sector_size);

	// check if we have long boot sectors in a DD image, or otherwise fragments
	const uint32_t partial_sector_len = (para_count << 4) % sector_size;
	if (partial_sector_len & 0x7F)
		fatalf("Inconsistent ATR paragraph count (%u) for sector size %u.\n", para_count, sector_size);

	const bool short_boot_sectors = (partial_sector_len != 0);

	// compute sector count
	uint32_t sector_count = (para_count << 4) / sector_size;

	if (short_boot_sectors)
		sector_count += 2;

	// determine disk geometry
	int sectors_per_track = 0;
	int sides = 1;
	bool mfm = false;

	if (sector_size == 128) {
		if (sector_count == 1040) {
			mfm = true;
			sectors_per_track = 26;
		} else if (sector_count == 720) {
			sectors_per_track = 18;
		}
	} else {
		if (sector_count == 720 || sector_count == 1440) {
			mfm = true;
			sectors_per_track = 18;

			if (sector_count == 1440)
				sides = 2;
		}
	}

	if (!sectors_per_track)
		fatalf("Unsupported ATR disk geometry: %u sectors, %u bytes per sector.", sector_count, sector_size);

	// read in all sectors
	std::vector<uint8_t> sector_data(sector_size * sector_count, 0);

	if (short_boot_sectors) {
		// read in 3 SD boot sectors
		for(int i=0; i<3; ++i)
			read_raw(sector_data.data() + i*256, 128, fi, path);

		// read in remaining DD sectors
		read_raw(sector_data.data() + 3*256, sector_data.size() - 3*256, fi, path);
	} else {
		read_raw(sector_data.data(), sector_data.size(), fi, path);
	}

	// if we had long DD boot sectors, check whether they were stored as 128b or not
	if (sector_size == 256 && !short_boot_sectors) {
		bool slotTwoEmpty = true;

		for(int i=0; i<128; ++i) {
			if (sector_data[128 + i]) {
				slotTwoEmpty = false;
				break;
			}
		}

		bool slotFiveEmpty = true;

		for(int i=0; i<128; ++i) {
			if (sector_data[128*4 + i]) {
				slotFiveEmpty = false;
				break;
			}
		}

		if (!slotTwoEmpty && slotFiveEmpty) {
			// they're packed together -- unpack them and zero the upper halves
			memcpy(&sector_data[512], &sector_data[256], 128);
			memcpy(&sector_data[256], &sector_data[128], 128);
			memset(&sector_data[256], 0, 128);
		}
	}

	for(int side = 0; side < sides; ++side) {
		for(int track = 0; track < 40; ++track) {
			if (track_select >= 0 && track != track_select)
				continue;

			TrackInfo& track_info = disk.mPhysTracks[side][track * 2];

			// side 2 needs to be reversed from side 1, so we start from the end of the
			// last track and walk backwards
			const uint8_t *data = sector_data.data() + sector_size * sectors_per_track * (side ? 80 - track : track);

			track_info.mSectors.reserve(sectors_per_track);
			for(int secidx = 0; secidx < sectors_per_track; ++secidx) {
				track_info.mSectors.emplace_back();
				auto& sec = track_info.mSectors.back();

				memset(sec.mData, 0, sizeof sec.mData);

				if (side)
					data -= sector_size;

				memcpy(sec.mData, data, sector_size);

				if (!side)
					data += sector_size;

				sec.mPosition = -1.0f;
				sec.mIndex = secidx + 1;
				sec.mbMFM = mfm;
				sec.mAddressMark = 0xFB;
				sec.mSectorSize = sector_size;
				sec.mWeakOffset = -1;
				sec.mComputedAddressCRC = ComputeAddressCRC(track, 0, sec.mIndex, sector_size, mfm);
				sec.mRecordedAddressCRC = sec.mComputedAddressCRC;
				sec.mComputedCRC = ComputeInvertedCRC(sec.mData, sector_size, ComputeCRC(&sec.mAddressMark, 1));
				sec.mRecordedCRC = sec.mComputedCRC;
			}
		}
	}

	disk.mTrackCount = 40;
	disk.mTrackStep = 2;
	disk.mSideCount = sides;
	disk.mPrimarySectorSize = sector_size;
	disk.mPrimarySectorsPerTrack = sectors_per_track;

	fclose(fi);
}

void write_atr(const char *path, DiskInfo& disk, int selected_track) {
	uint32_t sector_size = 128;
	uint32_t sectors_per_track = 18;
	int sides = disk.mSideCount;

	// scan track 0 to see if we have DD format (256 byte sectors) or ED format
	// (128 byte sectors, >18 sectors/track)
	for(const SectorInfo& si : disk.mPhysTracks[0][0].mSectors) {
		if (si.mSectorSize >= 256)
			sector_size = 256;

		if (si.mIndex > 18)
			sectors_per_track = 26;
	};

	if (sector_size >= 256) {
		if (disk.mSideCount > 1)
			printf("Writing DSDD ATR file: %s\n", path);
		else
			printf("Writing double density ATR file: %s\n", path);
	} else if (sectors_per_track > 18)
		printf("Writing enhanced density ATR file: %s\n", path);
	else
		printf("Writing single density ATR file: %s\n", path);

	FILE *fo = fopen(path, "wb");
	if (!fo)
		fatalf("Unable to open output file: %s.\n", path);

	const uint32_t data_tracks = std::min<uint32_t>(disk.mTrackCount, disk.mTrackStep > 1 ? 40 : 80);
	const uint32_t total_sectors = sides * data_tracks * sectors_per_track;
	const uint32_t total_bytes = total_sectors * sector_size - (sector_size > 128 ? 384 : 0);
	const uint32_t total_paras = total_bytes >> 4;

	// write header
	write_u8(0x96, fo);
	write_u8(0x02, fo);
	write_u16((uint16_t)total_paras, fo);
	write_u16(sector_size, fo);
	write_u16((uint16_t)(total_paras >> 16), fo);
	write_pad(8, fo);

	// write tracks
	char secbuf[256];

	std::vector<const SectorInfo *> ordered_sectors;

	for(int side=0; side<sides; ++side) {
		for(int i=0; i<(int)data_tracks; ++i) {
			const int track = side ? (data_tracks - 1) - i : i;

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
						printf("WARNING: Variable sectors not supported by ATR format. Discarding duplicate physical sector for track %d, sector %d.\n", track, secptr->mIndex);
					else
						secptrs2[secptr->mIndex - 1] = secptr;
				}
			};

			// queue sectors for write in ATR order
			for(uint32_t j=0; j<sectors_per_track; ++j) {
				const SectorInfo *sec = secptrs2[side ? (sectors_per_track - 1) - j : j];

				ordered_sectors.push_back(sec);

				if (!sec) {
					if (selected_track < 0 || selected_track == track)
						printf("WARNING: Missing sectors not supported by ATR format. Writing out null data for track %d.%d, sector %d.\n", track, side, j+1);
				} else {
					if (sec->mSectorSize != sector_size)
						printf("WARNING: Variable sector size not supported by ATR format. Writing out truncated data for track %d, sector %d.\n", track, j+1);
					else if (sec->mRecordedCRC != sec->mComputedCRC)
						printf("WARNING: CRC error encoding not supported by ATR format. Ignoring CRC error for track %d, sector %d.\n", track, j+1);
					else if (sec->mAddressMark != 0xFB)
						printf("WARNING: Deleted sector encoding not supported by ATR format. Ignoring error for track %d, sector %d.\n", track, j+1);
					else if (sec->mWeakOffset >= 0)
						printf("WARNING: Weak sector encoding not supported by ATR format. Ignoring error for track %d, sector %d.\n", track, j+1);
				}
			}
		}
	}

	int vsec = 1;
	for(const SectorInfo *sec : ordered_sectors) {
		if (!sec)
			memset(secbuf, 0, sizeof secbuf);
		else
			memcpy(secbuf, sec->mData, sector_size);

		// boot sectors are always written as 128 bytes
		if (vsec <= 3)
			fwrite(secbuf, 128, 1, fo);
		else
			fwrite(secbuf, sector_size, 1, fo);

		++vsec;
	}

	fclose(fo);
}
