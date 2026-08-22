#include "stdafx.h"

SectorParser::SectorParser()
	: mReadPhase(0)
	, mBitPhase(0)
{
}

void SectorParser::Init(int track, const std::vector<uint32_t> *indexTimes, float samplesPerCell, TrackInfo *dstTrack, uint32_t streamTime) {
	mTrack = track;
	mpIndexTimes = indexTimes;
	mSamplesPerCell = samplesPerCell;
	mpDstTrack = dstTrack;
	mRawStart = streamTime;
}

bool SectorParser::Parse(uint32_t stream_time, uint8_t clock_bits, uint8_t data_bits) {
	if (mReadPhase < 6) {
		if (++mBitPhase == 16) {
			mBitPhase = 0;

			if (clock_bits != 0xFF)
				return false;

			mBuf[++mReadPhase] = data_bits;

			if (mReadPhase == 6) {
				if (mBuf[1] != mTrack) {
					//printf("Track number mismatch!\n");
					return false;
				}

				// Byte 3 of the address mark is the side indicator, which is supposed to be
				// zero for the 1771. It appears not to validate it, because Rescue on Fractalus
				// has garbage in that field.
#if 0
				if (mBuf[2] != 0) {
					printf("Zero #1 failed! (got %02X)\n", mBuf[2]);
					return false;
				}
#endif

				if (mBuf[3] < 1 || mBuf[3] > 18) {
					printf("Invalid sector number\n");
					return false;
				}

				mBuf[0] = 0xFE;
				const uint16_t computedCRC = ComputeCRC(mBuf, 5);
				const uint16_t recordedCRC = ((uint16_t)mBuf[5] << 8) + mBuf[6];

				mSector = mBuf[3];

				// Only the low two bits are used. Fight Night (Accolade) abuses this.
				mSectorSize = 128 << (mBuf[4] & 3);

				int vsn_time = stream_time;

				// find the nearest index mark
				auto it_index = std::upper_bound(mpIndexTimes->begin(), mpIndexTimes->end(), (uint32_t)vsn_time + 1);

				if (it_index == mpIndexTimes->begin()) {
					if (g_verbosity >= 2)
						printf("Skipping track %d, sector %d before first index mark\n", mTrack, mSector);
					return false;
				}

				if (it_index == mpIndexTimes->end()) {
					if (g_verbosity >= 2)
						printf("Skipping track %d, sector %d after last index mark\n", mTrack, mSector);
					return false;
				}

				int vsn_offset = vsn_time - *--it_index;

				mRotStart = it_index[0];
				mRotEnd = it_index[1];

				mRotPos = (float)vsn_offset / (float)(it_index[1] - it_index[0]);
				mRotPos -= floorf(mRotPos);

				if (g_verbosity >= 2)
					printf("Found track %d, sector %d at position %4.2f\n", mTrack, mSector, mRotPos);

				mRecordedAddressCRC = recordedCRC;
				mComputedAddressCRC = computedCRC;

				if (computedCRC != recordedCRC) {
					printf("Found track %d, sector %d with bad address CRC: %04X != %04X\n", mTrack, mSector, computedCRC, recordedCRC);

					auto& tracksecs = mpDstTrack->mSectors;
					tracksecs.emplace_back();
					SectorInfo& newsec = tracksecs.back();

					for(int i=0; i<mSectorSize; ++i)
						newsec.mData[i] = 0;

					newsec.mIndex = mSector;
					newsec.mRawStart = mRawStart;
					newsec.mRawEnd = stream_time;
					newsec.mPosition = mRotPos;
					newsec.mEndingPosition = (float)(stream_time - mRotStart) / (float)(mRotEnd - mRotStart);
					newsec.mEndingPosition -= floorf(newsec.mEndingPosition);
					newsec.mAddressMark = 0xFB;
					newsec.mRecordedAddressCRC = recordedCRC;
					newsec.mComputedAddressCRC = computedCRC;
					newsec.mRecordedCRC = 0;
					newsec.mComputedCRC = 0;
					newsec.mSectorSize = mSectorSize;
					newsec.mWeakOffset = -1;
					newsec.mbMFM = false;
					return false;
				}

				// The WD1772 documentation says that the DAM must appear within 30 bytes. We count both
				// bytes and time; for a safety margin we allow 20% extra on the time part.
				mDAMBitCounter = 30 * 16 + 1;
				mDAMMinTime = stream_time + (uint32_t)(11 * 16 * mSamplesPerCell);
				mDAMTimeoutTime = stream_time + (uint32_t)(30 * 20 * mSamplesPerCell);
			}
		}
	} else if (mReadPhase == 6) {
		if (!--mDAMBitCounter || stream_time - mDAMTimeoutTime < 0x80000000U) {
			if (g_verbosity >= 2)
				printf("FM track %d, sector %d: timeout while searching for DAM\n", mTrack, mSector);
			return false;
		}

		++mBitPhase;

		if (stream_time - mDAMMinTime >= 0x8000000U)
			return true;
			
		if (clock_bits == 0xC7) {
			// another IDAM detected before DAM -- terminate
			// UPDATE: We no longer do this check because it breaks Blue Max, which has interleaved
			// IDAM and DAM markers. The WD1772 documentation says that the DAM must come within
			// 30 bytes (FM) or 43 bytes (DD).
			//
			//if (data_bits == 0xFE)
			//	return false;

			if (data_bits == 0xF8 || data_bits == 0xF9 || data_bits == 0xFA || data_bits == 0xFB) {
				if (g_verbosity >= 2)
					printf("DAM detected (%02X)\n", data_bits);

				mReadPhase = 7;
				mBitPhase = 0;
				mBuf[0] = data_bits;
				mClockBuf[0] = clock_bits;
				mStreamTimes[0] = stream_time;
			}
		}
	} else {
		if (++mBitPhase == 16) {
			if (clock_bits != 0xFF) {
				if (g_verbosity > 1)
					printf("Bad data clock: %02X\n", clock_bits);
			}

//			printf("Data; %02X\n", ~data_bits);
			mBuf[mReadPhase - 6] = data_bits;
			mClockBuf[mReadPhase - 6] = clock_bits;
			mStreamTimes[mReadPhase - 6] = stream_time;

			mBitPhase = 0;
			if (++mReadPhase >= mSectorSize + 3 + 6) {
				// check if the CRC is correct
				// x^16 + x^12 + x^5 + 1
				uint16_t crc = ComputeCRC(mBuf, mSectorSize + 1);

				const uint16_t recordedCRC = ((uint16_t)mBuf[mSectorSize + 1] << 8) + (uint8_t)mBuf[mSectorSize + 2];

				//printf("Read sector %d: CRCs %04X, %04X\n", mSector, crc, recordedCRC);

				// add new sector entry
				auto& tracksecs = mpDstTrack->mSectors;
				tracksecs.emplace_back();
				SectorInfo& newsec = tracksecs.back();

				for(int i=0; i<mSectorSize; ++i)
					newsec.mData[i] = ~mBuf[i+1];

				newsec.mIndex = mSector;
				newsec.mRawStart = mRawStart;
				newsec.mRawEnd = stream_time;
				newsec.mPosition = mRotPos;
				newsec.mEndingPosition = (float)(stream_time - mRotStart) / (float)(mRotEnd - mRotStart);
				newsec.mEndingPosition -= floorf(newsec.mEndingPosition);
				newsec.mAddressMark = mBuf[0];
				newsec.mRecordedAddressCRC = mRecordedAddressCRC;
				newsec.mComputedAddressCRC = mComputedAddressCRC;
				newsec.mRecordedCRC = recordedCRC;
				newsec.mComputedCRC = crc;
				newsec.mSectorSize = mSectorSize;
				newsec.mWeakOffset = -1;
				newsec.mbMFM = false;

				if (g_verbosity >= 1 || (crc != recordedCRC && g_dumpBadSectors)) {
					// Compute end position. We may end up extrapolating here, but that's fine.
					auto it_index = std::upper_bound(mpIndexTimes->begin(), mpIndexTimes->end(), (uint32_t)stream_time + 1);
					float endPos = mRotPos;

					if (it_index != mpIndexTimes->begin())
						--it_index;

					int vsn_offset = stream_time - it_index[0];

					endPos = (float)vsn_offset / (float)(it_index[1] - it_index[0]);
					endPos -= floorf(endPos);

					if (recordedCRC == crc) {
						printf("Decoded FM track %d, sector %2d: %u bytes, pos %5.3f-%5.3f, DAM %02X, CRC %04X (OK)\n",
							mTrack,
							mSector,
							mSectorSize,
							mRotPos,
							endPos,
							mBuf[0],
							recordedCRC);
					} else {
						printf("Decoded FM track %d, sector %2d: %u bytes, pos %5.3f-%5.3f, DAM %02X, CRC %04X (bad -- computed %04X)\n",
							mTrack,
							mSector,
							mSectorSize,
							mRotPos,
							endPos,
							mBuf[0],
							recordedCRC,
							crc);
					}
				}
			
				if (g_dumpBadSectors && crc != recordedCRC) {
					printf("  Index Clk Data Cells\n");
					for(int i=0; i<mSectorSize + 1; ++i) {
						printf("  %4d  %02X | %02X (%02X,%02X %02X %02X %02X %02X %02X %02X) | %+6.1f%s\n"
							, i - 1
							, mClockBuf[i]
							, mBuf[i]
							, 0xFF ^ mBuf[i]
							, 0xFF ^ (((mBuf[i] << 1) + (mBuf[i+1]>>7)) & 0xFF)
							, 0xFF ^ (((mBuf[i] << 2) + (mBuf[i+1]>>6)) & 0xFF)
							, 0xFF ^ (((mBuf[i] << 3) + (mBuf[i+1]>>5)) & 0xFF)
							, 0xFF ^ (((mBuf[i] << 4) + (mBuf[i+1]>>4)) & 0xFF)
							, 0xFF ^ (((mBuf[i] << 5) + (mBuf[i+1]>>3)) & 0xFF)
							, 0xFF ^ (((mBuf[i] << 6) + (mBuf[i+1]>>2)) & 0xFF)
							, 0xFF ^ (((mBuf[i] << 7) + (mBuf[i+1]>>1)) & 0xFF)
							, i ? (float)(mStreamTimes[i] - mStreamTimes[i-1]) / mSamplesPerCell : 0
							, i && mClockBuf[i] != 0xFF ? " <!>" : ""
							);
					}
				}

				return false;
			}
		}
	}

	return true;
}

///////////////////////////////////////////////////////////////////////////

SectorParserMFM::SectorParserMFM()
	: mReadPhase(0)
	, mBitPhase(0)
{
}

void SectorParserMFM::Init(int track, int side, const std::vector<uint32_t> *indexTimes, float samplesPerCell, TrackInfo *dstTrack, uint32_t streamTime) {
	mTrack = track;
	mSide = side;
	mpIndexTimes = indexTimes;
	mSamplesPerCell = samplesPerCell;
	mpDstTrack = dstTrack;
	mRawStart = streamTime;
}

bool SectorParserMFM::Parse(uint32_t stream_time, uint8_t clock_bits, uint8_t data_bits) {
	if (mReadPhase < 7) {
		if (++mBitPhase == 16) {
			mBitPhase = 0;

			mBuf[mReadPhase+3] = data_bits;
			++mReadPhase;

			if (mReadPhase == 7) {
				if (mBuf[3] != 0xFE)
					return false;

				if (mBuf[4] != mTrack) {
					printf("Track number mismatch on track %d.%d: %02X != %02X\n", mTrack, mSide, mBuf[4], mTrack);
					return false;
				}

				mSectorSize = 128 << (mBuf[7] & 3);

				mBuf[0] = 0xA1;
				mBuf[1] = 0xA1;
				mBuf[2] = 0xA1;
				const uint16_t computedCRC = ComputeCRC(mBuf, 8);
				const uint16_t recordedCRC = ((uint16_t)mBuf[8] << 8) + mBuf[9];

				mRecordedAddressCRC = recordedCRC;
				mComputedAddressCRC = computedCRC;

				if (computedCRC != recordedCRC) {
					printf("CRC failure on sector header: %04X != %04X\n", computedCRC, recordedCRC);
					return false;
				}

				mSector = mBuf[6];

				int vsn_time = stream_time;

				// find the nearest index mark
				auto it_index = std::upper_bound(mpIndexTimes->begin(), mpIndexTimes->end(), (uint32_t)vsn_time + 1);

				if (it_index == mpIndexTimes->begin()) {
					if (g_verbosity >= 2)
						printf("Skipping track %d, sector %d before first index mark\n", mTrack, mSector);
					return false;
				}

				if (it_index == mpIndexTimes->end()) {
					if (g_verbosity >= 2)
						printf("Skipping track %d, sector %d after last index mark\n", mTrack, mSector);
					return false;
				}

				int vsn_offset = vsn_time - *--it_index;

				mRotStart = it_index[0];
				mRotEnd = it_index[1];

				mRotPos = (float)vsn_offset / (float)(it_index[1] - it_index[0]);

				if (mRotPos >= 1.0f)
					mRotPos -= 1.0f;

				if (g_verbosity >= 2)
					printf("Found track %d, sector %d at position %4.2f\n", mTrack, mSector, mRotPos);
			}
		}
	} else if (mReadPhase == 7) {
//		if (clock_bits == 0x0A && data_bits != 0xA1)
		if ((clock_bits & 0x7F) == 0x0A && data_bits == 0xA1) {
			++mReadPhase;
		}
	} else if (mReadPhase == 8 || mReadPhase == 9) {
		if ((clock_bits & 0x7F) == 0x0A) {
			if (data_bits != 0xA1) {
				mReadPhase = 7;
				return true;
			}

			++mReadPhase;
			mBitPhase = 0;
			mBuf[0] = data_bits;
			mBuf[1] = data_bits;
			mBuf[2] = data_bits;
		}
	} else if (mReadPhase == 10) {
		if (++mBitPhase == 16) {
			if (clock_bits == 0x0A && data_bits == 0xA1) {
				mBitPhase = 0;
			} else {
				if (data_bits == 0xF8 || data_bits == 0xF9 || data_bits == 0xFA || data_bits == 0xFB) {
					//printf("DAM detected (%02X)\n", data_bits);
					++mReadPhase;
					mBitPhase = 0;
					mBuf[3] = data_bits;
				} else
					return false;
			}
		}
	} else {
		if (++mBitPhase == 16) {
//			printf("Data; %02X\n", ~data_bits);
			mBuf[mReadPhase - 7] = data_bits;

			mBitPhase = 0;
			if (++mReadPhase >= 7 + mSectorSize + 6) {
				// check if the CRC is correct
				// x^16 + x^12 + x^5 + 1
				uint16_t crc = ComputeCRC(mBuf, mSectorSize + 4);

				const uint16_t recordedCRC = ((uint16_t)mBuf[mSectorSize + 4] << 8) + (uint8_t)mBuf[mSectorSize + 5];

				//printf("Read sector %d: CRCs %04X, %04X\n", mSector, crc, recordedCRC);

				// add new sector entry
				auto& tracksecs = mpDstTrack->mSectors;
				tracksecs.push_back(SectorInfo());
				SectorInfo& newsec = tracksecs.back();

				for(int i=0; i<mSectorSize; ++i)
					newsec.mData[i] = ~mBuf[i+4];

				newsec.mIndex = mSector;
				newsec.mRawStart = mRawStart;
				newsec.mRawEnd = stream_time;
				newsec.mPosition = mRotPos;
				newsec.mEndingPosition = (float)(stream_time - mRotStart) / (float)(mRotEnd - mRotStart);
				newsec.mEndingPosition -= floorf(newsec.mEndingPosition);
				newsec.mAddressMark = mBuf[3];
				newsec.mRecordedAddressCRC = mRecordedAddressCRC;
				newsec.mComputedAddressCRC = mComputedAddressCRC;
				newsec.mRecordedCRC = recordedCRC;
				newsec.mComputedCRC = crc;
				newsec.mSectorSize = mSectorSize;
				newsec.mbMFM = true;
				newsec.mWeakOffset = -1;

				if (g_verbosity >= 1)
					printf("Decoded MFM track %2d, sector %2d with %u bytes, DAM %02X, recorded CRC %04X (computed %04X) [pos %.3f-%.3f]\n",
						mTrack,
						mSector,
						mSectorSize,
						mBuf[3],
						recordedCRC,
						crc,
						newsec.mPosition,
						newsec.mEndingPosition);

				return false;
			}
		}
	}

	return true;
}


///////////////////////////////////////////////////////////////////////////

const uint8_t SectorParserMFMAmiga::kSpaceTable[16]={
	0x00, 0x01, 0x04, 0x05,
	0x10, 0x11, 0x14, 0x15,
	0x40, 0x41, 0x44, 0x45,
	0x50, 0x51, 0x54, 0x55,
};

void SectorParserMFMAmiga::Init(int cylinder, int head, const std::vector<uint32_t> *indexTimes, float samplesPerCell, TrackInfo *dstTrack, uint32_t streamTime) {
	mCylinder = cylinder;
	mHead = head;
	mpIndexTimes = indexTimes;
	mSamplesPerCell = samplesPerCell;
	mpDstTrack = dstTrack;
	mRawStart = streamTime;
}

bool SectorParserMFMAmiga::Parse(uint32_t stream_time, uint8_t clock_bits, uint8_t data_bits) {
	// What we are looking for:
	//	sync A1 ($4489) (already parsed for us)
	//	sync A1 ($4489) (already parsed for us)
	//	format byte $FF
	//	track number (0-159)
	//	sector number (0-10)
	//	sectors to end of write (0-10)
	//	16 bytes of recovery info
	//	longword - header checksum
	//	longword - data checksum
	//	512 bytes of sector data
	//
	// There aren't separate address/data frames, so we simply stream in 540 bytes and periodically
	// peek at it for validity once in a while.

	if (++mBitPhase < 16)
		return true;

	mBitPhase = 0;

	mBuf[mReadPhase] = data_bits;
	++mReadPhase;

	switch(mReadPhase) {
		case 4:
		{
			const uint32_t addressInfo
				= kSpaceTable[mBuf[3] & 15]
				+ (kSpaceTable[mBuf[3] >> 4] << 8)
				+ (kSpaceTable[mBuf[1] & 15] << 1)
				+ (kSpaceTable[mBuf[1] >> 4] << 9)
				+ (kSpaceTable[mBuf[2] & 15] << 16)
				+ (kSpaceTable[mBuf[2] >> 4] << 24)
				+ (kSpaceTable[mBuf[0] & 15] << 17)
				+ (kSpaceTable[mBuf[0] >> 4] << 25);

			uint8_t format = (uint8_t)(addressInfo >> 24);
			uint8_t track = (uint8_t)(addressInfo >> 16);
			uint8_t sector = (uint8_t)(addressInfo >> 8);

			if (format != 0xFF || track != mCylinder * 2 + mHead || sector >= 11)
				return false;

			mSector = sector;
			break;
		}

		case 24: {
			// The Amiga checksum is a longword XOR sum on MFM longwords of the data, which
			// is then encoded into MFM. This is a really odd way to do it because it means
			// that only even bits are ever set in the checksum, which is then split into
			// odd/even pairs for MFM encoding... which means that the odd word is always
			// $0000. Oh well.
			uint8_t chk0 = 0;
			uint8_t chk1 = 0;

			for(int i=0; i<22; i += 2) {
				chk0 ^= mBuf[i];
				chk1 ^= mBuf[i+1];
			}

			uint32_t computedSum = ((uint32_t)chk0 << 8) + ((uint32_t)chk1 << 0);
			uint32_t receivedSum = ((uint32_t)mBuf[20] << 24) + ((uint32_t)mBuf[21] << 16) + ((uint32_t)mBuf[22] << 8) + mBuf[23];
			
			if (computedSum != receivedSum) {
				printf("Checksum failure on sector header: %08X != %08X\n", computedSum, receivedSum);
				return false;
			}

			int vsn_time = stream_time;

			// find the nearest index mark
			auto it_index = std::upper_bound(mpIndexTimes->begin(), mpIndexTimes->end(), (uint32_t)vsn_time + 1);

			if (it_index == mpIndexTimes->begin()) {
				if (g_verbosity >= 2)
					printf("Skipping track %d.%d, sector %d before first index mark\n", mCylinder, mHead, mSector);
				return false;
			}

			if (it_index == mpIndexTimes->end()) {
				if (g_verbosity >= 2)
					printf("Skipping track %d.%d, sector %d after last index mark\n", mCylinder, mHead, mSector);
				return false;
			}

			int vsn_offset = vsn_time - *--it_index;

			mRotStart = it_index[0];
			mRotEnd = it_index[1];

			mRotPos = (float)vsn_offset / (float)(it_index[1] - it_index[0]);

			if (mRotPos >= 1.0f)
				mRotPos -= 1.0f;

			if (g_verbosity >= 2)
				printf("Found track %d.%d, sector %d at position %4.2f\n", mCylinder, mHead, mSector, mRotPos);

			break;
		}

		case 540: {
			// recompute data checksum
			uint8_t chk0 = 0;
			uint8_t chk1 = 0;

			for(int i=0; i<512; i += 2) {
				chk0 ^= mBuf[i+28];
				chk1 ^= mBuf[i+29];
			}

			uint32_t computedSum = ((uint32_t)chk0 << 8) + (uint32_t)chk1;
			uint32_t recordedSum = ((uint32_t)mBuf[24] << 24) + ((uint32_t)mBuf[25] << 16) + ((uint32_t)mBuf[26] << 8) + mBuf[27];

			// add new sector entry
			auto& tracksecs = mpDstTrack->mSectors;
			tracksecs.emplace_back();
			SectorInfo& newsec = tracksecs.back();

			for(int i=0; i<256; ++i) {
				newsec.mData[i*2]
					=  kSpaceTable[mBuf[i + 284] >> 4]
					+ (kSpaceTable[mBuf[i +  28] >> 4] << 1);

				newsec.mData[i*2+1]
					=  kSpaceTable[mBuf[i + 284] & 15]
					+ (kSpaceTable[mBuf[i +  28] & 15] << 1);
			}

			newsec.mIndex = mSector;
			newsec.mRawStart = mRawStart;
			newsec.mRawEnd = stream_time;
			newsec.mPosition = mRotPos;
			newsec.mEndingPosition = (float)(stream_time - mRotStart) / (float)(mRotEnd - mRotStart);
			newsec.mEndingPosition -= floorf(newsec.mEndingPosition);
			newsec.mAddressMark = mBuf[3];
			newsec.mRecordedAddressCRC = 0;
			newsec.mComputedAddressCRC = 0;
			newsec.mRecordedCRC = recordedSum;
			newsec.mComputedCRC = computedSum;
			newsec.mSectorSize = 512;
			newsec.mbMFM = true;
			newsec.mWeakOffset = -1;

			if (g_verbosity >= 1)
				printf("Decoded Amiga track %2d.%d, sector %2d with recorded checksum %08X (computed %08X) [pos %.3f-%.3f]\n",
					mCylinder,
					mHead,
					mSector,
					recordedSum,
					computedSum,
					newsec.mPosition,
					newsec.mEndingPosition);

			return false;
		}
	}

	return true;
}
