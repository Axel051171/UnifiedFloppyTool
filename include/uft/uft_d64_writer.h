/**
 * @file uft_d64_writer.h
 * @brief D64 Writer with Accurate Gap Timing
 * 
 * P2-006: Writer Gap-Timing D64
 * 
 * Creates authentic 1541 disk images with:
 * - Accurate GCR encoding
 * - Proper inter-sector gaps
 * - Correct sync patterns
 * - Zone-based timing (speed zones 0-3)
 * - Header/data block checksums
 * 
 * 1541 Track Layout:
 * Zone 0 (Tracks 1-17):  21 sectors, 3.25 ms/revolution
 * Zone 1 (Tracks 18-24): 19 sectors, 3.50 ms/revolution
 * Zone 2 (Tracks 25-30): 18 sectors, 3.75 ms/revolution
 * Zone 3 (Tracks 31-35): 17 sectors, 4.00 ms/revolution
 */

#ifndef UFT_D64_WRITER_H
#define UFT_D64_WRITER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ═══════════════════════════════════════════════════════════════════════════════
 * Constants
 * ═══════════════════════════════════════════════════════════════════════════════ */

#define D64_TRACK_COUNT         35
#define D64_TRACK_COUNT_EXT     40    /* Extended D64 */
#define D64_SECTOR_SIZE         256
#define D64_GCR_SECTOR_SIZE     325   /* After GCR encoding */
#define D64_HEADER_SIZE         10    /* 8 bytes → 10 GCR */
#define D64_DATA_SIZE           325   /* 260 bytes → 325 GCR */

/* Sync marks */
#define D64_SYNC_BYTE           0xFF
#define D64_SYNC_COUNT          5     /* Standard sync length */
#define D64_HEADER_MARK         0x08  /* Header block ID */
#define D64_DATA_MARK           0x07  /* Data block ID */

/* ── Zonen-Zellzeiten bei 300 U/min (MF-565) ─────────────────────────────
 *
 * **Diese vier Werte standen gegenlaeufig zu ihren eigenen Kommentaren.**
 * `D64_ZONE_0` heisst laut Enum „21 sectors, fastest" und laut Kommentar
 * „Tracks 1-17" — trug aber 4,0 us, die Zellzeit der LANGSAMSTEN Zone.
 * Alle vier Zeilen waren vertauscht.
 *
 * ── Referenz ────────────────────────────────────────────────────────────
 *
 * Die 1541 leitet ihre vier Datenraten aus 16 MHz ab (Immers/Neufeld,
 * „Inside Commodore DOS", Zonentabelle; dieselben Raten stehen im Baum in
 * `include/uft/uft_c64_gcr.h:137`):
 *
 *     16 MHz / 52 = 307 692 bps  ->  3,25 us   Spuren  1-17  (21 Sektoren)
 *     16 MHz / 56 = 285 714 bps  ->  3,50 us   Spuren 18-24  (19 Sektoren)
 *     16 MHz / 60 = 266 667 bps  ->  3,75 us   Spuren 25-30  (18 Sektoren)
 *     16 MHz / 64 = 250 000 bps  ->  4,00 us   Spuren 31-35  (17 Sektoren)
 *
 * ── Die Gegenprobe, die es entschieden hat ──────────────────────────────
 *
 * Sie braucht keine Quelle, nur eine Uhr: eine Umdrehung bei 300 U/min
 * dauert 200,0 ms. `uft_c64_bytes_per_track(1)` meldet 7692 Byte fuer
 * Spur 1, und
 *
 *     7692 Byte * 8 Bit * 3,25 us = 200,0 ms      <- passt
 *     7692 Byte * 8 Bit * 4,00 us = 246,1 ms      <- so war es
 *
 * Der alte Wert beschrieb eine Diskette, die sich 23 % zu langsam dreht.
 *
 * ── Was daran hing ──────────────────────────────────────────────────────
 *
 * `d64_gcr_to_flux()` schrieb den Fluss mit dieser Zellzeit, der Leser
 * stellte seinen PLL nach `uft_c64_track_bitrate()` ein — und das ist die
 * RICHTIGE Tabelle. Schreiber und Leser lagen um 23 % auseinander.
 * Gemessen im Rundlauf D64 -> SCP -> D64: **0 von 683 Sektoren**
 * wiedergefunden, auf einer fehlerfreien synthetischen Aufnahme
 * (`tests/test_convert_scp_d64_multirev.c`).
 *
 * Die beiden Zahlen standen seit MF-537 nebeneinander im Kommentar von
 * `uft_format_convert_bitstream.c` — „4000 ns Zellzeit" und „der PLL …
 * mit einer Zellzeit von 3250 ns". Verglichen hat sie niemand.
 */
#define D64_ZONE0_BIT_TIME_US   3.25  /* Tracks 1-17,   307 692 bps */
#define D64_ZONE1_BIT_TIME_US   3.5   /* Tracks 18-24,  285 714 bps */
#define D64_ZONE2_BIT_TIME_US   3.75  /* Tracks 25-30,  266 667 bps */
#define D64_ZONE3_BIT_TIME_US   4.0   /* Tracks 31-35+, 250 000 bps */

/* Gap lengths (in GCR bytes) */
#define D64_GAP1_LENGTH         9     /* After header, before data */
#define D64_GAP2_LENGTH         9     /* After data, before next header */
#define D64_HEADER_GAP          5     /* Minimum gap before header */

/* ═══════════════════════════════════════════════════════════════════════════════
 * Types
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Speed zone for track
 */
typedef enum {
    D64_ZONE_0 = 0,  /* 21 sectors, fastest */
    D64_ZONE_1 = 1,  /* 19 sectors */
    D64_ZONE_2 = 2,  /* 18 sectors */
    D64_ZONE_3 = 3   /* 17 sectors, slowest */
} d64_speed_zone_t;

/**
 * @brief Sector interleave patterns
 */
typedef enum {
    D64_INTERLEAVE_STANDARD = 10,   /* Standard 1541 interleave */
    D64_INTERLEAVE_FAST     = 6,    /* Fast loader interleave */
    D64_INTERLEAVE_CUSTOM   = 0     /* Use custom table */
} d64_interleave_t;

/**
 * @brief D64 sector header (8 bytes raw → 10 GCR)
 */
typedef struct {
    uint8_t block_id;      /* 0x08 for header */
    uint8_t checksum;      /* XOR of track, sector, id1, id2 */
    uint8_t sector;        /* Sector number (0-20) */
    uint8_t track;         /* Track number (1-35) */
    uint8_t id2;           /* Disk ID byte 2 */
    uint8_t id1;           /* Disk ID byte 1 */
    uint8_t padding[2];    /* 0x0F padding */
} d64_header_t;

/**
 * @brief D64 data block (260 bytes raw → 325 GCR)
 */
typedef struct {
    uint8_t block_id;      /* 0x07 for data */
    uint8_t data[256];     /* Sector data */
    uint8_t checksum;      /* XOR of all data bytes */
    uint8_t padding[2];    /* 0x00 padding */
} d64_data_block_t;

/**
 * @brief Writer configuration
 */
typedef struct {
    /* Timing options */
    bool accurate_timing;      /* Use real 1541 timing */
    bool variable_gaps;        /* Vary gap lengths slightly */
    int gap1_length;           /* Override Gap1 length (-1 = default) */
    int gap2_length;           /* Override Gap2 length (-1 = default) */
    int sync_length;           /* Sync byte count (default 5) */
    
    /* Format options */
    uint8_t disk_id[2];        /* Disk ID bytes */
    d64_interleave_t interleave;
    uint8_t *custom_interleave;/* Custom interleave table (if CUSTOM) */
    int custom_interleave_len;
    
    /* Extended format */
    bool extended_tracks;      /* Write tracks 36-40 */
    int track_count;           /* Total tracks (35 or 40) */
    
    /* Output options */
    bool include_error_info;   /* Include error byte per sector */
    bool generate_g64;         /* Output G64 instead of D64 */
    bool flux_output;          /* Generate flux timing data */
} d64_writer_config_t;

/**
 * @brief Default configuration
 */
#define D64_WRITER_CONFIG_DEFAULT { \
    .accurate_timing = true, \
    .variable_gaps = false, \
    .gap1_length = -1, \
    .gap2_length = -1, \
    .sync_length = 5, \
    .disk_id = {0x30, 0x30}, \
    .interleave = D64_INTERLEAVE_STANDARD, \
    .custom_interleave = NULL, \
    .custom_interleave_len = 0, \
    .extended_tracks = false, \
    .track_count = 35, \
    .include_error_info = false, \
    .generate_g64 = false, \
    .flux_output = false \
}

/**
 * @brief Track write result
 */
typedef struct {
    int track;
    int sectors_written;
    int gcr_bytes;
    double track_time_ms;
    int errors;
    char error_msg[64];
} d64_track_result_t;

/**
 * @brief Writer context
 */
typedef struct d64_writer d64_writer_t;

/* ═══════════════════════════════════════════════════════════════════════════════
 * API
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Create D64 writer
 */
d64_writer_t* d64_writer_create(const d64_writer_config_t *config);

/**
 * @brief Destroy writer
 */
void d64_writer_destroy(d64_writer_t *writer);

/**
 * @brief Write D64 from sector data
 * @param writer Writer context
 * @param sectors Sector data (174848 bytes for 35 tracks)
 * @param sector_count Number of sectors
 * @param output Output buffer (must be large enough)
 * @param output_size Size written to output
 * @return 0 on success
 */
int d64_writer_write(
    d64_writer_t *writer,
    const uint8_t *sectors,
    int sector_count,
    uint8_t *output,
    size_t *output_size);

/**
 * @brief Write single track to GCR
 */
int d64_write_track_gcr(
    d64_writer_t *writer,
    int track,
    const uint8_t *sector_data,
    int sector_count,
    uint8_t *gcr_output,
    size_t *gcr_size,
    d64_track_result_t *result);

/**
 * @brief Get sectors per track for given track number
 */
int d64_sectors_per_track(int track);

/**
 * @brief Welcher Sektor liegt PHYSISCH an Position @p position?
 *
 * Aufsteigend: Position i traegt Sektor i. Ein 1541 formatiert seine
 * Spuren sequenziell; der Interleave ist Sache der Vergabe
 * (@ref uft_d64_vergabe_an_position), nicht des Spurbildes (MF-861).
 *
 * Die Funktion ist bewusst trivial und existiert trotzdem: sie ist die
 * Stelle, an der die Aussage steht, und sie hat einen Rotbeweis.
 *
 * @return Sektornummer, oder -1 bei ungueltiger Spur/Position.
 */
int uft_d64_sektor_an_position(int track, int position);

/** Versatz fuer Datenspuren (1541-DOS: `secinc` = 10, ROM $EBCD). */
#define UFT_D64_INTERLEAVE_DATEN      10

/** Versatz fuer die Directory-Spur (1541-DOS: NXDRBK $D497 setzt 3). */
#define UFT_D64_INTERLEAVE_DIRECTORY   3

/** Die Directory-Spur einer 1541-Diskette. */
#define UFT_D64_DIRECTORY_TRACK       18

/**
 * @brief Welchen Sektor VERGIBT das CBM-DOS als naechsten?
 *
 * ── BERICHTIGT MF-861: das ist Vergabe, nicht Spurbelegung ───────────
 *
 * MF-859 hat diese Funktion gebaut und sie in `d64_write_track_gcr()`
 * benutzt, um die PHYSISCHE Reihenfolge der Sektoren auf der Spur zu
 * bestimmen. Das war die falsche Ebene, und MF-859 hat die Frage selbst
 * als P3-111 offen gelassen, statt sie zu klaeren.
 *
 * Geklaert ist sie jetzt: **ein 1541 legt die Sektoren aufsteigend auf
 * die Spur** — 0, 1, 2, …, n-1. Fuenf unabhaengige Umsetzungen und eine
 * Messung an einer realen G64 sagen dasselbe (ROM-Formatierroutine
 * $FC36-$FD1C, OpenCBM `cbmformat.a65`, VICE `fsimage-dxx.c:262`,
 * nibtools `fileio.c:760`, 1541ultimate `disk_image.cc:251`). Der
 * Versatz 10 lebt ausschliesslich in der BLOCKVERGABE.
 *
 * Die Funktion bleibt — die Vergabereihenfolge ist eine echte Groesse,
 * nur nicht die, fuer die MF-859 sie eingesetzt hat. Sie heisst jetzt
 * so, wie sie ist.
 *
 * ── Die Regel, und warum sie nicht modular ist ───────────────────────
 *
 * Beim Ueberlauf zieht das DOS die Sektorzahl ab und **danach noch
 * eins**, sofern das Ergebnis nicht 0 ist (ROM FNDNXT $F189-$F193;
 * byte-fuer-byte dieselbe Regel wie `lib1541img`
 * `cbmdosfs.c:126-134`). Fuer 21 Sektoren ergibt das
 * `0, 10, 20, 8, 18, 6, …` — NICHT `0, 10, 20, 9, 19, 8, …`.
 *
 * Die modulare Fassung, die MF-859 gebaut hat, entspricht
 * `lib1541img`s Schalter `CFF_SIMPLEINTERLEAVE` und nicht dem DOS.
 *
 * Ist die Stelle schon vergeben, rueckt das DOS um eins weiter.
 *
 * @return Sektornummer, oder -1 bei ungueltiger Spur/Position.
 */
int uft_d64_vergabe_an_position(int track, int position);

/**
 * @brief Get speed zone for track
 */
d64_speed_zone_t d64_track_zone(int track);

/**
 * @brief Get bit time in microseconds for zone
 */
double d64_zone_bit_time(d64_speed_zone_t zone);

/**
 * @brief Calculate track length in bits
 */
int d64_track_length_bits(int track);

/**
 * @brief Get track length in GCR bytes
 */
int d64_track_length_gcr(int track);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Low-Level GCR Functions
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Encode 4 bytes to 5 GCR bytes
 */
void d64_gcr_encode_4to5(const uint8_t *data, uint8_t *gcr);

/**
 * @brief Decode 5 GCR bytes to 4 bytes
 */
int d64_gcr_decode_5to4(const uint8_t *gcr, uint8_t *data);

/**
 * @brief Encode sector header to GCR
 */
void d64_encode_header(const d64_header_t *header, uint8_t *gcr);

/**
 * @brief Encode data block to GCR
 */
void d64_encode_data_block(const d64_data_block_t *block, uint8_t *gcr);

/**
 * @brief Calculate header checksum
 */
uint8_t d64_header_checksum(int track, int sector, uint8_t id1, uint8_t id2);

/**
 * @brief Calculate data checksum
 */
uint8_t d64_data_checksum(const uint8_t *data, int size);

/**
 * @brief Write sync bytes
 */
void d64_write_sync(uint8_t *output, int count);

/**
 * @brief Write gap bytes
 */
void d64_write_gap(uint8_t *output, int count);

/* ═══════════════════════════════════════════════════════════════════════════════
 * Flux Output (for SCP/G64)
 * ═══════════════════════════════════════════════════════════════════════════════ */

/**
 * @brief Convert GCR track to flux timing
 */
int d64_gcr_to_flux(
    const uint8_t *gcr_data,
    size_t gcr_size,
    d64_speed_zone_t zone,
    uint32_t *flux_output,
    size_t *flux_count);

#ifdef __cplusplus
}
#endif

#endif /* UFT_D64_WRITER_H */
