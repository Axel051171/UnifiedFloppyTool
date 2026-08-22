#include "stdafx.h"

// Nominal cell times for encodings at 5ns/tick @ 360 RPM
const uint32_t kNominalFMBitCellTime	= 640;	// 4us @ 288 RPM = 3.200us @ 360 RPM (Atari FM)
const uint32_t kNominalA2GCRBitCellTime	= 667;	// 4us @ 300 RPM = 3.333us @ 360 RPM (Apple II GCR)

static const uint8_t kGCR6Encoder[64]={
	0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
	0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
	0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
	0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
	0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
	0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
	0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
	0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

class SectorEncoder {
public:
	SectorEncoder()	{

	}

	void SetBitCellTime(uint32_t bct) {
		mBitCellTime = bct;
	}

	void SetPrecompEnabled(bool enabled) {
		mbPrecompEnabled = enabled;
	}

	void BeginCritical() {
		mCriticalStart = mTime;
	}

	void EndCritical() {
		mCriticalEnd = mTime;
	}

	void EncodeByteFM(uint8_t v) {
		EncodeByteFM(0xFF, v);
	}

	void EncodeByteFM(uint8_t clock, uint8_t data) {
		EncodePartialByteFM(clock, data, 8);
	}

	void EncodePartialByteFM(uint8_t clock, uint8_t data, int bits) {
		for(int i=0; i<bits; ++i) {
			if (clock & 0x80)
				mStream.push_back(mTime);

			if (data & 0x80)
				mStream.push_back(mTime + mBitCellTime);

			clock += clock;
			data += data;

			mTime += mBitCellTime*2;
		}		
	}

	void EncodeWeakByteFM() {
		for(int i=0; i<5; ++i) {
			mStream.push_back(mBitCellTime);

			mTime += (mBitCellTime * 3) >> 1;

			mStream.push_back(mBitCellTime);

			mTime += (mBitCellTime * 3 + 1) >> 1;
		}

		mTime += mBitCellTime;
	}

	void EncodeByteMFM(uint8_t v) {
		EncodeByteMFM(0xFF, v);
	}

	void EncodeByteMFM(uint8_t clock_mask, uint8_t data) {
		EncodeByteMFM(clock_mask, data, 8);
	}

	void EncodeByteMFM(uint8_t clock_mask, uint8_t data, int bits) {
		static const uint8_t kExpand4[16] = {
			0b0'00000000,
			0b0'00000001,
			0b0'00000100,
			0b0'00000101,
			0b0'00010000,
			0b0'00010001,
			0b0'00010100,
			0b0'00010101,
			0b0'01000000,
			0b0'01000001,
			0b0'01000100,
			0b0'01000101,
			0b0'01010000,
			0b0'01010001,
			0b0'01010100,
			0b0'01010101,
		};

		// shift in data bits only
		mMFMShifter = (mMFMShifter & 0xFF0000) + (kExpand4[data >> 4] << 8) + kExpand4[data & 15];

		// recompute new clock bits
		uint32_t clockMask32 = (kExpand4[clock_mask >> 4] << 8) + kExpand4[clock_mask & 15];

		mMFMShifter += ~((mMFMShifter << 1) | (mMFMShifter >> 1)) & (clockMask32 << 1);

		// shift out data and clock bits
		int bits2 = bits * 2;

		if (mbPrecompEnabled) {
			// write precompensation -- shift flux transitions by 125us next to adjacent transitions
			for(int i=0; i<bits2; ++i) {
				if (mMFMShifter & 0x8000) {
					uint32_t adjacentBits = mMFMShifter & 0x22000;

					if (adjacentBits == 0x20000) {
						// close to prior transition -- shift backwards 1/16th of a bit cell
						mStream.push_back(mTime);
					} else if (adjacentBits == 0x2000) {
						// close to next transition -- shift forwards 1/16th of a bit cell
						mStream.push_back(mTime + (mBitCellTime >> 3));
					} else {
						// no transitions nearly -- use nominal delay
						mStream.push_back(mTime + (mBitCellTime >> 4));
					}

				}

				mMFMShifter += mMFMShifter;

				mTime += mBitCellTime;
			}
		} else {
			for(int i=0; i<bits2; ++i) {
				if (mMFMShifter & 0x8000)
					mStream.push_back(mTime);

				mMFMShifter += mMFMShifter;

				mTime += mBitCellTime;
			}
		}
	}

	void EncodeWeakByteMFM() {
		for(int i=0; i<5; ++i) {
			mStream.push_back(mBitCellTime);

			mTime += (mBitCellTime * 3) >> 1;

			mStream.push_back(mBitCellTime);

			mTime += (mBitCellTime * 3 + 1) >> 1;
		}

		mTime += mBitCellTime;
	}

	void FlushMFM() {
		EncodeByteMFM(0xFF, 0, 2);
	}

	void EncodeByteGCR(uint8_t data) {
		for(int i=0; i<8; ++i) {
			if (data & 0x80)
				mStream.push_back(mTime);

			data <<= 1;
			mTime += mBitCellTime;
		}
	}

	void EncodeSyncByteGCR() {
		// write $FF
		EncodeByteGCR(0xFF);

		// slip two bits (%00)
		mTime += mBitCellTime*2;
	}

	void EncodeSyncBytesGCR(int count) {
		while(count-- > 0)
			EncodeSyncByteGCR();
	}

	std::vector<uint32_t> mStream;
	uint32_t mTime = 0;
	uint32_t mCriticalStart = ~0;
	uint32_t mCriticalEnd = ~0;
	uint32_t mBitCellTime = 0;
	uint32_t mMFMShifter = 0;
	bool mbPrecompEnabled = false;
};

struct SectorCopy {
	const SectorInfo *mpSector;
	const SectorEncoder *mpEncodedSector;
	uint32_t mPosition;
	uint32_t mEncodeStart;
	uint32_t mEncodeEnd;
};

void encode_track(RawTrack& dst, TrackInfo& src, int track, int side, double periodMultiplier, bool a2gcr, bool precise) {
	uint32_t bitCellTime = (uint32_t)(0.5 + kNominalFMBitCellTime * periodMultiplier);

	// check if we have MFM sectors
	bool mfm = false;

	for(auto it = src.mSectors.begin(), itEnd = src.mSectors.end();
		it != itEnd;
		++it)
	{
		if (it->mbMFM)
			mfm = true;
	}

	if (mfm)
		bitCellTime >>= 1;

	if (a2gcr)
		bitCellTime = (uint32_t)(0.5 + kNominalA2GCRBitCellTime * periodMultiplier);

	// We use 5ns encoding so as to be able to hit both KryoFlux (40ns) and SuperCard Pro (25ns).
	// Rotational speed is 360 RPM.
	dst.mSamplesPerRev = 200000000.0f / 6.0f;

	for(int i=0; i<6; ++i)
		dst.mIndexTimes.push_back(200000000 * (i + 1) / 6);

	// collect sectors to encode
	std::vector<SectorInfo *> sectors;
	sift_sectors(src, track, sectors);

	// find the lowest numbered sector and use that for the index mark
	SectorInfo *lowest_sec = nullptr;
	size_t numsecs = sectors.size();
	for(size_t i=0; i<numsecs; ++i) {
		if (!lowest_sec || sectors[i]->mIndex < lowest_sec->mIndex)
			lowest_sec = sectors[i];
	}

	// find the biggest gap and use that for the splice point
	SectorInfo *first_sec = nullptr;

	if (numsecs)
		first_sec = sectors[0];

	dst.mSpliceStart = -1;
	dst.mSpliceEnd = -1;

	// encode sectors to flux transition bitstreams
	std::vector<SectorEncoder> sector_encoders(sectors.size());

	// encode sector to FM, MFM, or GCR
	if (a2gcr) {
		for(size_t i=0; i<numsecs; ++i) {
			const SectorInfo& sec = *sectors[i];
			SectorEncoder& enc = sector_encoders[i];
			enc.SetBitCellTime(bitCellTime);

			bool first_sec = (&sec == lowest_sec);

			// Minimal layout that we need:
			//
			//	5 x FF_sync
			//	D5 AA 96
			//	volume, track, sector, checksum (4-4)
			//	D5 AA EB
			//	6 x FF_sync (gap 2)
			//	D5 AA AD
			//	343 x 6-2 encoded data + checksum
			//	D5 AA EB
			//
			// Gap between sectors is around 40 sync bytes. 4us at 300 RPM gives
			// 50K raw bits to encode, above takes 3014 raw bits/sector or just
			// over 48K raw bits minimum to encode 16 sectors. This allows to fit
			// about 11 sync bytes between sectors (gap 3); we use 10 to give us
			// a little margin.

			enc.BeginCritical();
			enc.EncodeSyncBytesGCR(5);
			enc.EncodeByteGCR(0xD5);
			enc.EncodeByteGCR(0xAA);
			enc.EncodeByteGCR(0x96);

			uint8_t hdr[4]={
				sec.mAddressMark,		// volume
				(uint8_t)track,			// track
				(uint8_t)sec.mIndex,	// sector
				0x00					// checksum
			};

			// compute checksum
			hdr[3] = hdr[0] ^ hdr[1] ^ hdr[2];

			// encode header bytes using 4-4 encoding
			for(const uint8_t v : hdr) {
				enc.EncodeByteGCR((v >> 1) | 0xAA);
				enc.EncodeByteGCR(v | 0xAA);
			}

			enc.EncodeByteGCR(0xDE);
			enc.EncodeByteGCR(0xAA);
			enc.EncodeByteGCR(0xEB);
			enc.EncodeSyncBytesGCR(6);
			enc.EncodeByteGCR(0xD5);
			enc.EncodeByteGCR(0xAA);
			enc.EncodeByteGCR(0xAD);

			// prenibble data block using 6-2 encoding
			uint8_t nibblebuf[344];

			nibblebuf[0] = 0;

			// prenibble whole fragment bytes
			for(int j=0; j<84; ++j) {
				const uint8_t a = sec.mData[j] & 3;
				const uint8_t b = sec.mData[j + 86] & 3;
				const uint8_t c = sec.mData[j + 172] & 3;
				const uint8_t v = a + (b << 2) + (c << 4);

				nibblebuf[j + 1] = ((v >> 1) & 0x15) + ((v << 1) & 0x2A);
			}

			// prenibble partial fragment bytes
			for(int j=84; j<86; ++j) {
				const uint8_t a = sec.mData[j] & 3;
				const uint8_t b = sec.mData[j + 86] & 3;
				const uint8_t v = a + (b << 2);

				nibblebuf[j + 1] = ((v >> 1) & 0x15) + ((v << 1) & 0x2A);
			}

			// prenibble base bits 2-7
			for(int j=0; j<256; ++j) {
				nibblebuf[j + 87] = sec.mData[j] >> 2;
			}

			nibblebuf[343] = 0;

			// apply adjacent XOR encoding and encode to GCR
			for(int j = 0; j < 343; ++j) {
				enc.EncodeByteGCR(kGCR6Encoder[nibblebuf[j] ^ nibblebuf[j + 1]]);
			}
			
			enc.EncodeByteGCR(0xD5);
			enc.EncodeByteGCR(0xAA);
			enc.EncodeByteGCR(0xEB);
			enc.EndCritical();
			enc.EncodeSyncBytesGCR(10);
		}
	} else if (mfm) {
		for(size_t i=0; i<numsecs; ++i) {
			const SectorInfo& sec = *sectors[i];
			SectorEncoder& enc = sector_encoders[i];
			enc.SetBitCellTime(bitCellTime);
			enc.SetPrecompEnabled(dst.mPhysTrack >= 40);

			bool first_sec = (&sec == lowest_sec);

			for(int j=0; j<11; ++j)
				enc.EncodeByteMFM(0x00);

			enc.BeginCritical();
			enc.EncodeByteMFM(0x00);

			// write sector header
			uint8_t sechdr[10] = {
				0xA1,
				0xA1,
				0xA1,
				0xFE,
				(uint8_t)track,
				(uint8_t)side,
				(uint8_t)sec.mIndex,
				(uint8_t)(sec.mSectorSize > 128 ? 1 : 0),
				0,
				0
			};

			uint16_t crc = ComputeCRC(sechdr, 8);

			if (sec.mRecordedAddressCRC != sec.mComputedAddressCRC)
				crc = ~crc;

			sechdr[8] = (uint8_t)(crc >> 8);
			sechdr[9] = (uint8_t)(crc >> 0);

			// first three bytes require special clocking, but are included in CRC
			enc.EncodeByteMFM(0xFB, 0xA1);
			enc.EncodeByteMFM(0xFB, 0xA1);
			enc.EncodeByteMFM(0xFB, 0xA1);

			for(int i=3; i<10; ++i)
				enc.EncodeByteMFM(sechdr[i]);

			for(int i=0; i<22; ++i)
				enc.EncodeByteMFM(0x4E);

			for(int i=0; i<12; ++i)
				enc.EncodeByteMFM(0x0D);

			// check if the data field is actually encoded
			if (sec.mAddressMark) {
				// write DAM
				enc.EncodeByteMFM(0xFB, 0xA1);
				enc.EncodeByteMFM(0xFB, 0xA1);
				enc.EncodeByteMFM(0xFB, 0xA1);
				enc.EncodeByteMFM(sec.mAddressMark);

				// write payload
				for(uint32_t i=0; i<sec.mSectorSize; ++i)
					enc.EncodeByteMFM(~sec.mData[i]);

				// compute and write CRC
				const uint8_t secdhdr[4] = {
					0xA1,
					0xA1,
					0xA1,
					sec.mAddressMark,
				};

				uint16_t crc2 = ComputeCRC(secdhdr, 4);

				crc2 = ComputeInvertedCRC(sec.mData, sec.mSectorSize, crc2);

				if (sec.mRecordedCRC != sec.mComputedCRC)
					crc2 = ~crc2;

				enc.EncodeByteMFM((uint8_t)(crc2 >> 8));
				enc.EncodeByteMFM((uint8_t)(crc2 >> 0));
			} else {
				// no data field -- write padding bytes
				for(int i=0; i<40; ++i)
					enc.EncodeByteMFM(0x0D);
			}

			// write trailer
			enc.EncodeByteMFM(0x4E);
			enc.EndCritical();

			for(int i=1; i<24; ++i)
				enc.EncodeByteMFM(0x4E);

			enc.FlushMFM();
		}
	} else {
		for(size_t i=0; i<numsecs; ++i) {
			const SectorInfo& sec = *sectors[i];
			SectorEncoder& enc = sector_encoders[i];
			enc.SetBitCellTime(bitCellTime);

			const bool first_sec = (&sec == lowest_sec);

			if (first_sec) {
				enc.BeginCritical();
				enc.EncodeByteFM(0x00);
				enc.EncodeByteFM(0xD7, 0xFC);
			}

			for(int j=0; j<4; ++j)
				enc.EncodeByteFM(0x00);

			if (!first_sec)
				enc.BeginCritical();
		
			enc.EncodeByteFM(0x00);
			enc.EncodeByteFM(0x00);
			enc.EncodeByteFM(0xC7, 0xFE);
		
			uint8_t sechdr[7]={
				0xFE,
				(uint8_t)track,
				(uint8_t)side,
				(uint8_t)sec.mIndex,
				0,
				0,
				0
			};

			switch(sec.mSectorSize) {
				case 128:
				default:
					sechdr[4] = 0;
					break;

				case 256:
					sechdr[4] = 1;
					break;

				case 512:
					sechdr[4] = 2;
					break;

				case 1024:
					sechdr[4] = 3;
					break;
			}

			uint16_t crc = ComputeCRC(sechdr, 5);

			// check if we should force an address CRC error
			if (sec.mRecordedAddressCRC != sec.mComputedAddressCRC) {
				crc = ~crc;
			}

			sechdr[5] = (uint8_t)(crc >> 8);
			sechdr[6] = (uint8_t)crc;

			for(int j=1; j<7; ++j)
				enc.EncodeByteFM(sechdr[j]);

			for(int j=0; j<17; ++j)
				enc.EncodeByteFM(0x00);

			if (sec.mAddressMark) {
				uint8_t secdat[1024 + 3];
				secdat[0] = sec.mAddressMark;

				for(uint32_t j=0; j<sec.mSectorSize; ++j)
					secdat[j+1] = ~sec.mData[j];

				secdat[sec.mSectorSize + 1] = (uint8_t)(sec.mRecordedCRC >> 8);
				secdat[sec.mSectorSize + 2] = (uint8_t)sec.mRecordedCRC;

				enc.EncodeByteFM(0xC7, secdat[0]);

				// If this sector has a CRC error AND it's a long sector, don't bother writing
				// out the full sector to save room on the track.
				if (sec.mComputedCRC != sec.mRecordedCRC && sec.mSectorSize > 128) {
					for(uint32_t j=1; j<131; ++j) {
						if (sec.mWeakOffset >= 0 && j >= (uint32_t)sec.mWeakOffset+1)
							enc.EncodeWeakByteFM();
						else
							enc.EncodeByteFM(secdat[j]);
					}
				} else {
					for(uint32_t j=1; j<sec.mSectorSize + 3; ++j) {
						if (sec.mWeakOffset >= 0 && j >= (uint32_t)sec.mWeakOffset+1) {
							enc.EncodeWeakByteFM();
						} else
							enc.EncodeByteFM(secdat[j]);
					}
				}
			} else {
				for(int j=0; j<50; ++j)
					enc.EncodeByteFM(0x00);
			}

			enc.EncodeByteFM(0x00);
			enc.EndCritical();

			for(int j=0; j<8; ++j)
				enc.EncodeByteFM(0x00);
		}
	}

	// Compute byte time. We align all sectors to bits since a format normally maintains byte
	// alignment for the address fields, and this is easier on the PLL. FM and MFM use two bit
	// cells per data bit, while GCR uses one bit cell per encoded bit.
	const uint32_t dataBitTime = bitCellTime * (a2gcr ? 1 : 2);

	// create copies of all sectors
	std::vector<SectorCopy> copies;

	uint32_t encodingPosition = 0;

	if (lowest_sec)
		encodingPosition = (uint32_t)(0.5 + lowest_sec->mPosition * 200000000.0 / 6.0 / (double)dataBitTime) * dataBitTime;

	for(size_t i=0; i<numsecs; ++i) {
		const SectorInfo& sec = *sectors[i];
		const SectorEncoder& enc = sector_encoders[i];

		// skip any unused sector encoders (this can be due to overlapping sectors)
		if (enc.mStream.empty())
			continue;

		for(int j=0; j<7; ++j) {
			// we round off the position to bits to make the bitstream cleaner and disturb
			// the receiving PLL less
			uint32_t position;
			
			if (precise)
				position = (uint32_t)(0.5 + (sec.mPosition + j) * 200000000.0 / 6.0 / (double)dataBitTime) * dataBitTime;
			else
				position = encodingPosition + (uint32_t)(0.5 + j * 200000000.0 / 6.0 / (double)dataBitTime) * dataBitTime;

			SectorCopy copy;

			copy.mpSector = &sec;
			copy.mpEncodedSector = &enc;
			copy.mPosition = position;
			copy.mEncodeStart = position;
			copy.mEncodeEnd = position + enc.mTime;

			A8RC_RT_ASSERT(enc.mTime >= enc.mCriticalEnd);

			copies.push_back(copy);

			if (!j && g_verbosity >= 1) {
				printf("Encoding track %2u, sector %2u at %.3f-%.3f (critical %.3f-%.3f)\n"
					, track
					, sec.mIndex
					, fmod(encodingPosition / (200000000.0 / 6.0), 1.0)
					, fmod((encodingPosition + enc.mTime) / (200000000.0 / 6.0), 1.0)
					, fmod((encodingPosition + enc.mCriticalStart) / (200000000.0 / 6.0), 1.0)
					, fmod((encodingPosition + enc.mCriticalEnd) / (200000000.0 / 6.0), 1.0));
			}
		}

		encodingPosition += enc.mTime;
	}

	// sort all copies by rotational position
	std::sort(copies.begin(), copies.end(), [](const SectorCopy& x, const SectorCopy& y) { return x.mPosition < y.mPosition; });

	// scan the copies and try to deal with overlaps
	std::set<std::pair<uint32_t, uint32_t>> reportedOverlaps;

	const size_t numcopies = copies.size();

	for(size_t i=1; i<numcopies; ++i) {
		SectorCopy& cp0 = copies[i-1];
		SectorCopy& cp1 = copies[i];

		if (cp0.mEncodeEnd > cp1.mEncodeStart) {
			uint32_t cut = cp1.mEncodeStart + ((cp0.mEncodeEnd - cp1.mEncodeStart) >> 1);
			uint32_t lo = cp0.mPosition + cp0.mpEncodedSector->mCriticalEnd;
			uint32_t hi = cp1.mPosition + cp1.mpEncodedSector->mCriticalStart;

			if (lo > hi) {
				if (reportedOverlaps.insert(std::make_pair((int)cp0.mpSector->mIndex, (int)cp1.mpSector->mIndex)).second) {
					printf("WARNING: Track %u, sectors %u and %u overlapped by %.1f bytes during encoding. Encoded track may not work.\n"
						, track
						, cp0.mpSector->mIndex
						, cp1.mpSector->mIndex
						, ((double)lo - (double)hi) / ((double)dataBitTime * 8)
					);
				}
			} else {
				if (cut < lo)
					cut = lo;
				else if (cut > hi)
					cut = hi;
			}

			// trim both sectors
			cp0.mEncodeEnd = cut;
			cp1.mEncodeStart = cut;
		}
	}

	// encode unified bitstream
	uint32_t time_last = 0;

	for(size_t i=0; i<numcopies; ++i) {
		const SectorCopy& cp = copies[i];
		uint32_t sector_start = cp.mEncodeStart;

		// if sector start is with second rev, mark halfway between it and the next sector as the splice point
		if (i > 0 && cp.mpSector == first_sec && cp.mPosition >= dst.mIndexTimes[1] && cp.mPosition < dst.mIndexTimes[2]) {
			dst.mSpliceStart = ((int32_t)copies[i-1].mPosition + (int32_t)cp.mPosition) / 2;
			dst.mSpliceEnd = dst.mSpliceStart + (dst.mIndexTimes[2] - dst.mIndexTimes[1]);

			if (g_verbosity >= 2) {
				printf("Using [%u, %u] as the splice points for track\n", dst.mSpliceStart, dst.mSpliceEnd);
			}
		}

		// encode 1 bits until the sector starts, while we have room
		if (mfm) {
			while(sector_start - time_last > bitCellTime * 2) {
				dst.mTransitions.push_back(time_last);
				time_last += bitCellTime * 2;
			}
		} else {
			while(sector_start - time_last > bitCellTime) {
				dst.mTransitions.push_back(time_last);
				time_last += bitCellTime;
			}
		}

		// determine the range to copy over from the original encoded stream
		uint32_t xfer_start = cp.mEncodeStart - cp.mPosition;
		uint32_t xfer_end = cp.mEncodeEnd - cp.mPosition;

		A8RC_RT_ASSERT(xfer_start < 0x80000000U);
		A8RC_RT_ASSERT(xfer_end <= cp.mpEncodedSector->mTime);

		if (g_verbosity >= 2)
			printf("Encoding %u-%u of sector %u (critical %u-%u) to %u-%u\n", xfer_start, xfer_end, cp.mpSector->mIndex, cp.mpEncodedSector->mCriticalStart, cp.mpEncodedSector->mCriticalEnd, cp.mEncodeStart, cp.mEncodeEnd);

		if (xfer_end > xfer_start) {
			const auto& src_stream = cp.mpEncodedSector->mStream;
			auto xfer1 = std::lower_bound(src_stream.begin(), src_stream.end(), xfer_start);
			auto xfer2 = std::lower_bound(xfer1, src_stream.end(), xfer_end);

			for(auto it = xfer1; it != xfer2; ++it)
				dst.mTransitions.push_back(cp.mPosition + *it);
		}

		time_last = cp.mEncodeEnd;
	}
}

void encode_disk(RawDisk& dst, DiskInfo& src, double periodMultiplier, int trackSelect, bool a2gcr, bool precise) {
	dst.mTrackCount = src.mTrackCount;
	dst.mTrackStep = src.mTrackStep;
	dst.mSideCount = src.mSideCount;
	dst.mSynthesized = true;

	for(int i=0; i<src.mTrackCount; ++i) {
		if (trackSelect >= 0 && trackSelect != i)
			continue;

		for(int j=0; j<src.mSideCount; ++j) {
			if (g_verbosity >= 1) {
				if (src.mSideCount > 1)
					printf("Encoding track %u, side %u\n", i, j);
				else
					printf("Encoding track %u\n", i);
			}

			encode_track(dst.mPhysTracks[j][i * src.mTrackStep], src.mPhysTracks[j][i * src.mTrackStep], i, j, periodMultiplier, a2gcr, precise);
		}
	}
}
