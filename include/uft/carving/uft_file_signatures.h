/**
 * @file uft_file_signatures.h
 * @brief File Signature Database for Carving
 * 
 * Based on Foremost by Jesse Kornblum et al.
 * License: Public Domain (US Government)
 */

#ifndef UFT_FILE_SIGNATURES_H
#define UFT_FILE_SIGNATURES_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Constants
 *===========================================================================*/

#define UFT_SIG_MAX_HEADER_LEN   64
#define UFT_SIG_MAX_FOOTER_LEN   64
#define UFT_SIG_WILDCARD         '?'

/*===========================================================================
 * Structures
 *===========================================================================*/

/**
 * @brief File type signature definition
 */
typedef struct {
    const char *extension;      /**< File extension (e.g., "jpg") */
    const char *description;    /**< Human-readable description */
    
    const uint8_t *header;      /**< Header signature bytes */
    size_t header_len;          /**< Header length */
    
    const uint8_t *footer;      /**< Footer signature (NULL if none) */
    size_t footer_len;          /**< Footer length */
    
    size_t max_size;            /**< Maximum file size in bytes */
    bool case_sensitive;        /**< Case-sensitive matching */
    
    int builtin_handler;        /**< 0=config, 1=builtin extraction */
} uft_file_sig_t;

/*===========================================================================
 * File Signature Database
 *===========================================================================*/

/* JPEG signatures */
static const uint8_t UFT_SIG_JPEG_HDR[] = { 0xFF, 0xD8, 0xFF };
static const uint8_t UFT_SIG_JPEG_FTR[] = { 0xFF, 0xD9 };

/* PNG signature */
static const uint8_t UFT_SIG_PNG_HDR[] = { 0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A };
static const uint8_t UFT_SIG_PNG_FTR[] = { 0x49, 0x45, 0x4E, 0x44, 0xAE, 0x42, 0x60, 0x82 };

/* GIF signatures */
static const uint8_t UFT_SIG_GIF87_HDR[] = { 0x47, 0x49, 0x46, 0x38, 0x37, 0x61 };  /* GIF87a */
static const uint8_t UFT_SIG_GIF89_HDR[] = { 0x47, 0x49, 0x46, 0x38, 0x39, 0x61 };  /* GIF89a */
static const uint8_t UFT_SIG_GIF_FTR[] = { 0x00, 0x3B };

/* BMP signature */
static const uint8_t UFT_SIG_BMP_HDR[] = { 0x42, 0x4D };  /* "BM" */

/* TIFF signatures */
static const uint8_t UFT_SIG_TIFF_LE_HDR[] = { 0x49, 0x49, 0x2A, 0x00 };  /* Little-endian */
static const uint8_t UFT_SIG_TIFF_BE_HDR[] = { 0x4D, 0x4D, 0x00, 0x2A };  /* Big-endian */

/* PDF signature */
static const uint8_t UFT_SIG_PDF_HDR[] = { 0x25, 0x50, 0x44, 0x46, 0x2D };  /* "%PDF-" */
static const uint8_t UFT_SIG_PDF_FTR[] = { 0x25, 0x25, 0x45, 0x4F, 0x46 };  /* "%%EOF" */

/* ZIP/Office signature */
static const uint8_t UFT_SIG_ZIP_HDR[] = { 0x50, 0x4B, 0x03, 0x04 };

/* RAR signatures */
static const uint8_t UFT_SIG_RAR_HDR[] = { 0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00 };
static const uint8_t UFT_SIG_RAR5_HDR[] = { 0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x01, 0x00 };

/* 7-Zip signature */
static const uint8_t UFT_SIG_7Z_HDR[] = { 0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C };

/* GZIP signature */
static const uint8_t UFT_SIG_GZIP_HDR[] = { 0x1F, 0x8B, 0x08 };

/* MS Office OLE */
static const uint8_t UFT_SIG_OLE_HDR[] = { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };

/* AVI/RIFF */
static const uint8_t UFT_SIG_RIFF_HDR[] = { 0x52, 0x49, 0x46, 0x46 };  /* "RIFF" */
static const uint8_t UFT_SIG_AVI_TYPE[] = { 0x41, 0x56, 0x49, 0x20 };  /* "AVI " */
static const uint8_t UFT_SIG_WAV_TYPE[] = { 0x57, 0x41, 0x56, 0x45 };  /* "WAVE" */

/* MP3 signatures */
static const uint8_t UFT_SIG_MP3_ID3_HDR[] = { 0x49, 0x44, 0x33 };      /* "ID3" */
static const uint8_t UFT_SIG_MP3_SYNC_HDR[] = { 0xFF, 0xFB };           /* Frame sync */

/* MP4/MOV */
static const uint8_t UFT_SIG_FTYP[] = { 0x66, 0x74, 0x79, 0x70 };       /* "ftyp" at offset 4 */

/* EXE/DLL (MZ) */
static const uint8_t UFT_SIG_MZ_HDR[] = { 0x4D, 0x5A };

/* ELF */
static const uint8_t UFT_SIG_ELF_HDR[] = { 0x7F, 0x45, 0x4C, 0x46 };

/* Java class */
static const uint8_t UFT_SIG_CLASS_HDR[] = { 0xCA, 0xFE, 0xBA, 0xBE };

/* SQLite */
static const uint8_t UFT_SIG_SQLITE_HDR[] = { 0x53, 0x51, 0x4C, 0x69, 0x74, 0x65, 0x20, 0x66, 
                                               0x6F, 0x72, 0x6D, 0x61, 0x74, 0x20, 0x33, 0x00 };

/* Windows Registry */
static const uint8_t UFT_SIG_REGF_HDR[] = { 0x72, 0x65, 0x67, 0x66 };  /* "regf" */

/* Mach-O (macOS executable) */
static const uint8_t UFT_SIG_MACHO32_HDR[] = { 0xFE, 0xED, 0xFA, 0xCE };
static const uint8_t UFT_SIG_MACHO64_HDR[] = { 0xFE, 0xED, 0xFA, 0xCF };

/* DMG (Apple Disk Image) */
static const uint8_t UFT_SIG_DMG_FTR[] = { 0x6B, 0x6F, 0x6C, 0x79 };  /* "koly" at end */

/*===========================================================================
 * Signature Table
 *===========================================================================*/

static const uft_file_sig_t uft_file_signatures[] = {
    /* Images */
    { "jpg",  "JPEG Image", UFT_SIG_JPEG_HDR, 3, UFT_SIG_JPEG_FTR, 2, 20*1024*1024, true, 1 },
    { "png",  "PNG Image", UFT_SIG_PNG_HDR, 8, UFT_SIG_PNG_FTR, 8, 200*1024*1024, true, 1 },
    { "gif",  "GIF Image (87a)", UFT_SIG_GIF87_HDR, 6, UFT_SIG_GIF_FTR, 2, 155*1024*1024, true, 1 },
    { "gif",  "GIF Image (89a)", UFT_SIG_GIF89_HDR, 6, UFT_SIG_GIF_FTR, 2, 155*1024*1024, true, 1 },
    { "bmp",  "BMP Image", UFT_SIG_BMP_HDR, 2, NULL, 0, 100*1024*1024, true, 1 },
    { "tif",  "TIFF Image (LE)", UFT_SIG_TIFF_LE_HDR, 4, NULL, 0, 200*1024*1024, true, 0 },
    { "tif",  "TIFF Image (BE)", UFT_SIG_TIFF_BE_HDR, 4, NULL, 0, 200*1024*1024, true, 0 },
    
    /* Documents */
    { "pdf",  "PDF Document", UFT_SIG_PDF_HDR, 5, UFT_SIG_PDF_FTR, 5, 50*1024*1024, true, 1 },
    { "doc",  "MS Office OLE", UFT_SIG_OLE_HDR, 8, NULL, 0, 50*1024*1024, true, 1 },
    
    /* Archives */
    { "zip",  "ZIP Archive", UFT_SIG_ZIP_HDR, 4, NULL, 0, 100*1024*1024, true, 1 },
    { "rar",  "RAR Archive", UFT_SIG_RAR_HDR, 7, NULL, 0, 100*1024*1024, true, 1 },
    { "rar",  "RAR5 Archive", UFT_SIG_RAR5_HDR, 8, NULL, 0, 100*1024*1024, true, 1 },
    { "7z",   "7-Zip Archive", UFT_SIG_7Z_HDR, 6, NULL, 0, 100*1024*1024, true, 0 },
    { "gz",   "GZIP Archive", UFT_SIG_GZIP_HDR, 3, NULL, 0, 100*1024*1024, true, 0 },
    
    /* Audio/Video */
    { "avi",  "AVI Video", UFT_SIG_RIFF_HDR, 4, NULL, 0, 1024*1024*1024, true, 1 },
    { "wav",  "WAV Audio", UFT_SIG_RIFF_HDR, 4, NULL, 0, 200*1024*1024, true, 1 },
    { "mp3",  "MP3 Audio (ID3)", UFT_SIG_MP3_ID3_HDR, 3, NULL, 0, 100*1024*1024, true, 0 },
    
    /* Executables */
    { "exe",  "DOS/Windows EXE", UFT_SIG_MZ_HDR, 2, NULL, 0, 100*1024*1024, true, 0 },
    { "elf",  "ELF Executable", UFT_SIG_ELF_HDR, 4, NULL, 0, 100*1024*1024, true, 0 },
    { "class","Java Class", UFT_SIG_CLASS_HDR, 4, NULL, 0, 10*1024*1024, true, 0 },
    
    /* Databases */
    { "sqlite","SQLite Database", UFT_SIG_SQLITE_HDR, 16, NULL, 0, 1024*1024*1024, true, 0 },
    
    /* System */
    { "reg",  "Windows Registry", UFT_SIG_REGF_HDR, 4, NULL, 0, 100*1024*1024, true, 0 },
    
    { NULL, NULL, NULL, 0, NULL, 0, 0, false, 0 }  /* Terminator */
};

#define UFT_FILE_SIG_COUNT (sizeof(uft_file_signatures) / sizeof(uft_file_signatures[0]) - 1)

/*===========================================================================
 * API Functions
 *===========================================================================*/

/**
 * @brief Find matching signature for data
 * @param data Data buffer (at least 64 bytes)
 * @param len Data length
 * @return Matching signature or NULL
 */
const uft_file_sig_t *uft_sig_match_header(const uint8_t *data, size_t len);

/**
 * @brief Check if data matches signature footer
 * @param sig Signature to check
 * @param data Data buffer
 * @param len Data length
 * @return true if footer matches
 */
bool uft_sig_match_footer(const uft_file_sig_t *sig, const uint8_t *data, size_t len);

/**
 * @brief Boyer-Moore search with wildcard support
 * @param needle Pattern to search for
 * @param needle_len Pattern length
 * @param haystack Data to search
 * @param haystack_len Data length
 * @param case_sensitive Case-sensitive search
 * @return Pointer to match or NULL
 */
const uint8_t *uft_bm_search(const uint8_t *needle, size_t needle_len,
                             const uint8_t *haystack, size_t haystack_len,
                             bool case_sensitive);

/**
 * @brief Build Boyer-Moore skip table
 * @param needle Pattern
 * @param needle_len Pattern length
 * @param table Output table (256 entries)
 */
void uft_bm_build_table(const uint8_t *needle, size_t needle_len, size_t table[256]);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FILE_SIGNATURES_H */
