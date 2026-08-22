#include "stdafx.h"

static const int kLogicalToPhysicalA2DOS[16]={
	0, 13, 11, 9, 7, 5, 3, 1, 14, 12, 10, 8, 6, 4, 2, 15
};

static const int kLogicalToPhysicalA2ProDOS[16]={
	0, 2, 4, 6, 8, 10, 12, 14, 1, 3, 5, 7, 9, 11, 13, 15
};

void read_apple2_dsk(DiskInfo& disk, const char *path, int track, bool useProDOSOrder) {
	uint32_t sector_size = 256;
	uint32_t sectors_per_track = 16;

	printf("Reading Apple II disk image (%s ordering): %s\n", useProDOSOrder ? "ProDOS" : "DOS 3.3", path);

	FILE *fi = fopen(path, "rb");
	if (!fi)
		fatalf("Unable to open input file: %s.\n", path);

	// read tracks
	const int (&sectorOrder)[16] = useProDOSOrder ? kLogicalToPhysicalA2ProDOS : kLogicalToPhysicalA2DOS;

	bool seenMissingWarning = false;
	uint32_t missingSectors = 0;
	uint32_t badSectors = 0;

	for(int i=0; i<35; ++i) {
		TrackInfo& track_info = disk.mPhysTracks[0][i * 2];

		track_info.mSectors.resize(16, {});

		for(int j=0; j<16; ++j) {
			SectorInfo& si = track_info.mSectors[j];

			if (1 != fread(si.mData, 256, 1, fi))
				fatalf("Unable to read data from input file: %s.\n", path);

			si.mAddressMark = 0xFE;		// default DOS 3.3 volume number
			si.mbMFM = false;
			si.mRecordedCRC = 0;
			si.mComputedCRC = 0;
			si.mSectorSize = 256;
			si.mWeakOffset = -1;
			si.mIndex = sectorOrder[j];
			si.mPosition = (float)si.mIndex / 16.0f;
		}
	}

	fclose(fi);
}

void write_apple2_dsk(const char *path, DiskInfo& disk, int track, bool useProDOSOrder, bool mac_format) {
	uint32_t sector_size = mac_format ? 512 : 256;
	uint32_t sectors_per_track = 16;

	if (mac_format)
		printf("Writing 3.5\" Mac / Apple II Unidisk disk image: %s\n", path);
	else
		printf("Writing Apple II disk image (%s ordering): %s\n", useProDOSOrder ? "ProDOS" : "DOS 3.3", path);

	FILE *fo = fopen(path, "wb");
	if (!fo)
		fatalf("Unable to open output file: %s.\n", path);

	// write tracks
	const int (&sectorOrder)[16] = useProDOSOrder ? kLogicalToPhysicalA2ProDOS : kLogicalToPhysicalA2DOS;
	char secbuf[512];
	bool seenMissingWarning = false;
	uint32_t missingSectors = 0;
	uint32_t badSectors = 0;

	const int track_count = mac_format ? 80 : 35;
	const int sides = mac_format ? disk.mSideCount : 1;

	for(int i=0; i<track_count; ++i) {
		for (int side=0; side<sides; ++side) {
			TrackInfo& track_info = disk.mPhysTracks[side][mac_format ? i : i*2];

			// The 3.5" format starts with 12 sectors per track and drops one sector every
			// 16 tracks. The 5.25" DOS 3.3 / ProDOS format simply has 16 sectors.
			uint32_t sectorsPerTrack = mac_format ? 12 - (i >> 4) : 16;

			// sort and sift out usable sectors from track
			std::vector<SectorInfo *> secptrs;
			sift_sectors(track_info, i, secptrs);

			// iterate over sectors
			const SectorInfo *secptrs2[16] = { 0 };

			for(auto it = secptrs.begin(), itEnd = secptrs.end();
				it != itEnd;
				++it)
			{
				const SectorInfo *secptr = *it;

				if (secptr->mIndex >= 0 && secptr->mIndex < (int)sectorsPerTrack) {
					if (secptrs2[secptr->mIndex])
						printf("WARNING: Variable sectors not supported by DSK/DO format. Discarding duplicate physical sector for track %d, sector %d.\n", i, secptr->mIndex);
					else
						secptrs2[secptr->mIndex] = secptr;
				}
			};

			// write out sectors

			uint32_t missingSectorMask = 0;

			for(uint32_t j=0; j<sectorsPerTrack; ++j) {
				int physec = mac_format ? j : sectorOrder[j];
				const SectorInfo *sec = secptrs2[physec];

				if (!sec) {
					if (track < 0 || track == i) {
						++missingSectors;
						missingSectorMask |= (1 << physec);
					}

					memset(secbuf, 0, sizeof secbuf);
				} else {
					memcpy(secbuf, sec->mData, sector_size);

					if (sec->mSectorSize != sector_size) {
						printf("WARNING: Variable sector size not supported by DSK format. Writing out truncated data for track %d, sector %d.\n", i, physec+1);
						++badSectors;
					} else if (sec->mRecordedCRC != sec->mComputedCRC) {
						printf("WARNING: CRC error encoding not supported by DSK format. Ignoring CRC error for track %d, sector %d.\n", i, physec+1);
						++badSectors;
					} else if (sec->mWeakOffset >= 0) {
						printf("WARNING: Weak sector encoding not supported by DSK format. Ignoring error for track %d, sector %d.\n", i, physec+1);
						++badSectors;
					}
				}

				fwrite(secbuf, sector_size, 1, fo);
			}

			if (missingSectorMask) {
				if (!seenMissingWarning) {
					seenMissingWarning = true;
					printf("WARNING: Missing sectors not supported by DSK/DO format. Writing out null data.\n");
				}

				if (missingSectorMask == (1 << sectorsPerTrack) - 1)
					printf("WARNING: No sectors found on track %u.\n", i);
				else {
					printf("WARNING: Track %u: missing sectors:", i);

					for(uint32_t j=0; j<sectorsPerTrack; ++j) {
						if (missingSectorMask & (1 << j))
							printf(" %u", j+1);
					}

					printf("\n");
				}
			}
		}
	}

	fclose(fo);

	printf("%d missing sector%s, %d sector%s with errors\n"
		, missingSectors, missingSectors == 1 ? "" : "s"
		, badSectors, badSectors == 1 ? "" : "s");
}

void read_apple2_nib(RawDisk& raw_disk, const char *path, int selected_track) {
	printf("Reading Apple II nibble image: %s\n", path);

	FILE *fi = fopen(g_inputPath.c_str(), "rb");
	if (!fi)
		fatalf("Unable to open input file: %s.\n", path);

	// The image should be exactly $1A00 * 35 tracks = 232,960 bytes. We allocate
	// 5 bytes over for the sync detection below.
	std::unique_ptr<uint8_t[]> buf(new uint8_t[0x1A00 * 35 + 5]);

	if (1 != fread(buf.get(), 0x1A00 * 35, 1, fi))
		fatal_read();

	fclose(fi);

	// For now, take the easy/lazy out, and synthesize flux from the bytes.
	// This will result in bogus timing for sync bytes, but NIB doesn't
	// contain whether a byte was a sync byte or not, and we don't output
	// any A2 formats that care about sector timing.
	//
	// Apple II disks are encoded as GCR with 4us bit cells and a nominal
	// rotational speed of 300 RPM. This means that the 6,656 bytes per
	// track encoded in the NIB is always an overdump, as only 6,250 bytes
	// will fit at most, and less than that with 40-bit sync bytes. We do
	// have to generate normal sample timing as otherwise the track
	// parser will correct the timing as part of rotational speed compensation,
	// so we generate 1.065 tracks here.

	for(int i=0; i<35; ++i) {
		if (selected_track >= 0 && i != selected_track)
			continue;

		// initialize raw track parameters
		// 25ns samples for 0.2s (300 RPM) = 8M samples
		// 
		const uint32_t kBytesPerTrackImage = 0x1A00;
		const uint32_t kSamplesPerTrackImage = kBytesPerTrackImage * 8 * 160;
		const uint32_t kSamplesPerRev = 8000000;

		RawTrack& raw_track = raw_disk.mPhysTracks[0][i * 2];
		raw_track.mIndexTimes.push_back(0);
		raw_track.mIndexTimes.push_back(kSamplesPerRev);
		raw_track.mIndexTimes.push_back(kSamplesPerRev * 2);

		raw_track.mSamplesPerRev = (float)kSamplesPerRev;
		raw_track.mPhysTrack = i * 2;
		raw_track.mSpliceStart = -1;
		raw_track.mSpliceEnd = -1;
		
		// emit 5 sync bytes to ensure that the GCR decoder is byte aligned
		uint32_t t = 80;

		for(uint32_t j=0; j<5; ++j) {
			for(uint32_t k=0; k<8; ++k) {
				raw_track.mTransitions.push_back(t);
				t += 160;
			}

			t += 320;
		}

		// synthesize flux transitions (4us bit cell = 160 samples @ 25ns/sample)
		const uint8_t *tracksrc = buf.get() + 0x1A00 * i;

		for(uint32_t byteIdx = 0; byteIdx < 0x1A00; ++byteIdx) {
			const uint8_t c = tracksrc[byteIdx];

			// emit bits from MSB to LSB
			for(int bit = 0; bit < 8; ++bit) {
				if (c & (0x80 >> bit))
					raw_track.mTransitions.push_back(t);

				t += 160;
			}
		}
	}
};

void write_apple2_nib(const char *path, const DiskInfo& disk, int track) {
	printf("Writing Apple II nibble disk image: %s\n", path);

	if (disk.mPhysTracks[0][0].mGCRData.size() < 0x1A00)
		fatalf("No GCR data present. Apple II GCR decoding must be used for nibble output.\n");

	FILE *fo = fopen(path, "wb");
	if (!fo)
		fatalf("Unable to open output file: %s.\n", path);

	// write tracks
	char nibbuf[0x1A00];

	for(int i=0; i<35; ++i) {
		const auto& track_data = disk.mPhysTracks[0][i * 2];
		const uint8_t *p = track_data.mGCRData.data();
		int track_offset = 0;
		int track_len = (int)track_data.mGCRData.size();
		int max_offset = track_len - 0x1A00;

		if (max_offset <= 0) {
			if (max_offset < 0)
				printf("WARNING: Track %u is short (<$1A00 bytes) and will be padded.\n", track);

			max_offset = 0;
			memcpy(nibbuf, p, track_len);
			memset(nibbuf + track_len, 0, 0x1A00 - track_len);
		} else {
			int best_len = 0;
			int sync_count = 0;


			for(int i=0; i<=max_offset; ++i) {
				if (p[i] == 0xFF)
					++sync_count;
				else {
					if (sync_count > best_len) {
						best_len = sync_count;
						track_offset = i - sync_count/2;
					}

					sync_count = 0;
				}
			}

			if (sync_count > best_len) {
				best_len = sync_count;
				track_offset = max_offset - sync_count/2;
			}

			memcpy(nibbuf, p + track_offset, 0x1A00);
		}

		fwrite(nibbuf, 0x1A00, 1, fo);
	}

	fclose(fo);
}
