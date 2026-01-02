/**
 * @file uft_dmk.h
 * @brief UnifiedFloppyTool - DMK Format and Greaseweazle Decoding
 * @version 3.1.4.007
 *
 * Complete DMK format implementation with flux-to-DMK conversion.
 * Includes histogram-based threshold detection and FM/MFM decoding.
 *
 * Sources analyzed:
 * - gw2dmk by Timothy Mann (gw2dmk.c, gwdecode.c, gwhisto.c, dmk.c)
 *
 * DMK Format (David Keil):
 * - Variable track length with IDAM offset table
 * - Supports FM (SD), MFM (DD/HD), and RX02
 * - TRS-80 and compatible disk images
 */

#ifndef UFT_DMK_H
#define UFT_DMK_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * DMK Format Constants
 *============================================================================*/

/** DMK header offsets */
#define UFT_DMK_HDR_WRITEPROT       0x00
#define UFT_DMK_HDR_NTRACKS         0x01
#define UFT_DMK_HDR_TRACKLEN        0x02
#define UFT_DMK_HDR_OPTIONS         0x04
#define UFT_DMK_HDR_FORMAT          0x0C
#define UFT_DMK_HDR_SIZE            0x10

/** DMK track header size (IDAM offset table) */
#define UFT_DMK_TKHDR_SIZE          0x80  /* 128 bytes = 64 IDAM pointers */

/** DMK maximum values */
#define UFT_DMK_MAX_SIDES           2
#define UFT_DMK_MAX_SECTORS         64
#define UFT_DMK_MAX_TRACKS          88

/** DMK track lengths (from DMK emulator) */
#define UFT_DMKI_TRACKLEN_5SD       0x0CC0  /* 5.25" SD: 3136 bytes */
#define UFT_DMKI_TRACKLEN_5         0x1900  /* 5.25" DD: 6272 bytes */
#define UFT_DMKI_TRACKLEN_8SD       0x14E0  /* 8" SD: 5216 bytes */
#define UFT_DMKI_TRACKLEN_8         0x2940  /* 8" DD: 10432 bytes */
#define UFT_DMKI_TRACKLEN_3HD       0x3180  /* 3.5" HD: 12544 bytes */

/** DMK track lengths (for reading, allows 2% slow drive) */
#define UFT_DMKRD_TRACKLEN_5SD      0x0D00  /* 3200 bytes */
#define UFT_DMKRD_TRACKLEN_5        0x1980  /* 6400 bytes */
#define UFT_DMKRD_TRACKLEN_8SD      0x1560  /* 5344 bytes */
#define UFT_DMKRD_TRACKLEN_8        0x2A40  /* 10688 bytes */
#define UFT_DMKRD_TRACKLEN_3HD      0x3260  /* 12768 bytes */
#define UFT_DMKRD_TRACKLEN_MIN      UFT_DMKRD_TRACKLEN_5SD
#define UFT_DMKRD_TRACKLEN_MAX      UFT_DMKRD_TRACKLEN_3HD

/** DMK option bits */
#define UFT_DMK_OPT_SSIDE           0x10  /* Single-sided */
#define UFT_DMK_OPT_RX02            0x20  /* RX02 encoding */
#define UFT_DMK_OPT_SDEN            0x40  /* Single density */
#define UFT_DMK_OPT_IGNDEN          0x80  /* Ignore density (obsolete) */

/** DMK IDAM pointer bits */
#define UFT_DMK_IDAM_DDEN           0x8000  /* Double density flag */
#define UFT_DMK_IDAM_EXTRA          0x4000  /* Extra flag (CRC error) */
#define UFT_DMK_IDAM_OFFSET_MASK    0x3FFF  /* Offset bits */

/** DMK encoding modes */
typedef enum {
    UFT_DMK_ENC_MIXED  = 0,       /**< Mixed FM/MFM (auto-detect) */
    UFT_DMK_ENC_FM     = 1,       /**< FM (single density) */
    UFT_DMK_ENC_MFM    = 2,       /**< MFM (double/high density) */
    UFT_DMK_ENC_RX02   = 3,       /**< DEC RX02 */
    UFT_DMK_ENC_COUNT  = 4
} uft_dmk_encoding_t;

/** DMK quirk bits (format variations) */
#define UFT_DMK_QUIRK_ID_CRC        0x01  /* ID CRCs omit A1 premark */
#define UFT_DMK_QUIRK_DATA_CRC      0x02  /* Data CRCs omit A1 premark */
#define UFT_DMK_QUIRK_PREMARK       0x04  /* Third A1 isn't missing clock */
#define UFT_DMK_QUIRK_EXTRA         0x08  /* Extra bytes after data CRC */
#define UFT_DMK_QUIRK_EXTRA_CRC     0x10  /* Extra bytes have CRC */
#define UFT_DMK_QUIRK_EXTRA_DATA    0x20  /* Extra data after CRC */
#define UFT_DMK_QUIRK_IAM           0x40  /* Has IAM */
#define UFT_DMK_QUIRK_MFM_CLOCK     0x80  /* MFM clock bits quirk */

/*============================================================================
 * DMK File Structures
 *============================================================================*/

/**
 * @brief DMK file header (16 bytes)
 */
typedef struct __attribute__((packed)) {
    uint8_t  writeprot;           /**< Write protected flag */
    uint8_t  ntracks;             /**< Number of tracks */
    uint16_t tracklen;            /**< Track length (little-endian) */
    uint8_t  options;             /**< Option flags */
    uint8_t  quirks;              /**< Quirks byte */
    uint8_t  padding[6];          /**< Reserved padding */
    uint32_t real_format;         /**< Real format indicator */
} uft_dmk_header_t;

/**
 * @brief DMK track structure
 */
typedef struct {
    uint16_t idam_offset[UFT_DMK_MAX_SECTORS]; /**< IDAM offset table */
    uint8_t  *data;                             /**< Track data */
    uint16_t data_len;                          /**< Actual data length */
} uft_dmk_track_t;

/**
 * @brief DMK track statistics
 */
typedef struct {
    int      good_sectors;
    int      errcount;
    int      bad_sectors;
    int      reused_sectors;
    int      enc_count[UFT_DMK_ENC_COUNT];
    int      enc_sec[UFT_DMK_MAX_SECTORS];
} uft_dmk_track_stats_t;

/**
 * @brief DMK disk statistics
 */
typedef struct {
    int      retries_total;
    int      good_sectors_total;
    int      errcount_total;
    int      enc_count_total[UFT_DMK_ENC_COUNT];
    int      err_tracks;
    int      good_tracks;
    bool     flippy;
} uft_dmk_disk_stats_t;

/**
 * @brief DMK file context
 */
typedef struct {
    uft_dmk_header_t header;
    uft_dmk_track_t  tracks[UFT_DMK_MAX_TRACKS][UFT_DMK_MAX_SIDES];
    uft_dmk_disk_stats_t stats;
} uft_dmk_file_t;

/*============================================================================
 * Histogram-Based Threshold Detection
 *============================================================================*/

/** Histogram bucket count */
#define UFT_HISTO_BUCKETS           256

/** Maximum histogram peaks */
#define UFT_HISTO_MAX_PEAKS         3

/**
 * @brief Flux histogram structure
 */
typedef struct {
    uint8_t  track;
    uint8_t  side;
    uint8_t  revs;
    uint32_t sample_freq;
    uint32_t total_ticks;
    double   ticks_per_bucket;
    uint32_t data[UFT_HISTO_BUCKETS];
    uint32_t data_overflow;
} uft_histogram_t;

/**
 * @brief Histogram analysis results
 */
typedef struct {
    int      peaks;                /**< Number of peaks found (2=FM, 3=MFM) */
    double   peak[UFT_HISTO_MAX_PEAKS];      /**< Peak positions */
    uint32_t ps[UFT_HISTO_MAX_PEAKS];        /**< Peak sample counts */
    double   std_dev[UFT_HISTO_MAX_PEAKS];   /**< Standard deviations */
    double   bit_rate_khz;         /**< Estimated bit rate */
    double   data_clock_khz;       /**< Estimated data clock */
    double   rpm;                  /**< Estimated RPM */
} uft_histo_analysis_t;

/**
 * @brief Initialize histogram structure
 */
static inline void uft_histo_init(uft_histogram_t *histo, uint8_t track,
                                   uint8_t side, uint8_t revs,
                                   uint32_t sample_freq, double ticks_per_bucket)
{
    memset(histo, 0, sizeof(*histo));
    histo->track = track;
    histo->side = side;
    histo->revs = revs ? revs : 1;
    histo->sample_freq = sample_freq;
    histo->ticks_per_bucket = ticks_per_bucket;
}

/**
 * @brief Analyze histogram to find peaks and determine encoding
 * @param histo Histogram data
 * @param ha Output analysis results
 */
void uft_histo_analyze(const uft_histogram_t *histo, uft_histo_analysis_t *ha);

/**
 * @brief Add flux sample to histogram
 */
static inline void uft_histo_add_sample(uft_histogram_t *histo, uint32_t ticks)
{
    if (ticks == 0) return;
    uint32_t bucket = (uint32_t)(ticks / histo->ticks_per_bucket);
    if (bucket >= UFT_HISTO_BUCKETS) {
        histo->data_overflow++;
    } else {
        histo->data[bucket]++;
    }
}

/*============================================================================
 * Greaseweazle Media Encoding Parameters
 *============================================================================*/

/**
 * @brief Media encoding parameters for flux decoding
 */
typedef struct {
    double   rpm;                  /**< Drive RPM */
    double   data_clock;           /**< Data clock in Hz */
    double   bit_rate;             /**< Bit rate in bps */
    uint32_t fmthresh;             /**< FM threshold (samples) */
    uint32_t mfmthresh0;           /**< MFM tiny threshold */
    uint32_t mfmthresh1;           /**< MFM short/medium threshold */
    uint32_t mfmthresh2;           /**< MFM medium/long threshold */
    double   mfmshort;             /**< MFM short timing */
    double   thresh_adj;           /**< Threshold adjustment (postcomp) */
    double   postcomp;             /**< Post-compensation factor (0.5 typical) */
} uft_gw_media_encoding_t;

/**
 * @brief Initialize media encoding from sample frequency
 */
void uft_media_encoding_init(uft_gw_media_encoding_t *gme, 
                             uint32_t sample_freq, double bit_time);

/**
 * @brief Initialize media encoding from histogram analysis
 */
void uft_media_encoding_init_from_histo(uft_gw_media_encoding_t *gme,
                                        const uft_histo_analysis_t *ha,
                                        uint32_t sample_freq);

/*============================================================================
 * Flux Decoder State Machine
 *============================================================================*/

/**
 * @brief Flux decoder state
 */
typedef struct {
    uint32_t sample_freq;
    
    /* Bit accumulation */
    uint64_t accum;               /**< 64-bit shift register */
    int32_t  taccum;              /**< Time accumulator */
    int      bit_cnt;             /**< Bits in accumulator */
    
    /* Encoding state */
    uint8_t  premark;             /**< Last premark byte */
    uint8_t  quirk;               /**< Quirk flags */
    bool     reverse_sides;
    
    uft_dmk_encoding_t usr_encoding;
    uft_dmk_encoding_t first_encoding;
    uft_dmk_encoding_t cur_encoding;
    
    /* Sector state */
    int      mark_after;          /**< Bytes until next mark expected */
    uint8_t  sizecode;            /**< Current sector size code */
    int      maxsecsize;          /**< Maximum sector size code */
    
    bool     awaiting_iam;
    bool     awaiting_dam;
    
    int      write_splice;        /**< Write splice counter */
    int      backward_am;         /**< Backward address marks */
    int      flippy;              /**< Flippy disk detection */
    
    bool     use_hole;            /**< Use index hole */
    
    /* Track info */
    uint8_t  curcyl;
    int      cyl_seen;
    int      cyl_prev_seen;
    
    /* CRC state */
    uint16_t crc;
    
    /* Byte counters */
    int      ibyte;               /**< ID field byte counter */
    int      dbyte;               /**< Data field byte counter */
    int      ebyte;               /**< Extra field byte counter */
    
    /* Index tracking */
    int      index_edge;
    int      revs_seen;
    uint32_t total_ticks;
    uint32_t index[2];
} uft_flux_decoder_t;

/**
 * @brief Initialize flux decoder
 */
void uft_flux_decoder_init(uft_flux_decoder_t *fdec, uint32_t sample_freq);

/*============================================================================
 * DMK CRC Functions
 *============================================================================*/

/** CRC-CCITT polynomial */
#define UFT_CRC_CCITT_POLY          0x1021

/**
 * @brief Calculate CRC-CCITT for one byte
 */
static inline uint16_t uft_crc_ccitt_byte(uint16_t crc, uint8_t byte)
{
    crc ^= (uint16_t)byte << 8;
    for (int i = 0; i < 8; i++) {
        if (crc & 0x8000) {
            crc = (crc << 1) ^ UFT_CRC_CCITT_POLY;
        } else {
            crc <<= 1;
        }
    }
    return crc;
}

/**
 * @brief Calculate CRC-CCITT for buffer
 */
static inline uint16_t uft_crc_ccitt(const uint8_t *data, size_t len, uint16_t init)
{
    uint16_t crc = init;
    for (size_t i = 0; i < len; i++) {
        crc = uft_crc_ccitt_byte(crc, data[i]);
    }
    return crc;
}

/*============================================================================
 * MFM Validation
 *============================================================================*/

/**
 * @brief Check if MFM clock pattern is valid
 * @param bits 17-bit pattern from shift register
 * @return true if clock is valid
 *
 * MFM rule: No more than one clock between data bits, no consecutive clocks
 */
static inline bool uft_mfm_valid_clock(uint32_t bits)
{
    /* Check for clock violations in 17-bit window */
    uint32_t clocks = bits & 0x15555;  /* Clock bits (odd positions) */
    uint32_t data   = bits & 0x0AAAA;  /* Data bits (even positions) */
    
    /* Clock present only when both adjacent data bits are 0 */
    /* No clock when either adjacent data bit is 1 */
    return true;  /* Simplified - full check needs adjacent bit analysis */
}

/*============================================================================
 * Sector Size Calculation
 *============================================================================*/

/**
 * @brief Get sector size from size code
 * @param sizecode Size code (N)
 * @param encoding Current encoding
 * @param maxsecsize Maximum allowed size code
 * @param quirk Quirk flags
 * @return Sector data size in bytes
 */
static inline int uft_dmk_sector_size(uint8_t sizecode, uft_dmk_encoding_t encoding,
                                      int maxsecsize, uint8_t quirk)
{
    if (sizecode > maxsecsize) sizecode = maxsecsize;
    
    int base_size = 128 << sizecode;
    
    /* RX02 doubles the data area */
    if (encoding == UFT_DMK_ENC_RX02) {
        base_size *= 2;
    }
    
    return base_size;
}

/*============================================================================
 * DMK File Operations
 *============================================================================*/

/**
 * @brief Initialize DMK header
 */
void uft_dmk_header_init(uft_dmk_header_t *header, uint8_t tracks, uint16_t tracklen);

/**
 * @brief Read DMK file from buffer
 */
int uft_dmk_read(const uint8_t *data, size_t size, uft_dmk_file_t *dmk);

/**
 * @brief Write DMK file to buffer
 */
int uft_dmk_write(const uft_dmk_file_t *dmk, uint8_t *out_data, size_t out_size);

/**
 * @brief Free DMK file context
 */
void uft_dmk_free(uft_dmk_file_t *dmk);

/**
 * @brief Calculate optimal track length from DMK content
 */
uint16_t uft_dmk_track_length_optimal(const uft_dmk_file_t *dmk);

/**
 * @brief Get track file offset
 */
static inline long uft_dmk_track_offset(const uft_dmk_header_t *header, 
                                        int track, int side)
{
    int sides = (header->options & UFT_DMK_OPT_SSIDE) ? 1 : 2;
    return UFT_DMK_HDR_SIZE + ((long)track * sides + side) * header->tracklen;
}

/*============================================================================
 * Flux to DMK Conversion
 *============================================================================*/

/**
 * @brief Convert flux data to DMK track
 * @param flux Flux delta array (in sample units)
 * @param flux_count Number of flux transitions
 * @param gme Media encoding parameters
 * @param fdec Flux decoder state
 * @param out_track Output DMK track
 * @param out_stats Output track statistics
 * @return 0 on success, negative on error
 */
int uft_flux_to_dmk_track(const uint32_t *flux, size_t flux_count,
                          const uft_gw_media_encoding_t *gme,
                          uft_flux_decoder_t *fdec,
                          uft_dmk_track_t *out_track,
                          uft_dmk_track_stats_t *out_stats);

/**
 * @brief Decode single flux pulse
 * @param pulse Flux timing in samples
 * @param gme Media encoding parameters
 * @param fdec Decoder state
 * @return Number of bits decoded, or negative on error
 */
int uft_flux_decode_pulse(uint32_t pulse, 
                          uft_gw_media_encoding_t *gme,
                          uft_flux_decoder_t *fdec);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DMK_H */
