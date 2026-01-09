/**
 * @file liblibflux.h
 * 
 * Provides type definitions and macros compatible with UFT library
 * to allow integration of HxC-derived code into UFT.
 * 
 * @note This is a compatibility shim, not the full UFT library
 */

#ifndef LIBLIBFLUX_H
#define LIBLIBFLUX_H

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

#define LIBFLUX_NOERROR           0
#define LIBFLUX_ACCESSERROR      -1
#define LIBFLUX_BADFILE          -2
#define LIBFLUX_BADPARAMETER     -3
#define LIBFLUX_INTERNALERROR    -4
#define LIBFLUX_UNSUPPORTEDFILE  -5
#define LIBFLUX_VALIDFILE         1

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
#define APPLEMAC_GCR6A2         APPLEMAC_GCR_ENCODING
#define QD_MO5_ENCODING         0x10
#define C64_GCR_ENCODING        0x11
#define VICTOR9K_GCR_ENCODING   0x12
#define MICRALN_HS_FM_ENCODING  0x13
#define CENTURION_MFM_ENCODING  0x14
#define APPLE2_GCR6A2           0x15
#define DIRECT_ENCODING         0xFE
#define UNKNOWN_ENCODING        0xFF

/* Track generator flags */
#define REVERTED_INDEX          0x80000000
#define NO_SECTOR_UNDER_INDEX   0x00000001

/* ============================================================================
 * Forward Declarations
 * ============================================================================ */

typedef struct LIBFLUX_ LIBFLUX;
typedef struct LIBFLUX_FLOPPY_ LIBFLUX_FLOPPY;
typedef struct LIBFLUX_CYLINDER_ LIBFLUX_CYLINDER;

#ifndef LIBFLUX_SIDE_DEFINED
#define LIBFLUX_SIDE_DEFINED
typedef struct LIBFLUX_SIDE_ LIBFLUX_SIDE;
#endif

typedef struct LIBFLUX_IMGLDR_ LIBFLUX_IMGLDR;
typedef struct LIBFLUX_IMGLDR_FILEINFOS_ LIBFLUX_IMGLDR_FILEINFOS;
typedef struct LIBFLUX_SECTORACCESS_ LIBFLUX_SECTORACCESS;
typedef struct LIBFLUX_TDCFG_ LIBFLUX_TDCFG;

/* LIBFLUX_CTX alias for compatibility with track_generator.h */
#ifndef LIBFLUX_CTX_DEFINED
#define LIBFLUX_CTX_DEFINED
typedef LIBFLUX LIBFLUX_CTX;
#endif

/* LIBFLUX_SECTCFG - Forward declare, actual definition in track_generator.h */
/* If track_generator.h is included, it provides the full LIBFLUX_SECTCFG_EXT */
/* If not, we provide a compatible simple version */
#ifndef LIBFLUX_SECTCFG_EXT_DEFINED
#ifndef _LIBFLUX_SECTCFG_
typedef struct LIBFLUX_SECTCFG_SIMPLE_ LIBFLUX_SECTCFG_SIMPLE;
#endif
#endif

/* ============================================================================
 * Logging Support
 * ============================================================================ */

#define MSG_DEBUG    0
#define MSG_INFO     1
#define MSG_WARNING  2
#define MSG_ERROR    3

/* Printf function type for HxC compatibility */
typedef int (*libflux_printf_func)(int level, const char* fmt, ...);

/* No-op printf for compatibility */
static inline int libflux_printf_noop(int level, const char* fmt, ...) {
    (void)level; (void)fmt;
    return 0;
}

/* ============================================================================
 * Additional Encoding Constants (HxC compatibility)
 * ============================================================================ */

#define AED6200P_DD             AED6200P_MFM_ENCODING
#define ISOFORMAT_DD            ISOIBM_MFM_ENCODING
#define ISOFORMAT_SD            ISOIBM_FM_ENCODING
#define ISOFORMAT_DD11S         0x20  /* 11-sector DD format */
#define ISOFORMAT_HD            0x21  /* High density */
#define UKNCFORMAT_DD           0x22  /* UKNC computer format */
#define IBMFORMAT_SD            ISOIBM_FM_ENCODING
#define IBMFORMAT_DD            ISOIBM_MFM_ENCODING
#define AMIGAFORMAT_DD          AMIGA_MFM_ENCODING
#define ARBURG_DAT              ARBURGDAT_ENCODING
#define ARBURG_SYS              ARBURGSYS_ENCODING
#define DECRX02_SDDD            DEC_RX02_M2FM_ENCODING
#define EMUFORMAT_SD            EMU_FM_ENCODING
#define TYCOMFORMAT_SD          TYCOM_FM_ENCODING
#define MEMBRAINFORMAT_DD       MEMBRAIN_MFM_ENCODING
#define HEATHKIT_FM             HEATHKIT_HS_FM_ENCODING
#define HEATHKIT_HS_SD          HEATHKIT_HS_FM_ENCODING
#define NORTHSTAR_MFM           NORTHSTAR_HS_MFM_ENCODING
#define NORTHSTAR_HS_DD         NORTHSTAR_HS_MFM_ENCODING
#define MICRALN_FM              MICRALN_HS_FM_ENCODING
#define MICRALN_HS_SD           MICRALN_HS_FM_ENCODING
#define C64_GCR                 C64_GCR_ENCODING
#define VICTOR9K_GCR            VICTOR9K_GCR_ENCODING
#define APPLE2_GCR1             APPLEII_GCR1_ENCODING
#define APPLE2_GCR2             APPLEII_GCR2_ENCODING
#define APPLE2_GCR5A3           0x23  /* Apple II 5-and-3 GCR */
#define APPLEMAC_GCR            APPLEMAC_GCR_ENCODING
#define QD_MO5                  QD_MO5_ENCODING
#define QD_MO5_MFM              QD_MO5_ENCODING
#define CENTURION_MFM           CENTURION_MFM_ENCODING

/* Track flags */
#ifndef VARIABLEBITRATE
#define VARIABLEBITRATE         0x0001
#endif
#ifndef VARIABLEENCODING
#define VARIABLEENCODING        0x0002
#endif
#ifndef REVERTED_INDEX
#define REVERTED_INDEX          0x0004
#endif
#ifndef NO_SECTOR_UNDER_INDEX
#define NO_SECTOR_UNDER_INDEX   0x0008
#endif

/* Sector size lookup table - defined in luts.c */
extern unsigned short sectorsize[];

/* LUT tables - defined in luts.c */
extern unsigned short LUT_Byte2ShortOddBitsExpander[];
extern unsigned short LUT_Byte2ShortEvenBitsExpander[];
extern unsigned short LUT_Byte2MFM[];
extern unsigned short LUT_Byte2MFMClkMask[];
extern unsigned char  LUT_Byte2OddBits[];
extern unsigned char  LUT_Byte2EvenBits[];
extern unsigned char  LUT_Byte2HighBitsCount[];
extern unsigned char  LUT_ByteBitsInverter[];

/* ============================================================================
 * Core Structures
 * ============================================================================ */

struct LIBFLUX_ {
    void* userdata;
    int envflags;
    libflux_printf_func libflux_printf;  /* HxC logging callback */
};

struct LIBFLUX_FLOPPY_ {
    int32_t floppyNumberOfTrack;
    int32_t floppyNumberOfSide;
    int32_t floppySectorPerTrack;
    int32_t floppyBitRate;
    int32_t floppyiftype;
    double  floppyRPM;
    LIBFLUX_CYLINDER** tracks;
};

struct LIBFLUX_CYLINDER_ {
    int32_t number_of_side;
    LIBFLUX_SIDE** sides;
    int32_t floppyRPM;
};

struct LIBFLUX_SIDE_ {
    int32_t number_of_sector;
    uint32_t tracklen;          /* Track length in bits */
    uint8_t* databuffer;        /* MFM/FM encoded data */
    uint8_t* flakybitsbuffer;   /* Weak bits mask */
    uint8_t* indexbuffer;       /* Index pulse positions */
    uint32_t* timingbuffer;     /* Bit timing (ns) */
    uint8_t* track_encoding_buffer; /* Per-bit encoding type */
    int32_t bitrate;
    int32_t track_encoding;
};

/* Full sector config - compatible with track_generator.h LIBFLUX_SECTCFG_EXT */
#ifndef LIBFLUX_SECTCFG_EXT_DEFINED
#ifndef _LIBFLUX_SECTCFG_
struct LIBFLUX_SECTCFG_FULL_ {
    int32_t        head;
    int32_t        sector;
    int32_t        sectorsleft;
    int32_t        cylinder;

    int32_t        sectorsize;

    int32_t        use_alternate_sector_size_id;
    int32_t        alternate_sector_size_id;

    int32_t        missingdataaddressmark;

    int32_t        use_alternate_header_crc;    /* 0x1 -> Bad crc, 0x2 alternate crc */
    uint32_t       data_crc;

    int32_t        use_alternate_data_crc;      /* 0x1 -> Bad crc, 0x2 alternate crc */
    uint32_t       header_crc;

    int32_t        use_alternate_datamark;
    int32_t        alternate_datamark;

    int32_t        use_alternate_addressmark;
    int32_t        alternate_addressmark;

    int32_t        startsectorindex;
    int32_t        startdataindex;
    int32_t        endsectorindex;

    int32_t        trackencoding;

    int32_t        gap3;

    int32_t        bitrate;

    uint8_t*       input_data;
    int32_t*       input_data_index;

    uint8_t*       weak_bits_mask;

    uint8_t        fill_byte;
    uint8_t        fill_byte_used;  /* Set to indicate sector is filled with fill_byte */

    uint32_t       flags;
};
/* If track_generator.h not included, use full version */
typedef struct LIBFLUX_SECTCFG_FULL_ LIBFLUX_SECTCFG;
#define _LIBFLUX_SECTCFG_
#define LIBFLUX_SECTCFG_EXT_DEFINED
typedef LIBFLUX_SECTCFG LIBFLUX_SECTCFG_EXT;
#endif
#endif

struct LIBFLUX_IMGLDR_ {
    LIBFLUX* libflux;
    LIBFLUX* ctx;           /* HxC compatibility alias for libflux */
    void* userdata;
};

struct LIBFLUX_IMGLDR_FILEINFOS_ {
    char* path;
    int32_t file_size;
    uint8_t* file_header;  /* First bytes for magic detection */
    int32_t header_size;
};

struct LIBFLUX_TDCFG_ {
    int32_t x_us;           /* Track duration in us */
    int32_t y_us;           /* Cell duration in us */
    int32_t x_start_us;
    int32_t bitrate;
    int32_t rpm;
    int32_t disk_type;
};

/* Sector search track cache - for sector_extractor */
#define MAX_CACHED_SECTOR 64

typedef struct SECTORSEARCHTRACKCACHE_ {
    int32_t nb_sector_cached;
    LIBFLUX_SECTCFG sectorcache[MAX_CACHED_SECTOR];
} SECTORSEARCHTRACKCACHE;

/* FDC status codes */
#define FDC_NOERROR             0x00
#define FDC_NO_DATA             0x01
#define FDC_BAD_DATA_CRC        0x02
#define FDC_SECTOR_NOT_FOUND    0x04
#define FDC_ACCESS_ERROR        0x10

/* Sector access flags */
#define SECTORACCESS_IGNORE_SIDE_ID 0x00000001

/* Forward declaration for circular reference */
struct LIBFLUX_SECTORACCESS_;

/* FDC Controller emulation - for sector_extractor */
typedef struct LIBFLUX_FDCCTRL_ {
    LIBFLUX* flux_ctx;
    LIBFLUX_FLOPPY* loadedfp;
    struct LIBFLUX_SECTORACCESS_* ss_ctx;
    int32_t last_track;
    int32_t last_side;
    int32_t last_sector;
    int32_t status;
    uint8_t* data_buffer;
    int32_t data_length;
} LIBFLUX_FDCCTRL;

/* Error codes */
#define LIBFLUX_FILECORRUPTED   -2
#define LIBFLUX_NOERROR          0
#define LIBFLUX_ERR_GENERIC     -1

struct LIBFLUX_SECTORACCESS_ {
    LIBFLUX* libflux;
    LIBFLUX* ctx;           /* HxC compatibility alias */
    LIBFLUX_FLOPPY* fp;
    int32_t cur_track;
    int32_t cur_side;
    int32_t bitoffset;
    int32_t old_bitoffset;
    uint32_t flags;
    SECTORSEARCHTRACKCACHE* track_cache;
};

/* ============================================================================
 * Function Prototypes (Stubs)
 * ============================================================================ */

/* Context management */
static inline LIBFLUX* libflux_init(void) {
    LIBFLUX* libflux = (LIBFLUX*)calloc(1, sizeof(LIBFLUX));
    return libflux;
}

static inline void libflux_deinit(LIBFLUX* libflux) {
    if (libflux) free(libflux);
}

/* Floppy management */
static inline LIBFLUX_FLOPPY* libflux_allocFloppy(LIBFLUX* libflux, int32_t tracks, int32_t sides) {
    (void)libflux;
    LIBFLUX_FLOPPY* fp = (LIBFLUX_FLOPPY*)calloc(1, sizeof(LIBFLUX_FLOPPY));
    if (fp) {
        fp->floppyNumberOfTrack = tracks;
        fp->floppyNumberOfSide = sides;
        fp->tracks = (LIBFLUX_CYLINDER**)calloc(tracks, sizeof(LIBFLUX_CYLINDER*));
    }
    return fp;
}

static inline void libflux_freeFloppy(LIBFLUX* libflux, LIBFLUX_FLOPPY* fp) {
    (void)libflux;
    if (!fp) return;
    if (fp->tracks) {
        for (int i = 0; i < fp->floppyNumberOfTrack; i++) {
            if (fp->tracks[i]) {
                LIBFLUX_CYLINDER* cyl = fp->tracks[i];
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
static inline LIBFLUX_IMGLDR* libflux_initImgLoader(LIBFLUX* libflux) {
    LIBFLUX_IMGLDR* imgldr = (LIBFLUX_IMGLDR*)calloc(1, sizeof(LIBFLUX_IMGLDR));
    if (imgldr) imgldr->libflux = libflux;
    return imgldr;
}

static inline void libflux_deinitImgLoader(LIBFLUX_IMGLDR* imgldr) {
    if (imgldr) free(imgldr);
}

/* Progress callback (stub) */
static inline void libflux_imgCallProgressCallback(LIBFLUX_IMGLDR* imgldr, int cur, int total) {
    (void)imgldr; (void)cur; (void)total;
    /* Progress callback - can be overridden */
}

/* Track generation functions - defined in track_generator.h */
#ifndef UFT_TRACK_GENERATOR_H
/* Only declare if track_generator.h not included */
LIBFLUX_SIDE* tg_generateTrack(
    uint8_t* sectors_data,
    int32_t sector_size,
    int32_t number_of_sectors,
    int32_t track,
    int32_t side,
    int32_t sectorid,
    int32_t interleave,
    int32_t skew,
    int32_t bitrate,
    int32_t rpm,
    int32_t trackencoding,
    int32_t gap3,
    int32_t pregap,
    int32_t indexlen,
    int32_t indexpos
);

LIBFLUX_SIDE* tg_generateTrackEx(
    int32_t number_of_sector,
    LIBFLUX_SECTCFG* sectorconfigtab,
    int32_t interleave,
    int32_t skew,
    int32_t bitrate,
    int32_t rpm,
    int32_t trackencoding,
    int32_t pregap,
    int32_t indexlen,
    int32_t indexpos
);
#endif /* UFT_TRACK_GENERATOR_H */

/* CRC functions */
uint16_t libflux_crc16_ccitt(const uint8_t* data, size_t len, uint16_t init);

#ifdef __cplusplus
}
#endif

#endif /* LIBLIBFLUX_H */
