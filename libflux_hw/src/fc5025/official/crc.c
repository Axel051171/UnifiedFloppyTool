#include "crctable.h"

/* 16 bit crc, poly=0x1021 (x^16+x^12+x^5+1), iv=0xffff */
/* test vector: 0xfe 0x00 0x00 0x07 0x01 -> output value 0x6844 */

static unsigned short crc;

void crc_init(void) {
	crc=0xffff;
}

void crc_add(unsigned char c) {
	crc=(crc<<8)^crctable[((crc>>8)^c)&0xff];
}

void crc_add_block(unsigned char *buf, int count) {
	while(count--)
		crc_add(*buf++);
}

unsigned short crc_get(void) {
	return crc;
}

unsigned short crc_block(unsigned char *buf, int count) {
	crc_init();
	crc_add_block(buf,count);
	return crc;
}

void crc_append(unsigned char *buf, int count) {
	crc_init();
	while(count--)
		crc_add(*buf++);
	*buf++=crc>>8;
	*buf=crc&0xff;
}
