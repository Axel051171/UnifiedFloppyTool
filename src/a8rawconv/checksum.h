#ifndef f_CHECKSUM_H
#define f_CHECKSUM_H

uint16_t ComputeCRC(const uint8_t *buf, size_t len, uint16_t initialCRC = 0xFFFF);
uint16_t ComputeInvertedCRC(const uint8_t *buf, size_t len, uint16_t initialCRC = 0xFFFF);
uint32_t ComputeByteSum(const void *buf, size_t len);

uint16_t ComputeAddressCRC(uint32_t track, uint32_t side, uint32_t sector, uint32_t sectorSize, bool mfm);

#endif
