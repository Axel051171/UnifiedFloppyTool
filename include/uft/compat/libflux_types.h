/**
 * @file libflux_types.h
 * @brief Centralized LibFlux Type Definitions
 */
#ifndef LIBFLUX_TYPES_H
#define LIBFLUX_TYPES_H

#include <stdint.h>

#ifndef _LIBFLUX_SECTCFG_
#define _LIBFLUX_SECTCFG_

typedef struct _LIBFLUX_SECTCFG {
    int32_t  cylinder;
    int32_t  head;
    int32_t  sector;
    int32_t  sectorsize;
    uint32_t bitrate;
    uint32_t trackencoding;
    
    int32_t  gap3;
    int32_t  use_alternate_sector_size_id;
    int32_t  alternate_sector_size_id;
    
    uint8_t *input_data;
    int32_t  input_data_size;
    
    uint8_t *weak_bits_mask;
    
    uint8_t  fill_byte;
    uint8_t  fill_byte_used;
    
    uint32_t flags;
} LIBFLUX_SECTCFG;

#endif /* _LIBFLUX_SECTCFG_ */

#ifndef _LIBFLUX_SIDE_
#define _LIBFLUX_SIDE_

typedef struct _LIBFLUX_SIDE {
    uint32_t number_of_sector;
    uint32_t tracklen;
    uint8_t *databuffer;
    uint8_t *flakybitsbuffer;
    uint8_t *indexbuffer;
    uint32_t bitrate;
    uint32_t track_encoding;
    LIBFLUX_SECTCFG *sectorcfg;
} LIBFLUX_SIDE;

#endif /* _LIBFLUX_SIDE_ */

#endif /* LIBFLUX_TYPES_H */
