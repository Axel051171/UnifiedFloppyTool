/**
 * @file uft_mgt.h
 * @brief MGT disk format support (+D and DISCiPLE)
 * @version 3.9.0
 * 
 * MGT format for Miles Gordon Technology +D and DISCiPLE disk interfaces
 * for the ZX Spectrum. 
 * 
 * Format: 80 tracks, double-sided, 10 sectors of 512 bytes.
 * Total capacity: 800KB formatted.
 * 
 * Reference: libdsk drvmgt.c, World of Spectrum (libdsk ist LGPL-2.0-or-later; der genannte Treiber liegt NICHT in der geprueften Fassung 1.5.12 -- Datei unverifiziert, MF-651)
 */

#ifndef UFT_MGT_H
#define UFT_MGT_H

#include "uft/uft_format_common.h"
#include "uft/core/uft_disk_image_compat.h"
#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/* MGT disk parameters */
#define MGT_CYLINDERS           80
#define MGT_HEADS               2
#define MGT_SECTORS             10
#define MGT_SECTOR_SIZE         512
#define MGT_FIRST_SECTOR        1
#define MGT_TRACK_SIZE          (MGT_SECTORS * MGT_SECTOR_SIZE)  /* 5120 bytes */
#define MGT_DISK_SIZE           (MGT_CYLINDERS * MGT_HEADS * MGT_TRACK_SIZE) /* 819200 bytes */

/* Alternative 40-track format */
#define MGT_40_CYLINDERS        40
#define MGT_40_DISK_SIZE        (MGT_40_CYLINDERS * MGT_HEADS * MGT_TRACK_SIZE)

/* Directory structure
 *
 * MF-933: hier standen zwei Zahlen, die einander widersprachen —
 * `MGT_DIR_ENTRIES 80` und `MGT_SECTORS_PER_DIR 4`. Vier Sektoren zu
 * 512 Byte fassen bei 256 Byte je Eintrag genau ACHT Eintraege, nicht
 * 80, und `uft_mgt_read_directory()` folgte der kleineren Zahl: eine
 * Diskette mit mehr als acht Dateien lieferte acht, mit `UFT_OK`.
 *
 * Die Anordnung stammt aus zwei Quellen im Baum, die unabhaengig
 * dasselbe sagen:
 *
 *   1. `src/samdisk/Util.cpp` (vendort, MIT) laeuft das Verzeichnis als
 *      `dir_tracks` Zylinder x `MGT_SECTORS` Sektoren x 2 Eintraege.
 *   2. `MGT_DIR_ENTRIES 80` hier — beide treffen sich bei
 *      `dir_tracks = 4`: 4 * 10 * 2 = 80.
 *
 * `MGT_DIR_TRACKS` wird deshalb ABGELEITET statt getippt: wer eine der
 * beiden Zahlen aendert, bekommt die andere mitgezogen.
 *
 * NICHT belegt: bei SAMdisk ist `dir_tracks` eine VARIABLE, die
 * `GetDiskInfo()` aus Sektor 0 liest. Deren Definition steht in
 * `SAMCoupe.h`, und diese Datei fehlt im vendorten Bestand (gemessen
 * MF-933). Eine Diskette mit abweichendem `dir_tracks` ist hier nicht
 * abgedeckt; siehe P3-207.
 */
#define MGT_DIR_TRACK           0       /* Directory starts on track 0 */
#define MGT_DIR_ENTRIES         80      /* Maximum directory entries */
#define MGT_DIR_ENTRY_SIZE      256     /* Bytes per directory entry */
#define MGT_DIR_ENTRIES_PER_SECTOR  (MGT_SECTOR_SIZE / MGT_DIR_ENTRY_SIZE)  /* 2 */
#define MGT_DIR_TRACKS \
    (MGT_DIR_ENTRIES / (MGT_SECTORS * MGT_DIR_ENTRIES_PER_SECTOR))  /* 4 */

/* Wie viele Sektoren die SONDE anschaut. Bewusst klein und bewusst
 * getrennt von der Verzeichnisausdehnung: die Sonde braucht nur einen
 * plausiblen Anfang, nicht das ganze Verzeichnis. Bis MF-933 wurde
 * diese Zahl faelschlich AUCH als Ausdehnung benutzt. */
#define MGT_SECTORS_PER_DIR     4       /* nur fuer uft_mgt_probe() */

/* File types */
#define MGT_TYPE_FREE           0       /* Free slot */
#define MGT_TYPE_BASIC          1       /* BASIC program */
#define MGT_TYPE_NUM_ARRAY      2       /* Numeric array */
#define MGT_TYPE_STR_ARRAY      3       /* String array */
#define MGT_TYPE_CODE           4       /* Code file */
#define MGT_TYPE_48K_SNAP       5       /* 48K snapshot */
#define MGT_TYPE_MICRODRIVE     6       /* Microdrive file */
#define MGT_TYPE_SCREEN         7       /* SCREEN$ */
#define MGT_TYPE_SPECIAL        8       /* Special */
#define MGT_TYPE_128K_SNAP      9       /* 128K snapshot */
#define MGT_TYPE_OPENTYPE       10      /* Opentype */
#define MGT_TYPE_EXECUTE        11      /* Execute */

/**
 * @brief MGT directory entry (256 bytes)
 */
#pragma pack(push, 1)
typedef struct {
    uint8_t  type;              /* File type (0=free) */
    char     filename[10];      /* Filename (space-padded) */
    uint8_t  sectors_used;      /* Number of sectors */
    uint8_t  track;             /* First track */
    uint8_t  sector;            /* First sector */
    uint8_t  sector_map[195];   /* Sector allocation map */
    uint8_t  reserved[46];      /* Type-specific data */
} mgt_dir_entry_t;
#pragma pack(pop)

/**
 * @brief MGT read result
 */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;
    
    uint8_t  cylinders;
    uint8_t  heads;
    uint8_t  sectors;
    uint16_t sector_size;
    size_t   image_size;
    
    /* Directory info */
    uint32_t dir_entries;
    uint32_t used_entries;
    uint32_t free_sectors;
    
} mgt_read_result_t;

/* ============================================================================
 * MGT Disk Functions
 * ============================================================================ */

/**
 * @brief Read MGT disk image
 */
uft_error_t uft_mgt_read(const char *path,
                         uft_disk_image_t **out_disk,
                         mgt_read_result_t *result);

/**
 * @brief Read MGT disk from memory
 */
uft_error_t uft_mgt_read_mem(const uint8_t *data, size_t size,
                             uft_disk_image_t **out_disk,
                             mgt_read_result_t *result);

/**
 * @brief Write MGT disk image
 */
uft_error_t uft_mgt_write(const uft_disk_image_t *disk,
                          const char *path);

/**
 * @brief Probe if data is MGT format
 */
bool uft_mgt_probe(const uint8_t *data, size_t size, int *confidence);

/**
 * @brief Read MGT directory
 */
uft_error_t uft_mgt_read_directory(const uft_disk_image_t *disk,
                                   mgt_dir_entry_t *entries,
                                   size_t max_entries,
                                   size_t *entry_count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_MGT_H */
