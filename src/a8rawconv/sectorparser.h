#ifndef f_SECTORPARSER_H
#define f_SECTORPARSER_H

class SectorParser {
public:
	SectorParser();

	void Init(int track, const std::vector<uint32_t> *indexTimes, float samplesPerCell, TrackInfo *dstTrack, uint32_t streamTime);

	bool Parse(uint32_t stream_time, uint8_t clock_bits, uint8_t data_bits);

protected:
	TrackInfo *mpDstTrack;
	int mTrack;
	int mSector;
	int mSectorSize;
	int mReadPhase;
	int mBitPhase;
	int mDAMBitCounter;
	uint32_t mDAMMinTime;
	uint32_t mDAMTimeoutTime;
	uint32_t mRawStart;
	uint16_t mComputedAddressCRC;
	uint16_t mRecordedAddressCRC;
	float mRotPos;
	float mSamplesPerCell;

	uint32_t mRotStart;
	uint32_t mRotEnd;

	uint8_t mBuf[1024 + 4];
	uint8_t mClockBuf[1024 + 4];
	uint32_t mStreamTimes[1024 + 4];

	const std::vector<uint32_t> *mpIndexTimes;
};

class SectorParserMFM {
public:
	SectorParserMFM();

	void Init(int track, int side, const std::vector<uint32_t> *indexTimes, float samplesPerCell, TrackInfo *dstTrack, uint32_t streamTime);

	bool Parse(uint32_t stream_time, uint8_t clock_bits, uint8_t data_bits);

protected:
	TrackInfo *mpDstTrack;
	int mTrack;
	int mSide;
	int mSector;
	int mSectorSize;
	int mReadPhase;
	int mBitPhase;
	uint32_t mRawStart;
	uint16_t mComputedAddressCRC;
	uint16_t mRecordedAddressCRC;
	float mRotPos;
	uint32_t mRotStart;
	uint32_t mRotEnd;
	float mSamplesPerCell;

	uint8_t mBuf[1024 + 3];

	const std::vector<uint32_t> *mpIndexTimes;
};

class SectorParserMFMAmiga {
public:
	void Init(int track, int side, const std::vector<uint32_t> *indexTimes, float samplesPerCell, TrackInfo *dstTrack, uint32_t streamTime);

	bool Parse(uint32_t stream_time, uint8_t clock_bits, uint8_t data_bits);

protected:
	TrackInfo *mpDstTrack = nullptr;
	int mCylinder = 0;
	int mHead = 0;
	int mSector = 0;
	int mReadPhase = 0;
	int mBitPhase = 0;
	uint32_t mRawStart = 0;
	float mRotPos = 0;
	uint32_t mRotStart = 0;
	uint32_t mRotEnd = 0;
	float mSamplesPerCell = 0;

	uint8_t mBuf[540] = {};

	const std::vector<uint32_t> *mpIndexTimes = nullptr;

	static const uint8_t kSpaceTable[];
};

#endif
