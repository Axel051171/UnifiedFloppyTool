#ifndef UFT_ALGORITHMS_TRACKS_TRACKUTILS_H
#define UFT_ALGORITHMS_TRACKS_TRACKUTILS_H

/*
 * HxC Floppy Emulator - Track Utilities
 * Copyright (C) 2006-2025 Jean-Francois DEL NERO
 * GPL v2+
 */

#include <stdint.h>

/* getbit/setbit provided by floppy_utils.h or libflux_compat.h */
#ifndef HAVE_GETBIT
#include "uft/PRIVATE/compat/floppy_utils.h"
#endif

void setfieldbit(unsigned char * dstbuffer,unsigned char byte,int bitoffset,int size);

int  searchBitStream(unsigned char * input_data, int input_data_size, int searchlen,
                     unsigned char * chr_data, int chr_data_size, unsigned int bit_offset);

int slowSearchBitStream(unsigned char * input_data, int input_data_size, int searchlen,
                        unsigned char * chr_data, int chr_data_size, unsigned int bit_offset);

void sortbuffer(unsigned char * buffer,unsigned char * outbuffer,int size);

int chgbitptr(int tracklen,int cur_offset,int offset);
int calcbitptrdist(int tracklen,int first_offset,int last_offset);

#endif /* UFT_ALGORITHMS_TRACKS_TRACKUTILS_H */
