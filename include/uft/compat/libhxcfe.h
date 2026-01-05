/**
 * @file libhxcfe.h
 * @brief HxCFloppyEmulator Compatibility Layer for UFT
 * 
 * Provides type definitions and macros compatible with HxCFE library
 * to allow integration of HxC-derived code into UFT.
 * 
 * @note This is a compatibility shim, not the full HxCFE library
 */

#ifndef LIBHXCFE_H
#define LIBHXCFE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ============================================================================
 * Return Codes
 * ============================================================================ */

#define HXCFE_NOERROR           0
#define HXCFE_ACCESSERROR      -1
#define HXCFE_BADFILE          -2
#define HXCFE_BADPARAMETER     -3
#define HXCFE_INTERNALERROR    -4
#define HXCFE_UNSUPPORTEDFILE  -5
#define HXCFE_VALIDFILE         1

/* ============================================================================
 * Track/Sector Types
 * ============================================================================ */

#define ISOIBM_MFM_ENCODING     0x00
#define AMIGA_MFM_ENCODING      0x01
#define ISOIBM_FM_ENCODING      0x02
#define EMU_FM_ENCODING         0x03
#define TYCOM_FM_ENCODING       0x04
#define MEMBRAIN_MFM_ENCODING   0x05
#define APPLEII_GCR1_ENCODING   0x06
#define APPLEII_GCR2_ENCODING   0x07
#define APPLEII_HDDD_A2_ENCODING 0x08
#define ARBURGDAT_ENCODING      0x09
#define ARBURGSYS_ENCODING      0x0A
#define AED6200P_MFM_ENCODING   0x0B
#define NORTHSTAR_HS_MFM_ENCODING 0x0C
#define HEATHKIT_HS_FM_ENCODING 0x0D
#define DEC_RX02_M2FM_ENCODING  0x0E
#define APPLEMAC_GCR_ENCODING   0x0F
#define QD_MO5_ENCODING         0x10
#define C64_GCR_ENCODING        0x11
#define VICTOR9K_GCR_ENCODING   0x12
#define MICRALN_HS_FM_ENCODING  0x13
#define CENTURION_MFM_ENCODING  0x14
#define UNKNOWN_ENCODING        0xFF

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct HXCFE_ HXCFE;
typedef struct HXCFE_FLOPPY_ HXCFE_FLOPPY;
typedef struct HXCFE_CYLINDER_ HXCFE_CYLINDER;
typedef struct HXCFE_SIDE_ HXCFE_SIDE;
typedef struct HXCFE_IMGLDR_ HXCFE_IMGLDR;
typedef struct HXCFE_IMGLDR_FILEINFOS_ HXCFE_IMGLDR_FILEINFOS;
typedef struct HXCFE_SECTCFG_ HXCFE_SECTCFG;
typedef struct HXCFE_SECTORACCESS_ HXCFE_SECTORACCESS;
typedef struct HXCFE_TDCFG_ HXCFE_TDCFG;

/* ============================================================================
 * Core Structures
 * ============================================================================ */

struct HXCFE_ {
    void* userdata;
    int envflags;
};

struct HXCFE_FLOPPY_ {
    int32_t floppyNumberOfTrack;
    int32_t floppyNumberOfSide;
    int32_t floppySectorPerTrack;
    int32_t floppyBitRate;
    int32_t floppyiftype;
    double  floppyRPM;
    HXCFE_CYLINDER** tracks;
};

struct HXCFE_CYLINDER_ {
    int32_t number_of_side;
    HXCFE_SIDE** sides;
    int32_t floppyRPM;
};

struct HXCFE_SIDE_ {
    int32_t number_of_sector;
    uint32_t tracklen;          /* Track length in bits */
    uint8_t* databuffer;        /* MFM/FM encoded data */
    uint8_t* flakybitsbuffer;   /* Weak bits mask */
    uint8_t* indexbuffer;       /* Index pulse positions */
    uint32_t* timingbuffer;     /* Bit timing (ns) */
    int32_t bitrate;
    int32_t track_encoding;
};

struct HXCFE_SECTCFG_ {
    uint8_t  cylinder;
    uint8_t  head;
    uint8_t  sector;
    uint16_t sectorsize;
    uint8_t* input_data;
    uint8_t  trackencoding;
    uint8_t  gap3;
    uint8_t  fill_byte;
    uint16_t bitrate;
    
    /* Flags */
    uint8_t  use_alternate_data_crc;
    uint8_t  use_alternate_header_crc;
    uint16_t alternate_data_crc;
    uint16_t alternate_header_crc;
    uint8_t  missingdataaddressmark;
    uint8_t  alternate_datamark;
    uint8_t  alternate_addressmark;
    
    /* Weak bits */
    uint8_t* weak_bits_mask;
    int32_t  startsectorindex;
    int32_t  endsectorindex;
};

struct HXCFE_IMGLDR_ {
    HXCFE* hxcfe;
    void* userdata;
};

struct HXCFE_IMGLDR_FILEINFOS_ {
    char* path;
    int32_t file_size;
    uint8_t* file_header;  /* First bytes for magic detection */
    int32_t header_size;
};

struct HXCFE_TDCFG_ {
    int32_t x_us;           /* Track duration in us */
    int32_t y_us;           /* Cell duration in us */
    int32_t x_start_us;
    int32_t bitrate;
    int32_t rpm;
    int32_t disk_type;
};

struct HXCFE_SECTORACCESS_ {
    HXCFE* hxcfe;
    HXCFE_FLOPPY* fp;
    int32_t cur_track;
    int32_t cur_side;
};

/* ============================================================================
 * Function Prototypes (Stubs)
 * ============================================================================ */

/* Context management */
static inline HXCFE* hxcfe_init(void) {
    HXCFE* hxcfe = (HXCFE*)calloc(1, sizeof(HXCFE));
    return hxcfe;
}

static inline void hxcfe_deinit(HXCFE* hxcfe) {
    if (hxcfe) free(hxcfe);
}

/* Floppy management */
static inline HXCFE_FLOPPY* hxcfe_allocFloppy(HXCFE* hxcfe, int32_t tracks, int32_t sides) {
    (void)hxcfe;
    HXCFE_FLOPPY* fp = (HXCFE_FLOPPY*)calloc(1, sizeof(HXCFE_FLOPPY));
    if (fp) {
        fp->floppyNumberOfTrack = tracks;
        fp->floppyNumberOfSide = sides;
        fp->tracks = (HXCFE_CYLINDER**)calloc(tracks, sizeof(HXCFE_CYLINDER*));
    }
    return fp;
}

static inline void hxcfe_freeFloppy(HXCFE* hxcfe, HXCFE_FLOPPY* fp) {
    (void)hxcfe;
    if (!fp) return;
    if (fp->tracks) {
        for (int i = 0; i < fp->floppyNumberOfTrack; i++) {
            if (fp->tracks[i]) {
                HXCFE_CYLINDER* cyl = fp->tracks[i];
                if (cyl->sides) {
                    for (int j = 0; j < cyl->number_of_side; j++) {
                        if (cyl->sides[j]) {
                            if (cyl->sides[j]->databuffer) 
                                free(cyl->sides[j]->databuffer);
                            if (cyl->sides[j]->flakybitsbuffer) 
                                free(cyl->sides[j]->flakybitsbuffer);
                            if (cyl->sides[j]->indexbuffer)
                                free(cyl->sides[j]->indexbuffer);
                            if (cyl->sides[j]->timingbuffer)
                                free(cyl->sides[j]->timingbuffer);
                            free(cyl->sides[j]);
                        }
                    }
                    free(cyl->sides);
                }
                free(cyl);
            }
        }
        free(fp->tracks);
    }
    free(fp);
}

/* Image loader */
static inline HXCFE_IMGLDR* hxcfe_initImgLoader(HXCFE* hxcfe) {
    HXCFE_IMGLDR* imgldr = (HXCFE_IMGLDR*)calloc(1, sizeof(HXCFE_IMGLDR));
    if (imgldr) imgldr->hxcfe = hxcfe;
    return imgldr;
}

static inline void hxcfe_deinitImgLoader(HXCFE_IMGLDR* imgldr) {
    if (imgldr) free(imgldr);
}

/* Progress callback (stub) */
static inline void hxcfe_imgCallProgressCallback(HXCFE_IMGLDR* imgldr, int cur, int total) {
    (void)imgldr; (void)cur; (void)total;
    /* Progress callback - can be overridden */
}

/* Track generation */
HXCFE_SIDE* tg_generateTrack(
    uint8_t* sectors_data,
    int32_t sector_size,
    int32_t number_of_sectors,
    int32_t track,
    int32_t side,
    int32_t bitrate,
    int32_t rpm,
    int32_t encoding,
    int32_t gap3,
    int32_t interleave,
    int32_t skew,
    HXCFE_SECTCFG* sectorcfg,
    int32_t nb_sectorcfg
);

HXCFE_SIDE* tg_generateTrackEx(
    int32_t number_of_sectors,
    HXCFE_SECTCFG* sectorcfg,
    int32_t interleave,
    int32_t skew,
    int32_t bitrate,
    int32_t rpm,
    int32_t encoding,
    int32_t indexlen,
    int32_t indexpos
);

/* CRC functions */
uint16_t hxcfe_crc16_ccitt(const uint8_t* data, size_t len, uint16_t init);

#ifdef __cplusplus
}
#endif

#endif /* LIBHXCFE_H */
