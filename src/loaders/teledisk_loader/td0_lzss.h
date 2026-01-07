#ifndef UFT_LOADERS_TELEDISK_LOADER_TD0_LZSS_H
#define UFT_LOADERS_TELEDISK_LOADER_TD0_LZSS_H

void init_decompress();
unsigned char * unpack(unsigned char *packeddata,unsigned int size, unsigned int * unpacked_size);



#endif /* UFT_LOADERS_TELEDISK_LOADER_TD0_LZSS_H */
