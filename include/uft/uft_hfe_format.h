/**
 * @file uft_hfe_format.h
 * @brief HFE (UFT HFE Format) Image Format Support
 * 
 * 
 * HFE is used by HxC floppy emulators and Gotek devices.
 * Supports both V1 (HXCPICFE) and V3 (HXCHFEV3) formats.
 */

#ifndef UFT_HFE_FORMAT_H
#define UFT_HFE_FORMAT_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * HFE FORMAT CONSTANTS
 *============================================================================*/

#define HFE_SIGNATURE_V1        "HXCPICFE"
#define HFE_SIGNATURE_V3        "HXCHFEV3"
#define HFE_SIGNATURE_LEN       8

#define HFE_HEADER_SIZE         512     /* Header is always 512 bytes */
#define HFE_TRACK_BLOCK_SIZE    512     /* Track data in 512-byte blocks */
#define HFE_BASE_FREQUENCY      36000000    /* 36 MHz base frequency */

#define HFE_MAX_CYLINDERS       255
#define HFE_MAX_HEADS           2

/*============================================================================
 * TRACK ENCODING TYPES
 *============================================================================*/

typedef enum {
    HFE_ENC_ISOIBM_MFM      = 0x00,     /* Standard IBM MFM */
    HFE_ENC_AMIGA_MFM       = 0x01,     /* Amiga MFM (odd/even) */
    HFE_ENC_ISOIBM_FM       = 0x02,     /* IBM FM (single density) */
    HFE_ENC_EMU_FM          = 0x03,     /* Emulator FM */
    HFE_ENC_UNKNOWN         = 0xFF
} hfe_track_encoding_t;

/*============================================================================
 * FLOPPY INTERFACE TYPES
 *============================================================================*/

typedef enum {
    HFE_IF_IBMPC_DD         = 0x00,     /* PC 3.5" DD */
    HFE_IF_IBMPC_HD         = 0x01,     /* PC 3.5" HD */
    HFE_IF_ATARIST_DD       = 0x02,     /* Atari ST DD */
    HFE_IF_ATARIST_HD       = 0x03,     /* Atari ST HD */
    HFE_IF_AMIGA_DD         = 0x04,     /* Amiga DD */
    HFE_IF_AMIGA_HD         = 0x05,     /* Amiga HD */
    HFE_IF_CPC_DD           = 0x06,     /* Amstrad CPC DD */
    HFE_IF_GENERIC_SHUGART  = 0x07,     /* Generic Shugart */
    HFE_IF_IBMPC_ED         = 0x08,     /* PC 3.5" ED */
    HFE_IF_MSX2_DD          = 0x09,     /* MSX2 DD */
    HFE_IF_C64_DD           = 0x0A,     /* Commodore 64 DD */
    HFE_IF_EMU_SHUGART      = 0x0B,     /* Emulator Shugart */
    HFE_IF_S950_DD          = 0x0C,     /* Akai S950 DD */
    HFE_IF_S950_HD          = 0x0D,     /* Akai S950 HD */
    HFE_IF_LAST_KNOWN       = 0x0E
} hfe_floppy_interface_t;

/*============================================================================
 * STEP MODE
 *============================================================================*/

typedef enum {
    HFE_STEP_SINGLE         = 0x00,     /* Single step */
    HFE_STEP_DOUBLE         = 0x01      /* Double step (40-track in 80-track) */
} hfe_step_mode_t;

/*============================================================================
 * HFE V1 HEADER STRUCTURE
 *============================================================================*/

#pragma pack(push, 1)

/**
 * HFE file header (512 bytes)
 */
typedef struct {
    char     signature[8];          /* "HXCPICFE" or "HXCHFEV3" */
    uint8_t  format_revision;       /* 0 for V1 */
    uint8_t  n_cylinders;           /* Number of cylinders */
    uint8_t  n_heads;               /* Number of heads (1 or 2) */
    uint8_t  track_encoding;        /* hfe_track_encoding_t */
    uint16_t data_bit_rate;         /* Bit rate in kbit/s (250, 300, 500) */
    uint16_t drive_rpm;             /* RPM (300 or 360) */
    uint8_t  uft_floppy_interface;      /* hfe_floppy_interface_t */
    uint8_t  reserved1;
    uint16_t track_list_offset;     /* Offset to track LUT (in 512-byte blocks) */
    uint8_t  write_allowed;         /* 0xFF = write allowed */
    uint8_t  single_step;           /* 0xFF = single step, 0x00 = double step */
    uint8_t  track0s0_altencoding;  /* Alternative encoding for track 0, side 0 */
    uint8_t  track0s0_encoding;     /* Encoding if alt enabled */
    uint8_t  track0s1_altencoding;  /* Alternative encoding for track 0, side 1 */
    uint8_t  track0s1_encoding;     /* Encoding if alt enabled */
    uint8_t  reserved2[464];        /* Padding to 512 bytes */
} hfe_header_t;

/**
 * Track lookup table entry
 */
typedef struct {
    uint16_t offset;                /* Offset in 512-byte blocks */
    uint16_t length;                /* Track length in bytes */
} hfe_track_entry_t;

/**
 * Interleaved track data block
 * Head 0 and Head 1 data are interleaved in 256-byte chunks
 */
typedef struct {
    uint8_t side0[256];             /* First 256 bytes of head 0 */
    uint8_t side1[256];             /* First 256 bytes of head 1 */
} hfe_track_block_t;

#pragma pack(pop)

/*============================================================================
 * HEADER VALIDATION
 *============================================================================*/

/**
 * Check if header signature is valid
 */
static inline bool hfe_is_valid_signature(const hfe_header_t *hdr) {
    return (memcmp(hdr->signature, HFE_SIGNATURE_V1, 8) == 0) ||
           (memcmp(hdr->signature, HFE_SIGNATURE_V3, 8) == 0);
}

/**
 * Check if header is V3 format
 */
static inline bool hfe_is_v3(const hfe_header_t *hdr) {
    return memcmp(hdr->signature, HFE_SIGNATURE_V3, 8) == 0;
}

/**
 * Validate header structure
 */
static inline bool hfe_is_valid_header(const hfe_header_t *hdr) {
    if (!hfe_is_valid_signature(hdr)) return false;
    
    /* V1 must have format_revision = 0 */
    if (!hfe_is_v3(hdr) && hdr->format_revision != 0) return false;
    
    /* Must have at least 1 cylinder */
    if (hdr->n_cylinders == 0) return false;
    
    /* Must have 1 or 2 heads */
    if (hdr->n_heads == 0 || hdr->n_heads > 2) return false;
    
    /* Bit rate must be positive */
    if (hdr->data_bit_rate == 0) return false;
    
    /* Track list must start after header */
    if (hdr->track_list_offset == 0) return false;
    
    return true;
}

/*============================================================================
 * HEADER INITIALIZATION
 *============================================================================*/

/**
 * Initialize HFE header with defaults
 */
static inline void hfe_init_header(hfe_header_t *hdr, bool v3) {
    memset(hdr, 0, sizeof(hfe_header_t));
    
    if (v3) {
        memcpy(hdr->signature, HFE_SIGNATURE_V3, 8);
    } else {
        memcpy(hdr->signature, HFE_SIGNATURE_V1, 8);
    }
    
    hdr->format_revision = 0;
    hdr->n_cylinders = 80;
    hdr->n_heads = 2;
    hdr->track_encoding = HFE_ENC_ISOIBM_MFM;
    hdr->data_bit_rate = 250;       /* DD */
    hdr->drive_rpm = 300;
    hdr->uft_floppy_interface = HFE_IF_GENERIC_SHUGART;
    hdr->track_list_offset = 1;     /* Immediately after header */
    hdr->write_allowed = 0xFF;      /* Writeable */
    hdr->single_step = 0xFF;        /* Single step */
    hdr->track0s0_altencoding = 0xFF;   /* Disabled */
    hdr->track0s1_altencoding = 0xFF;   /* Disabled */
}

/*============================================================================
 * TRACK DATA HANDLING
 *============================================================================*/

/**
 * Calculate offset for track lookup table entry
 * 
 * @param track_list_offset From header (in 512-byte blocks)
 * @param cylinder Cylinder number
 * @return Byte offset in file
 */
static inline uint32_t hfe_track_entry_offset(uint16_t track_list_offset, 
                                               uint8_t cylinder) {
    return (uint32_t)track_list_offset * HFE_TRACK_BLOCK_SIZE +
           cylinder * sizeof(hfe_track_entry_t);
}

/**
 * Calculate byte offset for track data
 * 
 * @param entry Track lookup table entry
 * @return Byte offset in file
 */
static inline uint32_t hfe_track_data_offset(const hfe_track_entry_t *entry) {
    return (uint32_t)entry->offset * HFE_TRACK_BLOCK_SIZE;
}

/**
 * Calculate number of 512-byte blocks needed for track
 * 
 * @param track_len Track length in bytes (from entry)
 * @return Number of blocks
 */
static inline uint32_t hfe_track_blocks(uint16_t track_len) {
    /* Each block contains 256 bytes per head (interleaved) */
    return (track_len + 255) / 256;
}

/*============================================================================
 * BIT REVERSAL (HFE stores bits LSB-first)
 *============================================================================*/

/**
 * Reverse bits in a byte
 */
static inline uint8_t hfe_reverse_byte(uint8_t b) {
    b = ((b & 0xF0) >> 4) | ((b & 0x0F) << 4);
    b = ((b & 0xCC) >> 2) | ((b & 0x33) << 2);
    b = ((b & 0xAA) >> 1) | ((b & 0x55) << 1);
    return b;
}

/**
 * Reverse bits in buffer
 */
static inline void hfe_reverse_bits(uint8_t *buf, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        buf[i] = hfe_reverse_byte(buf[i]);
    }
}

/*============================================================================
 * BIT RATE AND TIMING
 *============================================================================*/

/**
 * Get cell time in nanoseconds from bit rate
 * 
 * @param bit_rate_kbps Bit rate in kbit/s
 * @return Cell time in ns
 */
static inline uint32_t hfe_cell_time_ns(uint16_t bit_rate_kbps) {
    if (bit_rate_kbps == 0) return 0;
    return 1000000 / bit_rate_kbps;     /* ns = 1e6 / kbps */
}

/**
 * Get standard bit rate for media type
 */
static inline uint16_t hfe_standard_bitrate(hfe_floppy_interface_t iface) {
    switch (iface) {
        case HFE_IF_IBMPC_HD:
        case HFE_IF_ATARIST_HD:
        case HFE_IF_AMIGA_HD:
        case HFE_IF_S950_HD:
            return 500;     /* HD = 500 kbit/s */
        
        case HFE_IF_IBMPC_ED:
            return 1000;    /* ED = 1 Mbit/s */
        
        default:
            return 250;     /* DD = 250 kbit/s */
    }
}

/**
 * Get standard RPM for media type
 */
static inline uint16_t hfe_standard_rpm(hfe_floppy_interface_t iface) {
    switch (iface) {
        case HFE_IF_IBMPC_HD:
            return 360;     /* 5.25" HD */
        default:
            return 300;     /* Most others */
    }
}

/*============================================================================
 * TRACK INTERLEAVING
 *============================================================================*/

/**
 * Extract single-head track data from interleaved HFE format
 * 
 * @param interleaved Source buffer (interleaved head data)
 * @param track_len Track length in bytes
 * @param head Head number (0 or 1)
 * @param output Output buffer (must be at least track_len bytes)
 */
static inline void hfe_deinterleave_track(const uint8_t *interleaved,
                                           uint16_t track_len,
                                           uint8_t head,
                                           uint8_t *output) {
    uint32_t out_pos = 0;
    uint32_t n_blocks = hfe_track_blocks(track_len);
    
    for (uint32_t block = 0; block < n_blocks && out_pos < track_len; block++) {
        const uint8_t *src = interleaved + block * HFE_TRACK_BLOCK_SIZE;
        
        /* Skip to correct head's data in block */
        if (head) {
            src += 256;
        }
        
        /* Copy up to 256 bytes from this block */
        uint32_t to_copy = track_len - out_pos;
        if (to_copy > 256) to_copy = 256;
        
        memcpy(output + out_pos, src, to_copy);
        out_pos += to_copy;
    }
}

/**
 * Create interleaved HFE format from two single-head tracks
 * 
 * @param head0 Head 0 data
 * @param head1 Head 1 data (can be NULL for single-sided)
 * @param track_len Track length in bytes
 * @param output Output buffer (must be hfe_track_blocks(track_len) * 512 bytes)
 */
static inline void hfe_interleave_track(const uint8_t *head0,
                                         const uint8_t *head1,
                                         uint16_t track_len,
                                         uint8_t *output) {
    uint32_t n_blocks = hfe_track_blocks(track_len);
    
    memset(output, 0, n_blocks * HFE_TRACK_BLOCK_SIZE);
    
    for (uint32_t block = 0; block < n_blocks; block++) {
        uint8_t *dst = output + block * HFE_TRACK_BLOCK_SIZE;
        uint32_t offset = block * 256;
        
        /* Copy head 0 data */
        uint32_t to_copy = track_len - offset;
        if (to_copy > 256) to_copy = 256;
        if (to_copy > 0 && head0) {
            memcpy(dst, head0 + offset, to_copy);
        }
        
        /* Copy head 1 data */
        if (to_copy > 0 && head1) {
            memcpy(dst + 256, head1 + offset, to_copy);
        }
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_HFE_FORMAT_H */
