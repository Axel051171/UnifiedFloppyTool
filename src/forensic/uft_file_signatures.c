/**
 * @file uft_file_signatures.c
 * @brief File signature (magic byte) detection implementation
 *
 * 140+ file signatures for forensic analysis and file recovery
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "uft/forensic/uft_file_signatures.h"
#include <string.h>
#include <ctype.h>

/* Portability for strcasecmp */
#ifdef _WIN32
#define strcasecmp _stricmp
#else
#include <strings.h>
#endif

/* ============================================================================
 * Magic Byte Definitions - IMAGES
 * ============================================================================ */

static const uint8_t SIG_JPEG_APP0[]   = { 0xFF, 0xD8, 0xFF, 0xE0 };
static const uint8_t SIG_JPEG_APP1[]   = { 0xFF, 0xD8, 0xFF, 0xE1 };
static const uint8_t SIG_JPEG_FOOTER[] = { 0xFF, 0xD9 };

static const uint8_t SIG_PNG[]         = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
static const uint8_t SIG_PNG_FOOTER[]  = { 'I', 'E', 'N', 'D' };

static const uint8_t SIG_GIF87A[]      = { 'G', 'I', 'F', '8', '7', 'a' };
static const uint8_t SIG_GIF89A[]      = { 'G', 'I', 'F', '8', '9', 'a' };
static const uint8_t SIG_GIF_FOOTER[]  = { 0x00, 0x3B };

static const uint8_t SIG_BMP[]         = { 'B', 'M' };
static const uint8_t SIG_TIFF_LE[]     = { 'I', 'I', 0x2A, 0x00 };
static const uint8_t SIG_TIFF_BE[]     = { 'M', 'M', 0x00, 0x2A };
static const uint8_t SIG_ICO[]         = { 0x00, 0x00, 0x01, 0x00 };
static const uint8_t SIG_PSD[]         = { '8', 'B', 'P', 'S' };
static const uint8_t SIG_XCF[]         = { 'g', 'i', 'm', 'p', ' ', 'x', 'c', 'f' };
static const uint8_t SIG_PCX[]         = { 0x0A, 0x05, 0x01, 0x08 };
static const uint8_t SIG_WMF[]         = { 0xD7, 0xCD, 0xC6, 0x9A };
static const uint8_t SIG_EMF[]         = { 0x01, 0x00, 0x00, 0x00 };

/* ----- Retro Image Formats ----- */
static const uint8_t SIG_MACPAINT[]    = { 0x00, 0x00, 0x00, 0x02 };  /* + zeros */
static const uint8_t SIG_IFF_FORM[]    = { 'F', 'O', 'R', 'M' };
static const uint8_t SIG_IFF_ILBM[]    = { 'I', 'L', 'B', 'M' };
static const uint8_t SIG_DEGAS_PI1[]   = { 0x00, 0x00 };  /* Needs size check */
static const uint8_t SIG_DEGAS_PI2[]   = { 0x00, 0x01 };
static const uint8_t SIG_DEGAS_PI3[]   = { 0x00, 0x02 };

/* ============================================================================
 * Magic Byte Definitions - AUDIO
 * ============================================================================ */

static const uint8_t SIG_MP3_ID3[]     = { 'I', 'D', '3' };
static const uint8_t SIG_MP3_SYNC[]    = { 0xFF, 0xFB };
static const uint8_t SIG_FLAC[]        = { 'f', 'L', 'a', 'C' };
static const uint8_t SIG_OGG[]         = { 'O', 'g', 'g', 'S' };
static const uint8_t SIG_MIDI[]        = { 'M', 'T', 'h', 'd' };
static const uint8_t SIG_WAV_RIFF[]    = { 'R', 'I', 'F', 'F' };
static const uint8_t SIG_AU[]          = { '.', 's', 'n', 'd' };
static const uint8_t SIG_AIFF[]        = { 'F', 'O', 'R', 'M' };
static const uint8_t SIG_AMR[]         = { '#', '!', 'A', 'M', 'R', '\n' };
static const uint8_t SIG_APE[]         = { 'M', 'A', 'C', ' ' };
static const uint8_t SIG_MOD[]         = { 'M', '.', 'K', '.' };  /* at offset 1080 */
static const uint8_t SIG_S3M[]         = { 'S', 'C', 'R', 'M' };  /* at offset 44 */
static const uint8_t SIG_XM[]          = { 'E', 'x', 't', 'e', 'n', 'd', 'e', 'd' };
static const uint8_t SIG_IT[]          = { 'I', 'M', 'P', 'M' };
static const uint8_t SIG_SID[]         = { 'P', 'S', 'I', 'D' };
static const uint8_t SIG_SID2[]        = { 'R', 'S', 'I', 'D' };

/* ============================================================================
 * Magic Byte Definitions - VIDEO
 * ============================================================================ */

static const uint8_t SIG_AVI[]         = { 'R', 'I', 'F', 'F' };
static const uint8_t SIG_MKV[]         = { 0x1A, 0x45, 0xDF, 0xA3 };
static const uint8_t SIG_FLV[]         = { 'F', 'L', 'V', 0x01 };
static const uint8_t SIG_MPG[]         = { 0x00, 0x00, 0x01, 0xBA };
static const uint8_t SIG_MP4_FTYP[]    = { 'f', 't', 'y', 'p' };

/* ============================================================================
 * Magic Byte Definitions - ARCHIVES
 * ============================================================================ */

static const uint8_t SIG_ZIP[]         = { 'P', 'K', 0x03, 0x04 };
static const uint8_t SIG_RAR[]         = { 'R', 'a', 'r', '!', 0x1A, 0x07, 0x00 };
static const uint8_t SIG_RAR5[]        = { 'R', 'a', 'r', '!', 0x1A, 0x07, 0x01, 0x00 };
static const uint8_t SIG_7Z[]          = { '7', 'z', 0xBC, 0xAF, 0x27, 0x1C };
static const uint8_t SIG_GZIP[]        = { 0x1F, 0x8B };
static const uint8_t SIG_BZIP2[]       = { 'B', 'Z', 'h' };
static const uint8_t SIG_XZ[]          = { 0xFD, '7', 'z', 'X', 'Z', 0x00 };
static const uint8_t SIG_ZSTD[]        = { 0x28, 0xB5, 0x2F, 0xFD };
static const uint8_t SIG_CAB[]         = { 'M', 'S', 'C', 'F' };
static const uint8_t SIG_ARJ[]         = { 0x60, 0xEA };
static const uint8_t SIG_LZH[]         = { '-', 'l', 'h' };
static const uint8_t SIG_ACE[]         = { '*', '*', 'A', 'C', 'E', '*', '*' };
static const uint8_t SIG_ZOO[]         = { 'Z', 'O', 'O', ' ' };
static const uint8_t SIG_ARC[]         = { 0x1A, 0x08 };
static const uint8_t SIG_PAK[]         = { 0x1A, 0x0A };

/* ============================================================================
 * Magic Byte Definitions - DOCUMENTS
 * ============================================================================ */

static const uint8_t SIG_PDF[]         = { '%', 'P', 'D', 'F', '-' };
static const uint8_t SIG_PDF_FOOTER[]  = { '%', '%', 'E', 'O', 'F' };
static const uint8_t SIG_OLE2[]        = { 0xD0, 0xCF, 0x11, 0xE0, 0xA1, 0xB1, 0x1A, 0xE1 };
static const uint8_t SIG_RTF[]         = { '{', '\\', 'r', 't', 'f', '1' };
static const uint8_t SIG_PS[]          = { '%', '!', 'P', 'S' };
static const uint8_t SIG_XML[]         = { '<', '?', 'x', 'm', 'l' };

/* ============================================================================
 * Magic Byte Definitions - RETRO / LEGACY
 * ============================================================================ */

/* Word Processors */
static const uint8_t SIG_WORDSTAR[]    = { 0x1D, 0x7D };  /* Common WS marker */
static const uint8_t SIG_WORDPERFECT[] = { 0xFF, 'W', 'P', 'C' };
static const uint8_t SIG_WP51[]        = { 0xFF, 'W', 'P', 'C', 0x10, 0x00 };
static const uint8_t SIG_MSWRITE[]     = { 0x31, 0xBE, 0x00, 0x00 };
static const uint8_t SIG_MSWRITE2[]    = { 0x32, 0xBE, 0x00, 0x00 };

/* Spreadsheets */
static const uint8_t SIG_LOTUS123_WK1[] = { 0x00, 0x00, 0x02, 0x00 };
static const uint8_t SIG_LOTUS123_WK3[] = { 0x00, 0x00, 0x1A, 0x00 };
static const uint8_t SIG_LOTUS123_WK4[] = { 0x00, 0x00, 0x1A, 0x00, 0x02, 0x10 };
static const uint8_t SIG_QUATTRO[]     = { 0x00, 0x00, 0x02, 0x00 };
static const uint8_t SIG_SYMPHONY[]    = { 0x00, 0x00, 0x1A, 0x00, 0x00, 0x10 };

/* Databases */
static const uint8_t SIG_DBASE3[]      = { 0x03 };
static const uint8_t SIG_DBASE4[]      = { 0x04 };
static const uint8_t SIG_DBASE5[]      = { 0x05 };
static const uint8_t SIG_FOXPRO[]      = { 0xF5 };
static const uint8_t SIG_CLIPPER[]     = { 0x03 };  /* Same as dBase */

/* ============================================================================
 * Magic Byte Definitions - DISK CONTAINERS
 * ============================================================================ */

/* Commodore */
static const uint8_t SIG_D64[]         = { 0x12, 0x01, 0x41, 0x00 };  /* BAM at T18,S0 */
static const uint8_t SIG_G64[]         = { 'G', 'C', 'R', '-', '1', '5', '4', '1' };
static const uint8_t SIG_D71[]         = { 0x12, 0x01, 0x41, 0x00 };  /* Similar to D64 */
static const uint8_t SIG_D81[]         = { 0x28, 0x03, 0x44, 0xBB };  /* BAM signature */
static const uint8_t SIG_T64[]         = { 'C', '6', '4', ' ', 't', 'a', 'p', 'e' };
static const uint8_t SIG_CRT[]         = { 'C', '6', '4', ' ', 'C', 'A', 'R', 'T' };
static const uint8_t SIG_PRG[]         = { 0x01, 0x08 };  /* Load address $0801 */
static const uint8_t SIG_TAP_C64[]     = { 'C', '6', '4', '-', 'T', 'A', 'P', 'E' };

/* Amiga */
static const uint8_t SIG_ADF[]         = { 'D', 'O', 'S' };  /* DOS0/DOS1/DOS2/DOS3 */
static const uint8_t SIG_ADZ[]         = { 0x1F, 0x8B };  /* gzipped ADF */

/* Atari ST */
static const uint8_t SIG_ST[]          = { 0x60, 0x1A };  /* Boot sector */
static const uint8_t SIG_STX[]         = { 'R', 'S', 'Y', 0x00 };  /* Pasti */
static const uint8_t SIG_MSA[]         = { 0x0E, 0x0F };

/* Apple */
static const uint8_t SIG_DSK_APPLE[]   = { 0x01, 0xA5, 0x27, 0xC9 };  /* ProDOS */
static const uint8_t SIG_NIB[]         = { 0xD5, 0xAA, 0x96 };  /* GCR sync */
static const uint8_t SIG_WOZ[]         = { 'W', 'O', 'Z', '1' };
static const uint8_t SIG_WOZ2[]        = { 'W', 'O', 'Z', '2' };
static const uint8_t SIG_2MG[]         = { '2', 'I', 'M', 'G' };
static const uint8_t SIG_PO[]          = { 0x00, 0x00, 0x03, 0x00 };  /* ProDOS order */

/* ZX Spectrum */
static const uint8_t SIG_TAP_ZX[]      = { 0x13, 0x00, 0x00 };  /* Header block */
static const uint8_t SIG_TZX[]         = { 'Z', 'X', 'T', 'a', 'p', 'e', '!' };
static const uint8_t SIG_Z80[]         = { 0x00, 0x00 };  /* A, F registers */
static const uint8_t SIG_SNA[]         = { 0x00 };  /* I register at offset 0 */

/* Amstrad CPC */
static const uint8_t SIG_DSK_CPC[]     = { 'M', 'V', ' ', '-', ' ', 'C', 'P', 'C' };
static const uint8_t SIG_DSK_EXT[]     = { 'E', 'X', 'T', 'E', 'N', 'D', 'E', 'D' };

/* Generic Flux/Container */
static const uint8_t SIG_SCP[]         = { 'S', 'C', 'P' };
static const uint8_t SIG_HFE[]         = { 'H', 'X', 'C', 'P', 'I', 'C', 'F', 'E' };
static const uint8_t SIG_HFE3[]        = { 'H', 'X', 'C', 'H', 'F', 'E', 'V', '3' };
static const uint8_t SIG_IPF[]         = { 'C', 'A', 'P', 'S' };
static const uint8_t SIG_IMD[]         = { 'I', 'M', 'D', ' ' };
static const uint8_t SIG_TD0[]         = { 'T', 'D' };
static const uint8_t SIG_TD0_ADV[]     = { 't', 'd' };  /* Advanced compression */
static const uint8_t SIG_FDI[]         = { 'F', 'o', 'r', 'm', 'a', 't', 't', 'e' };
static const uint8_t SIG_MFM[]         = { 'M', 'F', 'M', '_' };
static const uint8_t SIG_RAW_FLUX[]    = { 0xAA, 0xAA, 0xAA, 0xAA };

/* ============================================================================
 * Magic Byte Definitions - ROM / EMULATION
 * ============================================================================ */

/* Nintendo */
static const uint8_t SIG_NES[]         = { 'N', 'E', 'S', 0x1A };
static const uint8_t SIG_SNES[]        = { 0x00 };  /* Needs header analysis */
static const uint8_t SIG_SMC_HEADER[]  = { 0x00, 0x00, 0x00, 0x00 };  /* 512-byte header */
static const uint8_t SIG_GB[]          = { 0xCE, 0xED, 0x66, 0x66 };  /* at offset 0x104 */
static const uint8_t SIG_GBA[]         = { 0x24, 0xFF, 0xAE, 0x51 };  /* at offset 4 */
static const uint8_t SIG_N64[]         = { 0x80, 0x37, 0x12, 0x40 };
static const uint8_t SIG_NDS[]         = { 0x24, 0xFF, 0xAE, 0x51 };

/* Sega */
static const uint8_t SIG_SMS[]         = { 'T', 'M', 'R', ' ', 'S', 'E', 'G', 'A' };
static const uint8_t SIG_GEN[]         = { 'S', 'E', 'G', 'A' };  /* at offset 0x100 */
static const uint8_t SIG_32X[]         = { 'S', 'E', 'G', 'A', ' ', '3', '2', 'X' };

/* Atari */
static const uint8_t SIG_A26[]         = { 0x00 };  /* Size-based detection */
static const uint8_t SIG_A78[]         = { 0x01, 'A', 'T', 'A', 'R', 'I', '7', '8' };
static const uint8_t SIG_A8_XEX[]      = { 0xFF, 0xFF };
static const uint8_t SIG_A8_ATR[]      = { 0x96, 0x02 };

/* MSX */
static const uint8_t SIG_MSX_ROM[]     = { 'A', 'B' };  /* ROM header */
static const uint8_t SIG_MSX_CAS[]     = { 0x1F, 0xA6, 0xDE, 0xBA };

/* PC Engine */
static const uint8_t SIG_PCE[]         = { 0x00 };  /* Size-based */

/* Neo Geo */
static const uint8_t SIG_NEO_P[]       = { 0x10, 0x00 };

/* ============================================================================
 * Magic Byte Definitions - CAD / 3D
 * ============================================================================ */

static const uint8_t SIG_DWG_R14[]     = { 'A', 'C', '1', '0', '1', '4' };
static const uint8_t SIG_DWG_2000[]    = { 'A', 'C', '1', '0', '1', '5' };
static const uint8_t SIG_DWG_2004[]    = { 'A', 'C', '1', '0', '1', '8' };
static const uint8_t SIG_DWG_2007[]    = { 'A', 'C', '1', '0', '2', '1' };
static const uint8_t SIG_DXF[]         = { '0', '\r', '\n', 'S', 'E', 'C', 'T', 'I' };
static const uint8_t SIG_STL_ASCII[]   = { 's', 'o', 'l', 'i', 'd', ' ' };
static const uint8_t SIG_STL_BIN[]     = { 0x00 };  /* 80-byte header, then face count */
static const uint8_t SIG_OBJ[]         = { '#', ' ' };  /* Comment line */
static const uint8_t SIG_3DS[]         = { 0x4D, 0x4D, 0x00, 0x00 };
static const uint8_t SIG_STEP[]        = { 'I', 'S', 'O', '-', '1', '0', '3', '0' };
static const uint8_t SIG_IGES[]        = { ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ' };

/* ============================================================================
 * Magic Byte Definitions - EXECUTABLES
 * ============================================================================ */

static const uint8_t SIG_EXE_MZ[]      = { 'M', 'Z' };
static const uint8_t SIG_ELF[]         = { 0x7F, 'E', 'L', 'F' };
static const uint8_t SIG_MACHO32[]     = { 0xFE, 0xED, 0xFA, 0xCE };
static const uint8_t SIG_MACHO64[]     = { 0xFE, 0xED, 0xFA, 0xCF };
static const uint8_t SIG_JAVA_CLASS[]  = { 0xCA, 0xFE, 0xBA, 0xBE };
static const uint8_t SIG_COM[]         = { 0xE9 };
static const uint8_t SIG_COM_INT20[]   = { 0xCD, 0x20 };  /* INT 20h */

/* ============================================================================
 * Magic Byte Definitions - DATABASES
 * ============================================================================ */

static const uint8_t SIG_SQLITE[]      = { 'S', 'Q', 'L', 'i', 't', 'e', ' ', 'f', 'o', 'r', 'm', 'a', 't' };
static const uint8_t SIG_MDB[]         = { 0x00, 0x01, 0x00, 0x00, 'S', 't', 'a', 'n' };

/* ============================================================================
 * Magic Byte Definitions - DISK IMAGES
 * ============================================================================ */

static const uint8_t SIG_ISO9660[]     = { 'C', 'D', '0', '0', '1' };  /* at offset 32769 */
static const uint8_t SIG_VHD[]         = { 'c', 'o', 'n', 'e', 'c', 't', 'i', 'x' };
static const uint8_t SIG_VMDK[]        = { 'K', 'D', 'M', 'V' };
static const uint8_t SIG_QCOW[]        = { 'Q', 'F', 'I', 0xFB };

/* ============================================================================
 * Magic Byte Definitions - FONTS
 * ============================================================================ */

static const uint8_t SIG_TTF[]         = { 0x00, 0x01, 0x00, 0x00 };
static const uint8_t SIG_OTF[]         = { 'O', 'T', 'T', 'O' };
static const uint8_t SIG_WOFF[]        = { 'w', 'O', 'F', 'F' };
static const uint8_t SIG_WOFF2[]       = { 'w', 'O', 'F', '2' };

/* ============================================================================
 * Magic Byte Definitions - SYSTEM
 * ============================================================================ */

static const uint8_t SIG_NTFS[]        = { 'N', 'T', 'F', 'S', ' ', ' ', ' ', ' ' };
static const uint8_t SIG_GPT[]         = { 'E', 'F', 'I', ' ', 'P', 'A', 'R', 'T' };
static const uint8_t SIG_MBR[]         = { 0x55, 0xAA };  /* at offset 510 */
static const uint8_t SIG_FAT_BOOT[]    = { 0xEB };

/* ============================================================================
 * Magic Byte Definitions - SCIENTIFIC
 * ============================================================================ */

static const uint8_t SIG_FITS[]        = { 'S', 'I', 'M', 'P', 'L', 'E', ' ', ' ' };
static const uint8_t SIG_HDF5[]        = { 0x89, 'H', 'D', 'F', '\r', '\n', 0x1A, '\n' };
static const uint8_t SIG_NETCDF[]      = { 'C', 'D', 'F', 0x01 };
static const uint8_t SIG_MATLAB[]      = { 'M', 'A', 'T', 'L', 'A', 'B' };

/* ============================================================================
 * Magic Byte Definitions - EMAIL / PIM
 * ============================================================================ */

static const uint8_t SIG_PST[]         = { '!', 'B', 'D', 'N' };
static const uint8_t SIG_MBOX[]        = { 'F', 'r', 'o', 'm', ' ' };
static const uint8_t SIG_EML[]         = { 'R', 'e', 'c', 'e', 'i', 'v', 'e', 'd' };
static const uint8_t SIG_VCARD[]       = { 'B', 'E', 'G', 'I', 'N', ':', 'V', 'C' };
static const uint8_t SIG_ICAL[]        = { 'B', 'E', 'G', 'I', 'N', ':', 'V', 'C' };

/* ============================================================================
 * Magic Byte Definitions - CRYPTO / SECURITY
 * ============================================================================ */

static const uint8_t SIG_PGP_MSG[]     = { 0xA8 };
static const uint8_t SIG_PGP_PUB[]     = { 0x99 };
static const uint8_t SIG_PGP_SEC[]     = { 0x95 };
static const uint8_t SIG_PGP_SIG[]     = { 0x89 };
static const uint8_t SIG_SSH_PUB[]     = { 's', 's', 'h', '-' };
static const uint8_t SIG_PEM[]         = { '-', '-', '-', '-', '-', 'B', 'E', 'G' };
static const uint8_t SIG_DER[]         = { 0x30, 0x82 };
static const uint8_t SIG_PKCS12[]      = { 0x30, 0x82 };
static const uint8_t SIG_KDBX[]        = { 0x03, 0xD9, 0xA2, 0x9A };

/* ============================================================================
 * Magic Byte Definitions - OTHER
 * ============================================================================ */

static const uint8_t SIG_BLEND[]       = { 'B', 'L', 'E', 'N', 'D', 'E', 'R' };
static const uint8_t SIG_PCAP[]        = { 0xD4, 0xC3, 0xB2, 0xA1 };

/* ============================================================================
 * Signature Database
 * ============================================================================ */

/* MSVC doesn't accept ternary operator as constant initializer */
#define SIG_WITH_FOOTER(nm, ext, desc, cat, hdr, off, ftr, flg) \
    { nm, ext, desc, cat, hdr, sizeof(hdr), off, ftr, sizeof(ftr), flg }

#define SIG_NO_FOOTER(nm, ext, desc, cat, hdr, off, flg) \
    { nm, ext, desc, cat, hdr, sizeof(hdr), off, NULL, 0, flg }

#define SIG(nm, ext, desc, cat, hdr, off, ftr, flg) \
    SIG_WITH_FOOTER(nm, ext, desc, cat, hdr, off, ftr, flg)

#define SIG_SIMPLE(nm, ext, desc, cat, hdr) \
    { nm, ext, desc, cat, hdr, sizeof(hdr), 0, NULL, 0, UFT_SIG_FLAG_NONE }

#define SIG_FLOPPY(nm, ext, desc, cat, hdr) \
    { nm, ext, desc, cat, hdr, sizeof(hdr), 0, NULL, 0, UFT_SIG_FLAG_FLOPPY }

#define SIG_RETRO(nm, ext, desc, cat, hdr) \
    { nm, ext, desc, cat, hdr, sizeof(hdr), 0, NULL, 0, UFT_SIG_FLAG_RETRO | UFT_SIG_FLAG_FLOPPY }

static const uft_file_signature_t g_signatures[] = {
    /* ===== IMAGES ===== */
    SIG("JPEG", "jpg", "JPEG Image (APP0)", UFT_CAT_IMAGE, 
        SIG_JPEG_APP0, 0, SIG_JPEG_FOOTER, UFT_SIG_FLAG_HAS_FOOTER),
    SIG("JPEG", "jpg", "JPEG Image (EXIF)", UFT_CAT_IMAGE,
        SIG_JPEG_APP1, 0, SIG_JPEG_FOOTER, UFT_SIG_FLAG_HAS_FOOTER),
    SIG("PNG", "png", "Portable Network Graphics", UFT_CAT_IMAGE,
        SIG_PNG, 0, SIG_PNG_FOOTER, UFT_SIG_FLAG_HAS_FOOTER),
    SIG("GIF", "gif", "GIF Image (87a)", UFT_CAT_IMAGE,
        SIG_GIF87A, 0, SIG_GIF_FOOTER, UFT_SIG_FLAG_HAS_FOOTER),
    SIG("GIF", "gif", "GIF Image (89a)", UFT_CAT_IMAGE,
        SIG_GIF89A, 0, SIG_GIF_FOOTER, UFT_SIG_FLAG_HAS_FOOTER),
    SIG_FLOPPY("BMP", "bmp", "Windows Bitmap", UFT_CAT_IMAGE, SIG_BMP),
    SIG_SIMPLE("TIFF", "tif", "TIFF Image (LE)", UFT_CAT_IMAGE, SIG_TIFF_LE),
    SIG_SIMPLE("TIFF", "tif", "TIFF Image (BE)", UFT_CAT_IMAGE, SIG_TIFF_BE),
    SIG_SIMPLE("ICO", "ico", "Windows Icon", UFT_CAT_IMAGE, SIG_ICO),
    SIG_SIMPLE("PSD", "psd", "Adobe Photoshop", UFT_CAT_IMAGE, SIG_PSD),
    SIG_SIMPLE("XCF", "xcf", "GIMP Image", UFT_CAT_IMAGE, SIG_XCF),
    SIG_FLOPPY("PCX", "pcx", "PC Paintbrush", UFT_CAT_IMAGE, SIG_PCX),
    SIG_FLOPPY("WMF", "wmf", "Windows Metafile", UFT_CAT_IMAGE, SIG_WMF),
    SIG_RETRO("MacPaint", "pntg", "MacPaint Image", UFT_CAT_IMAGE, SIG_MACPAINT),
    SIG_RETRO("IFF/ILBM", "iff", "Amiga IFF Image", UFT_CAT_IMAGE, SIG_IFF_FORM),

    /* ===== AUDIO ===== */
    SIG_SIMPLE("MP3", "mp3", "MP3 Audio (ID3)", UFT_CAT_AUDIO, SIG_MP3_ID3),
    SIG_SIMPLE("MP3", "mp3", "MP3 Audio (Sync)", UFT_CAT_AUDIO, SIG_MP3_SYNC),
    SIG_SIMPLE("FLAC", "flac", "Free Lossless Audio", UFT_CAT_AUDIO, SIG_FLAC),
    SIG_SIMPLE("OGG", "ogg", "Ogg Vorbis", UFT_CAT_AUDIO, SIG_OGG),
    SIG_FLOPPY("MIDI", "mid", "MIDI Music", UFT_CAT_AUDIO, SIG_MIDI),
    SIG_SIMPLE("WAV", "wav", "RIFF WAVE Audio", UFT_CAT_AUDIO, SIG_WAV_RIFF),
    SIG_SIMPLE("AU", "au", "Sun Audio", UFT_CAT_AUDIO, SIG_AU),
    SIG_SIMPLE("AMR", "amr", "AMR Audio", UFT_CAT_AUDIO, SIG_AMR),
    SIG_RETRO("MOD", "mod", "Amiga ProTracker Module", UFT_CAT_AUDIO, SIG_MOD),
    SIG_RETRO("S3M", "s3m", "Scream Tracker 3 Module", UFT_CAT_AUDIO, SIG_S3M),
    SIG_RETRO("XM", "xm", "FastTracker II Module", UFT_CAT_AUDIO, SIG_XM),
    SIG_RETRO("IT", "it", "Impulse Tracker Module", UFT_CAT_AUDIO, SIG_IT),
    SIG_RETRO("SID", "sid", "C64 SID Music", UFT_CAT_AUDIO, SIG_SID),
    SIG_RETRO("RSID", "sid", "C64 RSID Music", UFT_CAT_AUDIO, SIG_SID2),

    /* ===== VIDEO ===== */
    SIG_SIMPLE("AVI", "avi", "AVI Video", UFT_CAT_VIDEO, SIG_AVI),
    SIG_SIMPLE("MKV", "mkv", "Matroska Video", UFT_CAT_VIDEO, SIG_MKV),
    SIG_SIMPLE("FLV", "flv", "Flash Video", UFT_CAT_VIDEO, SIG_FLV),
    SIG_SIMPLE("MPEG", "mpg", "MPEG Video", UFT_CAT_VIDEO, SIG_MPG),

    /* ===== ARCHIVES ===== */
    SIG_SIMPLE("ZIP", "zip", "ZIP Archive", UFT_CAT_ARCHIVE, SIG_ZIP),
    SIG_SIMPLE("RAR", "rar", "RAR Archive", UFT_CAT_ARCHIVE, SIG_RAR),
    SIG_SIMPLE("RAR5", "rar", "RAR5 Archive", UFT_CAT_ARCHIVE, SIG_RAR5),
    SIG_SIMPLE("7Z", "7z", "7-Zip Archive", UFT_CAT_ARCHIVE, SIG_7Z),
    SIG_SIMPLE("GZIP", "gz", "GZIP Compressed", UFT_CAT_ARCHIVE, SIG_GZIP),
    SIG_SIMPLE("BZIP2", "bz2", "BZIP2 Compressed", UFT_CAT_ARCHIVE, SIG_BZIP2),
    SIG_SIMPLE("XZ", "xz", "XZ Compressed", UFT_CAT_ARCHIVE, SIG_XZ),
    SIG_SIMPLE("ZSTD", "zst", "Zstandard Compressed", UFT_CAT_ARCHIVE, SIG_ZSTD),
    SIG_FLOPPY("CAB", "cab", "Windows Cabinet", UFT_CAT_ARCHIVE, SIG_CAB),
    SIG_FLOPPY("ARJ", "arj", "ARJ Archive", UFT_CAT_ARCHIVE, SIG_ARJ),
    SIG_FLOPPY("LZH", "lzh", "LHA/LZH Archive", UFT_CAT_ARCHIVE, SIG_LZH),
    SIG_FLOPPY("ACE", "ace", "ACE Archive", UFT_CAT_ARCHIVE, SIG_ACE),
    SIG_RETRO("ZOO", "zoo", "ZOO Archive", UFT_CAT_ARCHIVE, SIG_ZOO),
    SIG_RETRO("ARC", "arc", "ARC Archive", UFT_CAT_ARCHIVE, SIG_ARC),
    SIG_RETRO("PAK", "pak", "PAK Archive", UFT_CAT_ARCHIVE, SIG_PAK),

    /* ===== DOCUMENTS ===== */
    SIG("PDF", "pdf", "PDF Document", UFT_CAT_DOCUMENT,
        SIG_PDF, 0, SIG_PDF_FOOTER, UFT_SIG_FLAG_HAS_FOOTER),
    SIG_FLOPPY("OLE2", "doc", "Microsoft OLE2 (DOC/XLS/PPT)", UFT_CAT_DOCUMENT, SIG_OLE2),
    SIG_FLOPPY("RTF", "rtf", "Rich Text Format", UFT_CAT_DOCUMENT, SIG_RTF),
    SIG_SIMPLE("PS", "ps", "PostScript", UFT_CAT_DOCUMENT, SIG_PS),
    SIG_SIMPLE("XML", "xml", "XML Document", UFT_CAT_DOCUMENT, SIG_XML),

    /* ===== RETRO / LEGACY ===== */
    SIG_RETRO("WordStar", "ws", "WordStar Document", UFT_CAT_RETRO, SIG_WORDSTAR),
    SIG_RETRO("WordPerfect", "wpd", "WordPerfect Document", UFT_CAT_RETRO, SIG_WORDPERFECT),
    SIG_RETRO("WP51", "wp", "WordPerfect 5.1", UFT_CAT_RETRO, SIG_WP51),
    SIG_RETRO("MSWrite", "wri", "Microsoft Write", UFT_CAT_RETRO, SIG_MSWRITE),
    SIG_RETRO("Lotus123", "wk1", "Lotus 1-2-3 WK1", UFT_CAT_RETRO, SIG_LOTUS123_WK1),
    SIG_RETRO("Lotus123", "wk3", "Lotus 1-2-3 WK3", UFT_CAT_RETRO, SIG_LOTUS123_WK3),
    SIG_RETRO("dBase3", "dbf", "dBase III Database", UFT_CAT_RETRO, SIG_DBASE3),
    SIG_RETRO("dBase4", "dbf", "dBase IV Database", UFT_CAT_RETRO, SIG_DBASE4),
    SIG_RETRO("FoxPro", "dbf", "FoxPro Database", UFT_CAT_RETRO, SIG_FOXPRO),

    /* ===== DISK CONTAINERS ===== */
    SIG_FLOPPY("D64", "d64", "C64 D64 Disk Image", UFT_CAT_DISK_CONTAINER, SIG_D64),
    SIG_FLOPPY("G64", "g64", "C64 G64 GCR Image", UFT_CAT_DISK_CONTAINER, SIG_G64),
    SIG_FLOPPY("T64", "t64", "C64 T64 Tape Image", UFT_CAT_DISK_CONTAINER, SIG_T64),
    SIG_FLOPPY("CRT", "crt", "C64 Cartridge", UFT_CAT_DISK_CONTAINER, SIG_CRT),
    SIG_FLOPPY("PRG", "prg", "C64 Program", UFT_CAT_DISK_CONTAINER, SIG_PRG),
    SIG_FLOPPY("TAP-C64", "tap", "C64 Tape", UFT_CAT_DISK_CONTAINER, SIG_TAP_C64),
    SIG_FLOPPY("ADF", "adf", "Amiga Disk File", UFT_CAT_DISK_CONTAINER, SIG_ADF),
    SIG_FLOPPY("STX", "stx", "Atari ST Pasti Image", UFT_CAT_DISK_CONTAINER, SIG_STX),
    SIG_FLOPPY("WOZ", "woz", "Apple II WOZ Image", UFT_CAT_DISK_CONTAINER, SIG_WOZ),
    SIG_FLOPPY("WOZ2", "woz", "Apple II WOZ2 Image", UFT_CAT_DISK_CONTAINER, SIG_WOZ2),
    SIG_FLOPPY("2MG", "2mg", "Apple II 2MG Image", UFT_CAT_DISK_CONTAINER, SIG_2MG),
    SIG_FLOPPY("NIB", "nib", "Apple II NIB Image", UFT_CAT_DISK_CONTAINER, SIG_NIB),
    SIG_FLOPPY("TZX", "tzx", "ZX Spectrum TZX Tape", UFT_CAT_DISK_CONTAINER, SIG_TZX),
    SIG_FLOPPY("DSK-CPC", "dsk", "Amstrad CPC DSK", UFT_CAT_DISK_CONTAINER, SIG_DSK_CPC),
    SIG_FLOPPY("DSK-EXT", "dsk", "Extended CPC DSK", UFT_CAT_DISK_CONTAINER, SIG_DSK_EXT),
    SIG_FLOPPY("SCP", "scp", "SuperCard Pro Flux", UFT_CAT_DISK_CONTAINER, SIG_SCP),
    SIG_FLOPPY("HFE", "hfe", "UFT HFE Format", UFT_CAT_DISK_CONTAINER, SIG_HFE),
    SIG_FLOPPY("HFE3", "hfe", "UFT HFE Format v3", UFT_CAT_DISK_CONTAINER, SIG_HFE3),
    SIG_FLOPPY("IPF", "ipf", "SPS/CAPS Image", UFT_CAT_DISK_CONTAINER, SIG_IPF),
    SIG_FLOPPY("IMD", "imd", "ImageDisk", UFT_CAT_DISK_CONTAINER, SIG_IMD),
    SIG_FLOPPY("TD0", "td0", "Teledisk", UFT_CAT_DISK_CONTAINER, SIG_TD0),
    SIG_FLOPPY("FDI", "fdi", "Formatted Disk Image", UFT_CAT_DISK_CONTAINER, SIG_FDI),
    SIG_FLOPPY("ATR", "atr", "Atari 8-bit ATR", UFT_CAT_DISK_CONTAINER, SIG_A8_ATR),
    SIG_FLOPPY("MSX-CAS", "cas", "MSX Cassette", UFT_CAT_DISK_CONTAINER, SIG_MSX_CAS),

    /* ===== ROM / EMULATION ===== */
    SIG_SIMPLE("NES", "nes", "Nintendo NES ROM (iNES)", UFT_CAT_ROM, SIG_NES),
    SIG_SIMPLE("N64", "n64", "Nintendo 64 ROM", UFT_CAT_ROM, SIG_N64),
    SIG_SIMPLE("SMS", "sms", "Sega Master System ROM", UFT_CAT_ROM, SIG_SMS),
    SIG_SIMPLE("GEN", "md", "Sega Genesis/Mega Drive", UFT_CAT_ROM, SIG_GEN),
    SIG_SIMPLE("32X", "32x", "Sega 32X ROM", UFT_CAT_ROM, SIG_32X),
    SIG_SIMPLE("A78", "a78", "Atari 7800 ROM", UFT_CAT_ROM, SIG_A78),
    SIG_SIMPLE("XEX", "xex", "Atari 8-bit Executable", UFT_CAT_ROM, SIG_A8_XEX),
    SIG_SIMPLE("MSX-ROM", "rom", "MSX ROM Cartridge", UFT_CAT_ROM, SIG_MSX_ROM),

    /* ===== CAD / 3D ===== */
    SIG_SIMPLE("DWG-R14", "dwg", "AutoCAD R14 Drawing", UFT_CAT_CAD, SIG_DWG_R14),
    SIG_SIMPLE("DWG-2000", "dwg", "AutoCAD 2000 Drawing", UFT_CAT_CAD, SIG_DWG_2000),
    SIG_SIMPLE("DWG-2004", "dwg", "AutoCAD 2004 Drawing", UFT_CAT_CAD, SIG_DWG_2004),
    SIG_SIMPLE("DWG-2007", "dwg", "AutoCAD 2007 Drawing", UFT_CAT_CAD, SIG_DWG_2007),
    SIG_SIMPLE("STL", "stl", "STL 3D Model (ASCII)", UFT_CAT_CAD, SIG_STL_ASCII),
    SIG_SIMPLE("3DS", "3ds", "3D Studio Mesh", UFT_CAT_CAD, SIG_3DS),
    SIG_SIMPLE("STEP", "step", "STEP CAD Model", UFT_CAT_CAD, SIG_STEP),

    /* ===== EXECUTABLES ===== */
    SIG_FLOPPY("EXE", "exe", "DOS/Windows Executable", UFT_CAT_EXECUTABLE, SIG_EXE_MZ),
    SIG_SIMPLE("ELF", "elf", "Linux ELF Executable", UFT_CAT_EXECUTABLE, SIG_ELF),
    SIG_SIMPLE("MACHO32", "macho", "macOS Mach-O (32-bit)", UFT_CAT_EXECUTABLE, SIG_MACHO32),
    SIG_SIMPLE("MACHO64", "macho", "macOS Mach-O (64-bit)", UFT_CAT_EXECUTABLE, SIG_MACHO64),
    SIG_SIMPLE("CLASS", "class", "Java Class File", UFT_CAT_EXECUTABLE, SIG_JAVA_CLASS),
    SIG_FLOPPY("COM", "com", "DOS COM Executable", UFT_CAT_EXECUTABLE, SIG_COM),

    /* ===== DATABASES ===== */
    SIG_SIMPLE("SQLITE", "db", "SQLite Database", UFT_CAT_DATABASE, SIG_SQLITE),
    SIG_SIMPLE("MDB", "mdb", "Microsoft Access", UFT_CAT_DATABASE, SIG_MDB),

    /* ===== DISK IMAGES ===== */
    SIG_SIMPLE("VHD", "vhd", "Virtual Hard Disk", UFT_CAT_DISK_IMAGE, SIG_VHD),
    SIG_SIMPLE("VMDK", "vmdk", "VMware Disk", UFT_CAT_DISK_IMAGE, SIG_VMDK),
    SIG_SIMPLE("QCOW", "qcow2", "QEMU Copy-On-Write", UFT_CAT_DISK_IMAGE, SIG_QCOW),

    /* ===== FONTS ===== */
    SIG_SIMPLE("TTF", "ttf", "TrueType Font", UFT_CAT_FONT, SIG_TTF),
    SIG_SIMPLE("OTF", "otf", "OpenType Font", UFT_CAT_FONT, SIG_OTF),
    SIG_SIMPLE("WOFF", "woff", "Web Open Font", UFT_CAT_FONT, SIG_WOFF),
    SIG_SIMPLE("WOFF2", "woff2", "Web Open Font 2", UFT_CAT_FONT, SIG_WOFF2),

    /* ===== SYSTEM ===== */
    SIG_SIMPLE("NTFS", "ntfs", "NTFS Boot Sector", UFT_CAT_SYSTEM, SIG_NTFS),
    SIG_SIMPLE("GPT", "gpt", "GUID Partition Table", UFT_CAT_SYSTEM, SIG_GPT),

    /* ===== SCIENTIFIC ===== */
    SIG_SIMPLE("FITS", "fits", "FITS Astronomy Data", UFT_CAT_SCIENTIFIC, SIG_FITS),
    SIG_SIMPLE("HDF5", "h5", "HDF5 Scientific Data", UFT_CAT_SCIENTIFIC, SIG_HDF5),
    SIG_SIMPLE("NetCDF", "nc", "NetCDF Data", UFT_CAT_SCIENTIFIC, SIG_NETCDF),
    SIG_SIMPLE("MATLAB", "mat", "MATLAB Data", UFT_CAT_SCIENTIFIC, SIG_MATLAB),

    /* ===== EMAIL / PIM ===== */
    SIG_SIMPLE("PST", "pst", "Outlook PST", UFT_CAT_EMAIL, SIG_PST),
    SIG_SIMPLE("MBOX", "mbox", "MBOX Mail Archive", UFT_CAT_EMAIL, SIG_MBOX),
    SIG_SIMPLE("vCard", "vcf", "vCard Contact", UFT_CAT_EMAIL, SIG_VCARD),

    /* ===== CRYPTO / SECURITY ===== */
    SIG_SIMPLE("PGP-PUB", "pgp", "PGP Public Key", UFT_CAT_CRYPTO, SIG_PGP_PUB),
    SIG_SIMPLE("PGP-SEC", "pgp", "PGP Secret Key", UFT_CAT_CRYPTO, SIG_PGP_SEC),
    SIG_SIMPLE("SSH-PUB", "pub", "SSH Public Key", UFT_CAT_CRYPTO, SIG_SSH_PUB),
    SIG_SIMPLE("PEM", "pem", "PEM Certificate", UFT_CAT_CRYPTO, SIG_PEM),
    SIG_SIMPLE("DER", "der", "DER Certificate", UFT_CAT_CRYPTO, SIG_DER),
    SIG_SIMPLE("KeePass", "kdbx", "KeePass Database", UFT_CAT_CRYPTO, SIG_KDBX),

    /* ===== OTHER ===== */
    SIG_SIMPLE("BLEND", "blend", "Blender 3D", UFT_CAT_OTHER, SIG_BLEND),
    SIG_SIMPLE("PCAP", "pcap", "Packet Capture", UFT_CAT_OTHER, SIG_PCAP),

    /* ----- End marker ----- */
    { NULL, NULL, NULL, UFT_CAT_UNKNOWN, NULL, 0, 0, NULL, 0, 0 }
};

#define SIGNATURE_COUNT (sizeof(g_signatures) / sizeof(g_signatures[0]) - 1)

/* ============================================================================
 * Category Names
 * ============================================================================ */

static const char *g_category_names[] = {
    "Unknown",
    "Image",
    "Audio",
    "Video",
    "Archive",
    "Document",
    "Executable",
    "Database",
    "Disk Image",
    "Font",
    "System",
    "Other",
    "Retro/Legacy",
    "Disk Container",
    "ROM/Emulation",
    "CAD/3D",
    "Scientific",
    "Email/PIM",
    "Crypto/Security"
};

/* ============================================================================
 * API Implementation
 * ============================================================================ */

const uft_file_signature_t *uft_sig_get_database(size_t *count)
{
    if (count) {
        *count = SIGNATURE_COUNT;
    }
    return g_signatures;
}

const char *uft_sig_category_name(uft_file_category_t category)
{
    if (category >= 0 && category <= UFT_CAT_CRYPTO) {
        return g_category_names[category];
    }
    return "Unknown";
}

bool uft_sig_matches(const uint8_t *data, size_t size,
                     const uft_file_signature_t *sig)
{
    if (!data || !sig || !sig->header || sig->header_size == 0) {
        return false;
    }
    
    if (sig->header_offset + sig->header_size > size) {
        return false;
    }
    
    return memcmp(data + sig->header_offset, sig->header, sig->header_size) == 0;
}

bool uft_sig_detect(const uint8_t *data, size_t size,
                    uft_sig_detect_result_t *result)
{
    if (!data || size == 0 || !result) {
        return false;
    }
    
    memset(result, 0, sizeof(*result));
    
    for (size_t i = 0; i < SIGNATURE_COUNT; i++) {
        const uft_file_signature_t *sig = &g_signatures[i];
        
        if (uft_sig_matches(data, size, sig)) {
            result->signature = sig;
            result->match_offset = sig->header_offset;
            result->header_match = true;
            result->confidence = 80;
            
            /* Boost confidence for longer signatures */
            if (sig->header_size >= 8) {
                result->confidence = 95;
            } else if (sig->header_size >= 4) {
                result->confidence = 90;
            }
            
            return true;
        }
    }
    
    return false;
}

bool uft_sig_detect_with_footer(const uint8_t *header, size_t header_size,
                                const uint8_t *footer, size_t footer_size,
                                uft_sig_detect_result_t *result)
{
    if (!uft_sig_detect(header, header_size, result)) {
        return false;
    }
    
    const uft_file_signature_t *sig = result->signature;
    
    if (sig->footer && sig->footer_size > 0 && footer && footer_size >= sig->footer_size) {
        for (size_t off = 0; off <= footer_size - sig->footer_size; off++) {
            if (memcmp(footer + off, sig->footer, sig->footer_size) == 0) {
                result->footer_match = true;
                result->confidence = 98;
                break;
            }
        }
    }
    
    return true;
}

size_t uft_sig_detect_all(const uint8_t *data, size_t size,
                          uft_sig_detect_result_t *results, size_t max_results)
{
    if (!data || size == 0 || !results || max_results == 0) {
        return 0;
    }
    
    size_t count = 0;
    
    for (size_t i = 0; i < SIGNATURE_COUNT && count < max_results; i++) {
        const uft_file_signature_t *sig = &g_signatures[i];
        
        if (uft_sig_matches(data, size, sig)) {
            results[count].signature = sig;
            results[count].match_offset = sig->header_offset;
            results[count].header_match = true;
            results[count].footer_match = false;
            results[count].confidence = (sig->header_size >= 8) ? 95 : 
                                        (sig->header_size >= 4) ? 90 : 80;
            count++;
        }
    }
    
    return count;
}

const uft_file_signature_t *uft_sig_by_extension(const char *extension)
{
    if (!extension) return NULL;
    if (*extension == '.') extension++;
    
    for (size_t i = 0; i < SIGNATURE_COUNT; i++) {
        if (g_signatures[i].extension && 
            strcasecmp(g_signatures[i].extension, extension) == 0) {
            return &g_signatures[i];
        }
    }
    return NULL;
}

const uft_file_signature_t *uft_sig_by_name(const char *name)
{
    if (!name) return NULL;
    
    for (size_t i = 0; i < SIGNATURE_COUNT; i++) {
        if (g_signatures[i].name && 
            strcasecmp(g_signatures[i].name, name) == 0) {
            return &g_signatures[i];
        }
    }
    return NULL;
}

size_t uft_sig_by_category(uft_file_category_t category,
                           const uft_file_signature_t **signatures,
                           size_t max_count)
{
    if (!signatures || max_count == 0) return 0;
    
    size_t count = 0;
    for (size_t i = 0; i < SIGNATURE_COUNT && count < max_count; i++) {
        if (g_signatures[i].category == category) {
            signatures[count++] = &g_signatures[i];
        }
    }
    return count;
}

size_t uft_sig_get_floppy_signatures(const uft_file_signature_t **signatures,
                                     size_t max_count)
{
    if (!signatures || max_count == 0) return 0;
    
    size_t count = 0;
    for (size_t i = 0; i < SIGNATURE_COUNT && count < max_count; i++) {
        if (g_signatures[i].flags & UFT_SIG_FLAG_FLOPPY) {
            signatures[count++] = &g_signatures[i];
        }
    }
    return count;
}

size_t uft_sig_get_retro_signatures(const uft_file_signature_t **signatures,
                                    size_t max_count)
{
    if (!signatures || max_count == 0) return 0;
    
    size_t count = 0;
    for (size_t i = 0; i < SIGNATURE_COUNT && count < max_count; i++) {
        if ((g_signatures[i].flags & UFT_SIG_FLAG_RETRO) ||
            g_signatures[i].category == UFT_CAT_RETRO ||
            g_signatures[i].category == UFT_CAT_DISK_CONTAINER ||
            g_signatures[i].category == UFT_CAT_ROM) {
            signatures[count++] = &g_signatures[i];
        }
    }
    return count;
}

size_t uft_sig_scan_buffer(const uint8_t *data, size_t size,
                           uft_sig_scan_callback_t callback,
                           void *user_data)
{
    if (!data || size == 0 || !callback) {
        return 0;
    }
    
    size_t found = 0;
    
    for (size_t offset = 0; offset < size; offset++) {
        for (size_t i = 0; i < SIGNATURE_COUNT; i++) {
            const uft_file_signature_t *sig = &g_signatures[i];
            
            if (sig->header_offset != 0) {
                continue;
            }
            
            if (offset + sig->header_size <= size &&
                memcmp(data + offset, sig->header, sig->header_size) == 0) {
                callback(offset, sig, user_data);
                found++;
            }
        }
    }
    
    return found;
}
