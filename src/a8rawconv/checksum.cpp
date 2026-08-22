#include "stdafx.h"

uint16_t ComputeCRC(const uint8_t *buf, size_t len, uint16_t initialCRC) {
	// x^16 + x^12 + x^5 + 1
	uint16_t crc = initialCRC;

	for(uint32_t i=0; i<len; ++i) {
		uint8_t c = buf[i];

		crc ^= (uint16_t)c << 8;

		for(int j=0; j<8; ++j) {
			uint16_t xorval = (crc & 0x8000) ? 0x1021 : 0;

			crc += crc;

			crc ^= xorval;
		}
	}

	return crc;
}

uint16_t ComputeInvertedCRC(const uint8_t *buf, size_t len, uint16_t initialCRC) {
	// x^16 + x^12 + x^5 + 1
	uint16_t crc = initialCRC;

	for(uint32_t i=0; i<len; ++i) {
		uint8_t c = ~buf[i];

		crc ^= (uint16_t)c << 8;

		for(int j=0; j<8; ++j) {
			uint16_t xorval = (crc & 0x8000) ? 0x1021 : 0;

			crc += crc;

			crc ^= xorval;
		}
	}

	return crc;
}

uint32_t ComputeByteSum(const void *buf, size_t len) {
	const uint8_t *src = (const uint8_t *)buf;

	uint32_t chk = 0;
	while(len--) {
		chk += *src++;
	}

	return chk;
}

uint16_t ComputeAddressCRC(uint32_t track, uint32_t side, uint32_t sector, uint32_t sectorSize, bool mfm) {
	uint8_t data[]={
		0xA1,
		0xA1,
		0xA1,
		0xFE,
		(uint8_t)track,
		(uint8_t)side,
		(uint8_t)sector,
		(uint8_t)(sectorSize > 512 ? 3 : sectorSize > 256 ? 2 : sectorSize > 128 ? 1 : 0)
	};

	return mfm ? ComputeCRC(data, 8) : ComputeCRC(data + 3, 5);
}
