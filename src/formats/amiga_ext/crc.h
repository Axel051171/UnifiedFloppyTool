#ifndef UFT_FORMATS_AMIGA_EXT_CRC_H
#define UFT_FORMATS_AMIGA_EXT_CRC_H

unsigned short crc16( unsigned char data, unsigned short crc16 );
unsigned short crc16_buf(unsigned char * buffer,int size,unsigned short crc16);

#endif /* UFT_FORMATS_AMIGA_EXT_CRC_H */
