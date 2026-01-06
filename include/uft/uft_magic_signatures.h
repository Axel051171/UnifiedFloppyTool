/**
 * @file uft_magic_signatures.h
 * @brief File Type Detection via Magic Byte Signatures
 * 
 * Extracted from fileextractor project
 * Source: /home/claude/fileextractor/fileextractor-master/signatures.py
 * 
 * Signature-based file type detection for carving files
 * from disk images and raw data.
 */

#ifndef UFT_MAGIC_SIGNATURES_H
#define UFT_MAGIC_SIGNATURES_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/*============================================================================
 * SIGNATURE DETECTION TYPES
 *============================================================================*/

typedef enum {
    UFT_SIG_END_SEQUENCE    = 0,    /* File ends with specific bytes */
    UFT_SIG_FILE_SIZE       = 1,    /* Size encoded in header */
    UFT_SIG_MANUAL          = 2     /* Custom parsing required */
} uft_sig_type_t;

/*============================================================================
 * FILE TYPE IDENTIFIERS
 *============================================================================*/

typedef enum {
    UFT_FILE_UNKNOWN = 0,
    /* Images */
    UFT_FILE_JPEG,
    UFT_FILE_JPEG_EXIF,
    UFT_FILE_BMP,
    UFT_FILE_GIF,
    UFT_FILE_PNG,
    UFT_FILE_TIFF,
    UFT_FILE_CRW,           /* Canon RAW */
    UFT_FILE_CR2,           /* Canon RAW v2 */
    UFT_FILE_THM,           /* Canon thumbnail */
    /* Audio */
    UFT_FILE_WAV,
    UFT_FILE_MP3,
    UFT_FILE_OGG,
    /* Documents */
    UFT_FILE_PDF,
    UFT_FILE_DOC,           /* MS Word */
    UFT_FILE_XLS,           /* MS Excel */
    UFT_FILE_PPT,           /* MS PowerPoint */
    UFT_FILE_DOCX,          /* Office Open XML */
    /* Archives */
    UFT_FILE_ZIP,
    UFT_FILE_RAR,
    UFT_FILE_GZIP,
    UFT_FILE_BZIP2,
    UFT_FILE_7Z,
    /* Executables */
    UFT_FILE_EXE_MZ,        /* DOS/Windows EXE */
    UFT_FILE_ELF,           /* Linux ELF */
    UFT_FILE_MACHO,         /* macOS Mach-O */
    /* Disk Images */
    UFT_FILE_ADF,           /* Amiga Disk File */
    UFT_FILE_D64,           /* C64 disk image */
    UFT_FILE_ISO,           /* ISO 9660 */
    /* Floppy Formats */
    UFT_FILE_IPF,           /* Interchangeable Preservation Format */
    UFT_FILE_SCP,           /* SuperCard Pro */
    UFT_FILE_HFE,           /* HxC Floppy Emulator */
    UFT_FILE_KRYOFLUX,      /* KryoFlux stream */
    /* Count */
    UFT_FILE_TYPE_COUNT
} uft_file_type_t;

/*============================================================================
 * SIGNATURE STRUCTURE
 *============================================================================*/

#define UFT_MAX_SIG_LEN         32      /* Maximum signature length */
#define UFT_MAX_END_SIG_LEN     16      /* Maximum end sequence length */

/**
 * File signature definition
 */
typedef struct {
    uft_file_type_t type;               /* File type identifier */
    const char *name;                   /* Type name string */
    const char *extension;              /* File extension */
    const char *description;            /* Human-readable description */
    
    uft_sig_type_t sig_type;            /* Detection method */
    
    /* Start signature */
    uint8_t start_sig[UFT_MAX_SIG_LEN]; /* Start sequence bytes */
    uint8_t start_mask[UFT_MAX_SIG_LEN];/* Mask (0xFF=exact, 0x00=any) */
    uint8_t start_len;                  /* Start sequence length */
    
    /* For TYPE_END_SEQUENCE */
    uint8_t end_sig[UFT_MAX_END_SIG_LEN];/* End sequence bytes */
    uint8_t end_len;                    /* End sequence length */
    uint8_t skip_end_count;             /* Number of end markers to skip */
    
    /* For TYPE_FILE_SIZE */
    uint8_t size_offset;                /* Offset to size field */
    uint8_t size_len;                   /* Size field length (1-4) */
    bool size_little_endian;            /* True for little-endian */
    int32_t size_correction;            /* Add to size value */
} uft_signature_t;

/*============================================================================
 * BUILT-IN SIGNATURES
 *============================================================================*/

static const uft_signature_t UFT_SIGNATURES[] = {
    /* JPEG (with EXIF) */
    {
        .type = UFT_FILE_JPEG_EXIF,
        .name = "JPEG/EXIF",
        .extension = "jpg",
        .description = "JPEG Image with EXIF",
        .sig_type = UFT_SIG_END_SEQUENCE,
        .start_sig = { 0xFF, 0xD8, 0xFF, 0xE1 },
        .start_mask = { 0xFF, 0xFF, 0xFF, 0xFF },
        .start_len = 4,
        .end_sig = { 0xFF, 0xD9 },
        .end_len = 2,
        .skip_end_count = 1
    },
    /* JPEG (standard) */
    {
        .type = UFT_FILE_JPEG,
        .name = "JPEG",
        .extension = "jpg",
        .description = "JPEG Image File",
        .sig_type = UFT_SIG_END_SEQUENCE,
        .start_sig = { 0xFF, 0xD8, 0xFF },
        .start_mask = { 0xFF, 0xFF, 0xFF },
        .start_len = 3,
        .end_sig = { 0xFF, 0xD9 },
        .end_len = 2,
        .skip_end_count = 0
    },
    /* BMP */
    {
        .type = UFT_FILE_BMP,
        .name = "BMP",
        .extension = "bmp",
        .description = "Bitmap Image File",
        .sig_type = UFT_SIG_FILE_SIZE,
        .start_sig = { 0x42, 0x4D },  /* "BM" */
        .start_mask = { 0xFF, 0xFF },
        .start_len = 2,
        .size_offset = 2,
        .size_len = 4,
        .size_little_endian = true,
        .size_correction = 0
    },
    /* GIF */
    {
        .type = UFT_FILE_GIF,
        .name = "GIF",
        .extension = "gif",
        .description = "GIF Image File",
        .sig_type = UFT_SIG_END_SEQUENCE,
        .start_sig = { 0x47, 0x49, 0x46, 0x38 },  /* "GIF8" */
        .start_mask = { 0xFF, 0xFF, 0xFF, 0xFF },
        .start_len = 4,
        .end_sig = { 0x00, 0x3B },
        .end_len = 2,
        .skip_end_count = 0
    },
    /* PNG */
    {
        .type = UFT_FILE_PNG,
        .name = "PNG",
        .extension = "png",
        .description = "PNG Image File",
        .sig_type = UFT_SIG_END_SEQUENCE,
        .start_sig = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A },
        .start_mask = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
        .start_len = 8,
        .end_sig = { 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82 },  /* IEND + CRC */
        .end_len = 8,
        .skip_end_count = 0
    },
    /* WAV */
    {
        .type = UFT_FILE_WAV,
        .name = "WAV",
        .extension = "wav",
        .description = "WAV Audio File",
        .sig_type = UFT_SIG_FILE_SIZE,
        .start_sig = { 0x52, 0x49, 0x46, 0x46 },  /* "RIFF" */
        .start_mask = { 0xFF, 0xFF, 0xFF, 0xFF },
        .start_len = 4,
        .size_offset = 4,
        .size_len = 4,
        .size_little_endian = true,
        .size_correction = 8    /* Add 8 to size */
    },
    /* MP3 (with ID3) */
    {
        .type = UFT_FILE_MP3,
        .name = "MP3",
        .extension = "mp3",
        .description = "MP3 Audio File",
        .sig_type = UFT_SIG_END_SEQUENCE,
        .start_sig = { 0x49, 0x44, 0x33 },  /* "ID3" */
        .start_mask = { 0xFF, 0xFF, 0xFF },
        .start_len = 3,
        .end_sig = { 0xFF, 0xFB },  /* Frame sync (approximate) */
        .end_len = 0,   /* No reliable end marker - use manual */
        .skip_end_count = 0
    },
    /* PDF */
    {
        .type = UFT_FILE_PDF,
        .name = "PDF",
        .extension = "pdf",
        .description = "PDF Document",
        .sig_type = UFT_SIG_END_SEQUENCE,
        .start_sig = { 0x25, 0x50, 0x44, 0x46 },  /* "%PDF" */
        .start_mask = { 0xFF, 0xFF, 0xFF, 0xFF },
        .start_len = 4,
        .end_sig = { 0x25, 0x25, 0x45, 0x4F, 0x46 },  /* "%%EOF" */
        .end_len = 5,
        .skip_end_count = 0
    },
    /* ZIP */
    {
        .type = UFT_FILE_ZIP,
        .name = "ZIP",
        .extension = "zip",
        .description = "ZIP Archive",
        .sig_type = UFT_SIG_END_SEQUENCE,
        .start_sig = { 0x50, 0x4B, 0x03, 0x04 },  /* "PK\x03\x04" */
        .start_mask = { 0xFF, 0xFF, 0xFF, 0xFF },
        .start_len = 4,
        .end_sig = { 0x50, 0x4B, 0x05, 0x06 },  /* End of central directory */
        .end_len = 4,
        .skip_end_count = 0
    },
    /* RAR */
    {
        .type = UFT_FILE_RAR,
        .name = "RAR",
        .extension = "rar",
        .description = "RAR Archive",
        .sig_type = UFT_SIG_END_SEQUENCE,
        .start_sig = { 0x52, 0x61, 0x72, 0x21, 0x1A, 0x07 },  /* "Rar!\x1a\x07" */
        .start_mask = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
        .start_len = 6,
        .end_sig = { 0 },
        .end_len = 0,
        .skip_end_count = 0
    },
    /* GZIP */
    {
        .type = UFT_FILE_GZIP,
        .name = "GZIP",
        .extension = "gz",
        .description = "GZIP Compressed File",
        .sig_type = UFT_SIG_MANUAL,
        .start_sig = { 0x1F, 0x8B, 0x08 },
        .start_mask = { 0xFF, 0xFF, 0xFF },
        .start_len = 3,
    },
    /* EXE (DOS/Windows) */
    {
        .type = UFT_FILE_EXE_MZ,
        .name = "EXE",
        .extension = "exe",
        .description = "DOS/Windows Executable",
        .sig_type = UFT_SIG_MANUAL,
        .start_sig = { 0x4D, 0x5A },  /* "MZ" */
        .start_mask = { 0xFF, 0xFF },
        .start_len = 2,
    },
    /* ELF */
    {
        .type = UFT_FILE_ELF,
        .name = "ELF",
        .extension = "",
        .description = "ELF Executable",
        .sig_type = UFT_SIG_MANUAL,
        .start_sig = { 0x7F, 0x45, 0x4C, 0x46 },  /* "\x7fELF" */
        .start_mask = { 0xFF, 0xFF, 0xFF, 0xFF },
        .start_len = 4,
    },
    /* IPF (SPS) */
    {
        .type = UFT_FILE_IPF,
        .name = "IPF",
        .extension = "ipf",
        .description = "Interchangeable Preservation Format",
        .sig_type = UFT_SIG_MANUAL,
        .start_sig = { 0x43, 0x41, 0x50, 0x53 },  /* "CAPS" */
        .start_mask = { 0xFF, 0xFF, 0xFF, 0xFF },
        .start_len = 4,
    },
    /* SCP */
    {
        .type = UFT_FILE_SCP,
        .name = "SCP",
        .extension = "scp",
        .description = "SuperCard Pro Image",
        .sig_type = UFT_SIG_MANUAL,
        .start_sig = { 0x53, 0x43, 0x50 },  /* "SCP" */
        .start_mask = { 0xFF, 0xFF, 0xFF },
        .start_len = 3,
    },
    /* HFE */
    {
        .type = UFT_FILE_HFE,
        .name = "HFE",
        .extension = "hfe",
        .description = "HxC Floppy Emulator Image",
        .sig_type = UFT_SIG_MANUAL,
        .start_sig = { 0x48, 0x58, 0x43 },  /* "HXC" */
        .start_mask = { 0xFF, 0xFF, 0xFF },
        .start_len = 3,
    },
    /* Canon CRW */
    {
        .type = UFT_FILE_CRW,
        .name = "CRW",
        .extension = "crw",
        .description = "Canon RAW Image",
        .sig_type = UFT_SIG_MANUAL,
        .start_sig = { 0x49, 0x49, 0x1A, 0x00, 0x00, 0x00, 0x48, 0x45, 0x41, 0x50, 0x43, 0x43, 0x44, 0x52 },
        .start_mask = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
        .start_len = 14,
    },
    /* Canon CR2 */
    {
        .type = UFT_FILE_CR2,
        .name = "CR2",
        .extension = "cr2",
        .description = "Canon RAW v2 Image",
        .sig_type = UFT_SIG_END_SEQUENCE,
        .start_sig = { 0x49, 0x49, 0x2A, 0x00, 0x10, 0x00, 0x00, 0x00, 0x43, 0x52, 0x02, 0x00 },
        .start_mask = { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF },
        .start_len = 12,
        .end_sig = { 0xFF, 0xD9 },
        .end_len = 2,
        .skip_end_count = 2
    },
};

#define UFT_SIGNATURE_COUNT (sizeof(UFT_SIGNATURES) / sizeof(UFT_SIGNATURES[0]))

/*============================================================================
 * SIGNATURE MATCHING FUNCTIONS
 *============================================================================*/

/**
 * Check if data matches signature start sequence
 * 
 * @param data Data buffer
 * @param len Data length
 * @param sig Signature to match
 * @return true if matches
 */
static inline bool uft_sig_match_start(const uint8_t *data, uint32_t len,
                                        const uft_signature_t *sig) {
    if (len < sig->start_len) return false;
    
    for (uint8_t i = 0; i < sig->start_len; i++) {
        if (sig->start_mask[i] != 0x00) {
            if ((data[i] & sig->start_mask[i]) != (sig->start_sig[i] & sig->start_mask[i])) {
                return false;
            }
        }
    }
    return true;
}

/**
 * Find end sequence in data
 * 
 * @param data Data buffer
 * @param len Data length
 * @param sig Signature with end sequence
 * @param skip_count Number of matches to skip
 * @return Offset past end sequence, or 0 if not found
 */
static inline uint32_t uft_sig_find_end(const uint8_t *data, uint32_t len,
                                         const uft_signature_t *sig,
                                         uint8_t skip_count) {
    if (sig->end_len == 0) return 0;
    
    uint8_t found = 0;
    for (uint32_t i = 0; i + sig->end_len <= len; i++) {
        bool match = true;
        for (uint8_t j = 0; j < sig->end_len; j++) {
            if (data[i + j] != sig->end_sig[j]) {
                match = false;
                break;
            }
        }
        if (match) {
            if (found >= skip_count) {
                return i + sig->end_len;
            }
            found++;
        }
    }
    return 0;
}

/**
 * Read file size from header
 * 
 * @param data Data buffer (starting at file)
 * @param sig Signature with size info
 * @return File size, or 0 if invalid
 */
static inline uint32_t uft_sig_read_size(const uint8_t *data, 
                                          const uft_signature_t *sig) {
    if (sig->sig_type != UFT_SIG_FILE_SIZE) return 0;
    
    const uint8_t *p = data + sig->size_offset;
    uint32_t size = 0;
    
    if (sig->size_little_endian) {
        for (int i = sig->size_len - 1; i >= 0; i--) {
            size = (size << 8) | p[i];
        }
    } else {
        for (int i = 0; i < sig->size_len; i++) {
            size = (size << 8) | p[i];
        }
    }
    
    return size + sig->size_correction;
}

/**
 * Detect file type from data
 * 
 * @param data Data buffer
 * @param len Data length
 * @return File type, or UFT_FILE_UNKNOWN
 */
static inline uft_file_type_t uft_detect_file_type(const uint8_t *data, 
                                                    uint32_t len) {
    for (uint32_t i = 0; i < UFT_SIGNATURE_COUNT; i++) {
        if (uft_sig_match_start(data, len, &UFT_SIGNATURES[i])) {
            return UFT_SIGNATURES[i].type;
        }
    }
    return UFT_FILE_UNKNOWN;
}

/**
 * Get signature by file type
 */
static inline const uft_signature_t* uft_get_signature(uft_file_type_t type) {
    for (uint32_t i = 0; i < UFT_SIGNATURE_COUNT; i++) {
        if (UFT_SIGNATURES[i].type == type) {
            return &UFT_SIGNATURES[i];
        }
    }
    return NULL;
}

/**
 * Determine file size using signature
 * 
 * @param data Data buffer
 * @param len Available data length
 * @param sig Signature
 * @return Detected file size, or 0 if unknown
 */
static inline uint32_t uft_determine_file_size(const uint8_t *data, uint32_t len,
                                                const uft_signature_t *sig) {
    switch (sig->sig_type) {
        case UFT_SIG_END_SEQUENCE:
            return uft_sig_find_end(data, len, sig, sig->skip_end_count);
            
        case UFT_SIG_FILE_SIZE:
            return uft_sig_read_size(data, sig);
            
        case UFT_SIG_MANUAL:
        default:
            return 0;   /* Requires custom handling */
    }
}

#ifdef __cplusplus
}
#endif

#endif /* UFT_MAGIC_SIGNATURES_H */
