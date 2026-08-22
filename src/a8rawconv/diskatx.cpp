#include "stdafx.h"

struct ATXFileHeader {
	uint8_t		mSignature[4];			// AT8X
	uint16_t	mVersionMajor;			// 1
	uint16_t	mVersionMinor;			// 1
	uint16_t	mCreator;
	uint16_t	mCreatorVersion;
	uint32_t	mFlags;
	uint16_t	mImageType;
	uint8_t		mDensity;
	uint8_t		mFill1;
	uint32_t	mImageId;
	uint16_t	mImageVersion;
	uint16_t	mFill2;
	uint32_t	mTrackDataOffset;
	uint32_t	mTotalSize;
	uint8_t		mFill3[12];
};

static_assert(sizeof(ATXFileHeader) == 48, "ATX file header is not 48 bytes");

struct ATXTrackHeader {
	enum Flags : uint32_t {
		kFlag_MFM		= 0x00000002,		// track encoded as MFM
		kFlag_NoSkew	= 0x00000100,		// track-to-track relative skew not measured
	};

	uint32_t	mSize;
	uint16_t	mType;
	uint16_t	mReserved06;
	uint8_t		mTrackNum;
	uint8_t		mReserved09;
	uint16_t	mNumSectors;
	uint16_t	mRate;
	uint8_t		mFill2[2];
	uint32_t	mFlags;
	uint32_t	mDataOffset;
	uint8_t		mFill4[8];
};

static_assert(sizeof(ATXTrackHeader) == 32, "ATX track header is not 32 bytes");

struct ATXSectorHeader {
	uint8_t		mIndex;
	uint8_t		mFDCStatus;		// not inverted
	uint16_t	mTimingOffset;
	uint32_t	mDataOffset;
};

struct ATXTrackChunkHeader {
	enum {
		kTypeSectorData = 0x00,
		kTypeSectorList = 0x01,
		kTypeWeakBits = 0x10,
		kTypeExtSectorHeader = 0x11,
	};

	uint32_t	mSize;
	uint8_t		mType;
	uint8_t		mNum;
	uint16_t	mData;
};

void read_atx(DiskInfo& disk, const char *path, int track) {
	FILE *fi = fopen(path, "rb");
	if (!fi)
		fatalf("Unable to open file: %s\n", path);

	ATXFileHeader filehdr;
	read_raw(&filehdr, sizeof filehdr, fi, path);

	if (memcmp(&filehdr.mSignature, "AT8X", 4))
		fatalf("Cannot read ATX file %s: incorrect signature; possibly not an ATX file\n", path);

	for(;;) {
		ATXTrackHeader trkhdr;
		uint32_t trkbase = (uint32_t)ftell(fi);

		if (1 != fread(&trkhdr.mSize, 8, 1, fi))
			break;

		if (trkhdr.mSize < 8 || trkhdr.mSize >= 0x8000000U)
			fatal_read();

		if (trkhdr.mType != 0) {
			fseek(fi, trkbase + trkhdr.mSize, SEEK_SET);
			continue;
		}

		// read in the rest of the track header
		if (trkhdr.mSize < sizeof(trkhdr))
			fatal("Invalid track header in ATX file.\n");

		read_raw(&trkhdr.mTrackNum, sizeof(trkhdr) - 8, fi, path);

		// check track number
		if (trkhdr.mTrackNum > 40) {
			printf("WARNING: Ignoring track in ATX file: %u\n", trkhdr.mTrackNum);
			fseek(fi, trkbase + trkhdr.mSize, SEEK_SET);
			continue;
		}

		const bool track_mfm = (trkhdr.mFlags & ATXTrackHeader::kFlag_MFM) != 0;

		// read in the track
		TrackInfo& track_info = disk.mPhysTracks[0][trkhdr.mTrackNum * 2];

		std::vector<uint8_t> rawtrack(trkhdr.mSize);
		read_raw(rawtrack.data() + sizeof(trkhdr), trkhdr.mSize - sizeof(trkhdr), fi, path);

		// parse track chunks
		if (trkhdr.mSize >= sizeof(trkhdr) + 8) {
			std::vector<std::pair<uint8_t, uint16_t>> weakSectorInfo;
			std::vector<uint16_t> ext_sector_info(trkhdr.mNumSectors, 0);
			std::vector<ATXSectorHeader> sector_headers;

			// parse track chunks
			uint32_t tcpos = trkhdr.mDataOffset;
			while(tcpos < trkhdr.mSize - 8) {
				ATXTrackChunkHeader tchdr;
				memcpy(&tchdr, rawtrack.data() + tcpos, 8);

				if (!trkhdr.mSize || trkhdr.mSize - tcpos < tchdr.mSize)
					break;

				if (tchdr.mType == ATXTrackChunkHeader::kTypeSectorList) {
					if (tchdr.mSize < sizeof(ATXTrackChunkHeader) + sizeof(ATXSectorHeader) * trkhdr.mNumSectors)
						fatalf("Invalid ATX image: Sector list at %08x has size %08x insufficient to hold %u sectors.\n", (uint32_t)trkbase + tcpos, tchdr.mSize, trkhdr.mNumSectors);

					sector_headers.resize(trkhdr.mNumSectors);
					memcpy(sector_headers.data(), rawtrack.data() + tcpos + sizeof(ATXTrackChunkHeader), sizeof(ATXSectorHeader) * trkhdr.mNumSectors);
				} else if (tchdr.mType == ATXTrackChunkHeader::kTypeExtSectorHeader) {
					if (tchdr.mNum >= trkhdr.mNumSectors)
						fatalf("Invalid ATX image: Extended sector header chunk at %08X references invalid sector index %u.\n", (uint32_t)trkbase + tcpos, tchdr.mNum);

					ext_sector_info[tchdr.mNum] = tchdr.mData;
				} else if (tchdr.mType == ATXTrackChunkHeader::kTypeWeakBits) {
					weakSectorInfo.emplace_back(tchdr.mNum, tchdr.mData);
				}

				tcpos += tchdr.mSize;
			}

			// process sectors
			int sector_index = -1;
			for(const ATXSectorHeader& shdr : sector_headers) {
				++sector_index;

				// validate data location
				if (shdr.mDataOffset > trkhdr.mSize && trkhdr.mSize - shdr.mDataOffset < 128)
					fatalf("Invalid ATX image: track %u, sector %u extends outside of track chunk.\n", trkhdr.mTrackNum, shdr.mIndex);

				track_info.mSectors.emplace_back();
				auto& sec = track_info.mSectors.back();

				// copy the data field into the sector buffer, if there is a data field for the sector
				if (shdr.mFDCStatus & 0x10)
					memset(sec.mData, 0, 128);
				else
					memcpy(sec.mData, rawtrack.data() + shdr.mDataOffset, 128);

				memset(sec.mData + 128, 0, sizeof(sec.mData) - 128);

				sec.mIndex = shdr.mIndex;
				sec.mbMFM = track_mfm;
				sec.mAddressMark = (shdr.mFDCStatus & 0x10) ? 0x00 : (shdr.mFDCStatus & 0x20) ? 0xF8 : 0xFB;
				sec.mPosition = (float)shdr.mTimingOffset / 26042.0f;

				if (shdr.mFDCStatus & 0x04) {
					// Look for an extended sector info chunk to tell us the true physical size of
					// this sector. If we don't have it, assume 256 bytes for a 128 byte logical sector.
					sec.mSectorSize = 128 << std::max<int>(1, ext_sector_info[sector_index] & 3);
				} else {
					sec.mSectorSize = 128;
				}

				sec.mWeakOffset = -1;
				sec.mComputedAddressCRC = ComputeAddressCRC(track, 0, sec.mIndex, sec.mSectorSize, false);

				// If the CRC bit is set alone, it means a CRC error on the data field. If the CRC
				// and missing sector bits are both set, it means a CRC error on the address field.
				sec.mRecordedAddressCRC = (shdr.mFDCStatus & 0x18) == 0x18 ? ~sec.mComputedAddressCRC : sec.mComputedAddressCRC;

				sec.mComputedCRC = ComputeInvertedCRC(sec.mData, sec.mSectorSize, ComputeCRC(&sec.mAddressMark, 1));
				sec.mRecordedCRC = (shdr.mFDCStatus & 0x18) == 0x08 ? ~sec.mComputedCRC : sec.mComputedCRC;
			}

			// process weak sector data
			while(!weakSectorInfo.empty()) {
				const auto& wsi = weakSectorInfo.back();

				if (wsi.first >= track_info.mSectors.size()) {
					printf("WARNING: ATX track %u contains extra data for nonexistent sector index %u.\n", track, wsi.first);
				} else {
					SectorInfo& si = track_info.mSectors[wsi.first];

					if (wsi.second >= si.mSectorSize) {
						printf("WARNING: ATX track %u, sector %u has out of bounds weak data offset: %u.\n", track, si.mIndex, wsi.second);
					} else {
						si.mWeakOffset = (int16_t)wsi.second;
					}
				}

				weakSectorInfo.pop_back();
			}
		}

		// next track
		fseek(fi, trkbase + trkhdr.mSize, SEEK_SET);
	}

	fclose(fi);
}

void write_atx(const char *path, DiskInfo& disk, int track) {
	printf("Writing ATX file: %s\n", path);

	FILE *fo = fopen(path, "wb");
	if (!fo)
		fatalf("Unable to open output file: %s.\n", path);

	// check if we have enhanced density
	bool has_mfm = false;

	for(int i=0; i<40; ++i) {
		const TrackInfo& track_info = disk.mPhysTracks[0][i * 2];
	
		for(const SectorInfo& sec_info : track_info.mSectors) {
			if (sec_info.mbMFM) {
				has_mfm = true;
				break;
			}
		}

		if (has_mfm)
			break;
	}

	// write header
	fwrite("AT8X", 4, 1, fo);
	write_u16(1, fo);		// major version
	write_u16(1, fo);		// minor version
	write_u16(0x5241, fo);		// creator ('AR')
	write_u16(0, fo);		// creator version
	write_u32(0, fo);		// flags
	write_u16(0, fo);		// image type
	write_u8(has_mfm ? 1 : 0, fo);		// density
	write_u8(0, fo);		// [pad]
	write_u32(0, fo);		// image ID
	write_u16(0, fo);		// image version
	write_u16(0, fo);		// [pad]
	write_u32(48, fo);		// track data offset
	write_u32(0, fo);		// total size
	write_pad(12, fo);

	// write tracks
	int phantom_sectors = 0;
	int missing_sectors = 0;
	int error_sectors = 0;

	for(int i=0; i<40; ++i) {
		TrackInfo& track_info = disk.mPhysTracks[0][i * 2];

		// sort and sift out usable sectors from track
		std::vector<SectorInfo *> secptrs;
		sift_sectors(track_info, i, secptrs);

		const int num_secs = (int)secptrs.size();

		bool sector_map[18] = {0};
		int weak_count = 0;
		int ext_count = 0;
		bool track_has_mfm = false;
		bool track_has_fm = false;

		for(auto *sec_ptr : secptrs) {
			if (sec_ptr->mWeakOffset >= 0)
				++weak_count;

			if (sec_ptr->mIndex >= 1 && sec_ptr->mIndex <= 18) {
				if (sector_map[sec_ptr->mIndex - 1])
					++phantom_sectors;
				else
					sector_map[sec_ptr->mIndex - 1] = true;
			}

			if (sec_ptr->mbMFM)
				track_has_mfm = true;
			else
				track_has_fm = true;

			if (sec_ptr->mSectorSize > 256)
				++ext_count;
		};

		if (track_has_fm && track_has_mfm)
			printf("WARNING: Track %2u has mixed FM and MFM sectors, which is not supported by ATX.\n", i);

		// write track header
		write_u32(32					// track header
			+ 8 + 8*num_secs			// sector info chunk
			+ 8 + 128*num_secs			// sector data chunk
			+ 8*weak_count				// weak chunks
			+ 8*ext_count				// extended sector info chunks
			+ 8, fo);					// terminator chunk
		write_u16(0, fo);			// type
		write_u16(0, fo);			// [pad]
		write_u8(i, fo);			// track number
		write_u8(0, fo);			// [pad]
		write_u16(num_secs, fo);	// sector count
		write_u16(0, fo);			// rate
		write_pad(2, fo);			// [pad]
		write_u32(track_has_mfm ? 2 : 0, fo);			// flags
		write_u32(32, fo);			// data offset
		write_pad(8, fo);			// [pad]

		// write sector list header
		write_u32(8 + 8 * num_secs, fo);
		write_u8(1, fo);
		write_pad(3, fo);

		// write out sector headers

		uint32_t base_data_offset = 32 + 8 + 8*num_secs + 8;
		uint32_t data_offset = base_data_offset;
		for(auto it = secptrs.begin(), itEnd = secptrs.end();
			it != itEnd;
			++it)
		{
			auto *sec_ptr = *it;

			write_u8(sec_ptr->mIndex, fo);

			uint8_t fdcStatus = 0;

			// The 1050 uses the WD279X FDC, which only looks at and reports the MSB.
			fdcStatus += (~sec_ptr->mAddressMark & 2) << 4;

			if (sec_ptr->mAddressMark == 0xF8)
				printf("WARNING: Track %2d, sector %2d: Deleted sector found.\n", i, sec_ptr->mIndex);
			else if (sec_ptr->mAddressMark != 0xFB)
				printf("WARNING: Track %2d, sector %2d: User-defined sector found (%02X).\n", i, sec_ptr->mIndex, sec_ptr->mAddressMark);

			// set both CRC and missing sector bits if the address CRC didn't match
			// set CRC error flag only if the data frame CRC didn't match
			if (sec_ptr->mComputedAddressCRC != sec_ptr->mRecordedAddressCRC)
				fdcStatus |= 0x18;
			else if (sec_ptr->mComputedCRC != sec_ptr->mRecordedCRC)
				fdcStatus |= 0x08;

			// set lost data, and DRQ flags if sector is long
			if (sec_ptr->mSectorSize != 128) {
				printf("WARNING: Track %2d, sector %2d: Long sector of %u bytes found.\n", i, sec_ptr->mIndex, sec_ptr->mSectorSize);

				// We need to preserve the CRC flag for long reads. 810s appear to read the status byte
				// immediately, whereas 1050s wait for the FDC to complete the sector read first.
				// This can influence whether the CRC is correct. We follow the 1050, which means that
				// the CRC flag is set appropriately, lost data is set and DRQ is set.
				//
				fdcStatus |= 0x06;
			}

			// set extra data if weak sector
			if (sec_ptr->mWeakOffset >= 0)
				fdcStatus |= 0x40;

			if (fdcStatus)
				++error_sectors;

			write_u8(fdcStatus, fo);
			write_u16((uint16_t)((int)(sec_ptr->mPosition * 26042) % 26042), fo);
			write_u32(data_offset, fo);
			data_offset += 128;
		};

		// write out sector data
		write_u32((data_offset - base_data_offset) + 8, fo);
		write_u8(0, fo);
		write_pad(3, fo);

		for(auto it = secptrs.begin(), itEnd = secptrs.end();
			it != itEnd;
			++it)
		{
			auto *sec_ptr = *it;
			fwrite(sec_ptr->mData, 128, 1, fo);
		};

		// write out weak chunk and long sector info
		for(int j=0; j<num_secs; ++j) {
			const SectorInfo& si = *secptrs[j];
			if (si.mWeakOffset >= 0) {
				write_u32(8, fo);
				write_u8(0x10, fo);
				write_u8((uint8_t)j, fo);
				write_u16((uint16_t)secptrs[j]->mWeakOffset, fo);
			}

			if (si.mSectorSize > 256) {
				write_u32(8, fo);
				write_u8(0x11, fo);
				write_u8((uint8_t)j, fo);

				uint16_t sscode = 2;

				if (si.mSectorSize >= 1024)
					sscode = 3;

				write_u16((uint16_t)sscode, fo);
			}
		};

		// write end of track chunks
		write_u32(0, fo);
		write_u32(0, fo);

		// report any missing sectors
		if (std::find(std::begin(sector_map), std::end(sector_map), true) == std::end(sector_map)) {
			if (track < 0 || track == i) {
				printf("WARNING: No sectors found for track %d -- possibly unformatted.\n", i);
				missing_sectors += 18;
			}
		} else {
			std::vector<int> missingSecs;

			for(int j=0; j<18; ++j) {
				if (!sector_map[j])
					missingSecs.push_back(j + 1);
			}

			if (!missingSecs.empty()) {
				std::sort(missingSecs.begin(), missingSecs.end());

				printf("WARNING: Track %2d: Missing sectors:", i);

				int skipComma = 1;

				for(auto it = missingSecs.begin(), itEnd = missingSecs.end();
					it != itEnd;
					++it)
				{
					printf(", %d" + skipComma, *it);

					skipComma = 0;
				}

				printf(".\n");
			}

			missing_sectors += (int)missingSecs.size();
		}
	}

	// back-patch size
	uint32_t total_size = ftell(fo);
	fseek(fo, 32, SEEK_SET);
	write_u32(total_size, fo);

	printf("%d missing sector%s, %d phantom sector%s, %d sector%s with errors\n"
		, missing_sectors, missing_sectors == 1 ? "" : "s"
		, phantom_sectors, phantom_sectors == 1 ? "" : "s"
		, error_sectors, error_sectors == 1 ? "" : "s");

	fclose(fo);
}