/**
 * @file uft_a2r_parser.c
 * @brief A2R Apple II Flux Format Parser Implementation
 * 
 * Implements parsing of A2R v2 and v3 flux format files.
 * 
 * A2R Format Structure:
 * - Header: "A2R2" or "A2R3" + 0xFF 0x0A 0x0D 0x0A (8 bytes)
 * - Chunks: 4-byte ID + 4-byte size + data
 * 
 * Chunk Types:
 * - INFO: Disk information
 * - STRM: Flux stream data (v2)
 * - RWCP: Raw capture data (v3)
 * - SLVD: Solved/decoded data (v3)
 * - META: Optional metadata
 * 
 * @author UFT Team
 * @version 3.4.0
 */

#include "uft/parsers/uft_a2r_parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#ifndef SIZE_MAX
#define SIZE_MAX ((size_t)-1)
#endif

/* Sanity caps for A2R allocations */
#define A2R_MAX_FLUX_BYTES_PER_CAPTURE (16 * 1024 * 1024)  /* 16MB per capture */
#define A2R_MAX_NIBBLE_BYTES           (256 * 1024)          /* 256KB nibbles */

/*============================================================================
 * Internal Structures
 *============================================================================*/

/** Chunk header */
typedef struct {
    char        id[4];
    uint32_t    size;
} chunk_header_t;

/** STRM location entry (v2) */
typedef struct {
    uint8_t     location;       /**< Quarter track */
    uint8_t     capture_type;   /**< 1=timing, 2=bits, 3=xtiming */
    uint32_t    data_length;    /**< Flux data length */
    uint32_t    tick_count;     /**< Loop point tick */
} strm_entry_t;

/** RWCP location entry (v3) */
typedef struct {
    uint8_t     track;          /**< Track * 4 + quarter */
    uint8_t     side;           /**< Side (0 or 1) */
    uint32_t    index_count;    /**< Number of index marks */
    uint32_t    *indices;       /**< Index positions */
    uint32_t    data_length;    /**< Capture data length */
    uint8_t     *data;          /**< Capture data */
} rwcp_entry_t;

/*============================================================================
 * Error Strings
 *============================================================================*/

static const char *error_strings[A2R_ERR_COUNT] = {
    "OK",
    "Null parameter",
    "Cannot open file",
    "File read error",
    "Invalid A2R signature",
    "Unsupported A2R version",
    "Invalid chunk",
    "Missing INFO chunk",
    "No flux data",
    "Track out of range",
    "Capture out of range",
    "Memory allocation failed",
    "Corrupt data"
};

/* MF-868: die Aufzaehlung stammt jetzt aus der veroeffentlichten
 * Referenz („A2R 3.x Disk Image Reference", applesaucefdc.com/a2r/,
 * Feld „Drive Type" im INFO-Chunk).
 *
 * Vorher standen hier fuenf Eintraege, die ab Index 2 durchgehend etwas
 * anderes benannten als die Referenz — 2 hiess „5.25 Double-Sided"
 * (richtig: 3.5 DS 80trk Apple CLV), 3 hiess „3.5 Single-Sided"
 * (richtig: 5.25 DS 80trk), 4 hiess „3.5 Double-Sided" (richtig:
 * 5.25 DS 40trk). Die Werte 6..8 fehlten ganz — und genau 6 traegt die
 * Aufnahme im Korpus (`tests/corpus/kor_a`), die daraufhin als
 * „Unknown" angezeigt wurde, obwohl die Referenz sie kennt.
 *
 * Ein falsches Etikett ist schlimmer als gar keines: es sieht aus wie
 * eine Auskunft. */
static const char *disk_type_strings[] = {
    "Unknown",
    "5.25\" SS 40trk 0.25 step",
    "3.5\" DS 80trk Apple CLV",
    "5.25\" DS 80trk",
    "5.25\" DS 40trk",
    "3.5\" DS 80trk",
    "8\" DS",
    "3\" DS 80trk",
    "3\" DS 40trk"
};

/*============================================================================
 * Utility Functions
 *============================================================================*/

/** Read little-endian uint16_t */
static inline uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)p[0] | ((uint16_t)p[1] << 8);
}

/** Read little-endian uint32_t */
static inline uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | 
           ((uint32_t)p[1] << 8) | 
           ((uint32_t)p[2] << 16) | 
           ((uint32_t)p[3] << 24);
}

/** Safe string copy with null termination */
static void safe_strcpy(char *dst, const char *src, size_t dst_size) {
    if (!dst || !src || dst_size == 0) return;
    size_t len = strlen(src);
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
}

/** Copy fixed-length string with null termination */
static void copy_fixed_string(char *dst, const uint8_t *src, 
                              size_t src_len, size_t dst_size) {
    if (!dst || !src || dst_size == 0) return;
    size_t len = src_len;
    if (len >= dst_size) len = dst_size - 1;
    memcpy(dst, src, len);
    dst[len] = '\0';
    
    /* Trim trailing spaces */
    while (len > 0 && dst[len - 1] == ' ') {
        dst[--len] = '\0';
    }
}

/*============================================================================
 * Chunk Parsing
 *============================================================================*/

/** Parse INFO chunk.
 *
 * MF-868: hier stand `if (size < 60) return A2R_ERR_BAD_CHUNK;` und ein
 * Feldlayout, das den Erzeuger ab Byte 0 las.
 *
 * Beides ist falsch, und zwar gemessen an ZWEI unabhaengigen Haenden:
 * der veroeffentlichten „A2R 3.x Disk Image Reference"
 * (applesaucefdc.com/a2r/) und einer echten Aufnahme (Applesauce
 * v1.88.4, `tests/corpus/kor_a/…a2r`). Beide sagen dasselbe:
 *
 *     +0   1   INFO Version (aktuell 1)
 *     +1  32   Creator, mit 0x20 gefuellt
 *     +33  1   Drive Type
 *     +34  1   Write Protected
 *     +35  1   Synchronized
 *     +36  1   Hard Sector Count       -> 37 Byte gesamt
 *
 * Die echte Datei hat einen 37-Byte-INFO-Chunk. Der Leser wies ihn also
 * ab — und gab trotzdem `A2R_OK` mit einem genullten Datensatz zurueck,
 * weil der Aufrufer den Rueckgabewert nicht auswertete. Das ist der
 * eigentliche Schaden: nicht dass etwas nicht ging, sondern dass „OK"
 * daran stand.
 *
 * Fuer A2R 2.x fuehrt die Referenz denselben Aufbau ohne das fuehrende
 * Versionsbyte (Creator ab +0, 36 Byte). Der 2.x-Zweig bleibt deshalb
 * wie er war — er ist hier NICHT nachgemessen, weil im Korpus keine
 * 2.x-Datei liegt, und steht ausdruecklich als ungeprueft. */
static a2r_error_t parse_info_chunk(const uint8_t *data, size_t size,
                                    uint8_t version, a2r_info_t *info) {
    if (!data || !info) return A2R_ERR_NULL_PARAM;

    memset(info, 0, sizeof(*info));
    info->version = version;

    if (version == 2) {
        /* NICHT nachgemessen: keine 2.x-Datei im Korpus (MF-868). */
        if (size < 36) return A2R_ERR_BAD_CHUNK;
        copy_fixed_string(info->creator, data, 32, sizeof(info->creator));
        info->disk_type = data[32];
        info->write_protected = data[33] != 0;
        info->synchronized = data[34] != 0;
        return A2R_OK;
    }

    if (version == 3) {
        if (size < 37) return A2R_ERR_BAD_CHUNK;
        /* data[0] ist die INFO-Version, NICHT das erste Zeichen des
         * Erzeugers. Genau dieser eine Byte-Versatz zog sich durch alle
         * Felder. */
        copy_fixed_string(info->creator, &data[1], 32,
                          sizeof(info->creator));
        info->disk_type       = data[33];
        info->write_protected = data[34] != 0;
        info->synchronized    = data[35] != 0;
        /* data[36] ist die Zahl der Hartsektoren. Der Baum hat dafuer
         * kein Feld; sie wird bewusst nicht in `cleaned` o. ae.
         * einsortiert — ein falsch benanntes Feld ist schlimmer als ein
         * fehlendes. */
        return A2R_OK;
    }

    return A2R_ERR_BAD_CHUNK;
}

/** Parse STRM chunk (v2) */
static a2r_error_t parse_strm_chunk(a2r_context_t *ctx, 
                                    const uint8_t *data, size_t size) {
    if (!ctx || !data) return A2R_ERR_NULL_PARAM;
    
    /* STRM format: location entries followed by flux data */
    const uint8_t *ptr = data;
    const uint8_t *end = data + size;
    
    /* Count tracks first */
    uint8_t track_counts[A2R_MAX_TRACKS] = {0};
    const uint8_t *scan = data;
    
    while (scan + 10 <= end) {
        uint8_t location = scan[0];
        uint8_t capture_type = scan[1];
        uint32_t data_len = read_le32(&scan[2]);
        uint32_t tick_count = read_le32(&scan[6]);
        
        if (location == 0xFF) break;  /* End marker */
        if (location >= A2R_MAX_TRACKS) {
            scan += 10 + data_len;
            continue;
        }
        
        track_counts[location]++;
        scan += 10 + data_len;
    }
    
    /* Count unique tracks */
    ctx->track_count = 0;
    for (int i = 0; i < A2R_MAX_TRACKS; i++) {
        if (track_counts[i] > 0) ctx->track_count++;
    }
    
    if (ctx->track_count == 0) return A2R_ERR_NO_FLUX;

    /* Overflow check: track_count * sizeof(a2r_track_t) */
    if (ctx->track_count > SIZE_MAX / sizeof(a2r_track_t))
        return A2R_ERR_ALLOC;

    /* Allocate tracks */
    ctx->tracks = calloc(ctx->track_count, sizeof(a2r_track_t));
    if (!ctx->tracks) return A2R_ERR_ALLOC;

    /* Parse entries */
    uint8_t track_idx = 0;
    uint8_t current_location = 0xFF;
    a2r_track_t *current_track = NULL;

    ptr = data;
    while (ptr + 10 <= end) {
        uint8_t location = ptr[0];
        uint8_t capture_type = ptr[1];
        uint32_t data_len = read_le32(&ptr[2]);
        uint32_t tick_count = read_le32(&ptr[6]);

        if (location == 0xFF) break;
        if (location >= A2R_MAX_TRACKS) {
            ptr += 10 + data_len;
            continue;
        }

        /* New track? */
        if (location != current_location) {
            current_location = location;
            current_track = &ctx->tracks[track_idx++];
            current_track->track_number = location;
            current_track->side = 0;  /* v2 is always side 0 */
            current_track->capture_count = 0;
        }

        /* Add capture */
        if (current_track &&
            current_track->capture_count < A2R_MAX_CAPTURES) {

            a2r_capture_t *cap = &current_track->captures[current_track->capture_count];
            cap->capture_type = capture_type;
            cap->data_length = data_len;
            cap->tick_count = tick_count;

            /* Copy flux data */
            if (ptr + 10 + data_len <= end && data_len > 0) {
                /* Sanity cap on flux data size */
                if (data_len > A2R_MAX_FLUX_BYTES_PER_CAPTURE) {
                    ptr += 10 + data_len;
                    continue;
                }
                cap->data = malloc(data_len);
                if (cap->data) {
                    memcpy(cap->data, ptr + 10, data_len);
                    
                    /* Calculate duration */
                    uint64_t total_ticks = 0;
                    const uint8_t *flux = cap->data;
                    for (uint32_t i = 0; i < data_len; i++) {
                        if (flux[i] == 0xFF && i + 1 < data_len) {
                            total_ticks += flux[++i];
                        } else {
                            total_ticks += flux[i];
                        }
                    }
                    cap->duration_us = (total_ticks * A2R_TICK_NS) / 1000.0;
                    cap->rpm = a2r_duration_to_rpm(cap->duration_us);
                    
                    ctx->total_flux_bytes += data_len;
                    ctx->total_captures++;
                    
                    if (ctx->min_rpm == 0.0 || cap->rpm < ctx->min_rpm)
                        ctx->min_rpm = cap->rpm;
                    if (cap->rpm > ctx->max_rpm)
                        ctx->max_rpm = cap->rpm;
                }
            }
            
            current_track->capture_count++;
        }
        
        ptr += 10 + data_len;
    }
    
    return A2R_OK;
}

/** Parse RWCP chunk (v3).
 *
 * MF-868: der bisherige Rumpf traf die Struktur an keiner Stelle.
 *
 * Er nahm an: `location` in Byte 0, `side` in Byte 2, `data_len` in
 * Byte 6, feste Eintragsgroesse 10 + data_len — und uebersprang den
 * RWCP-Kopf GAR NICHT.
 *
 * Was wirklich dasteht (veroeffentlichte „A2R 3.x Disk Image
 * Reference", applesaucefdc.com/a2r/, byteweise an einer echten
 * Applesauce-Aufnahme nachgemessen):
 *
 *   Kopf, 16 Byte:
 *     +0   1   RWCP Version
 *     +1   4   Resolution, Pikosekunden je Tick
 *     +5  11   Reserve
 *
 *   danach Eintraege, Ende an Ende:
 *     +0   1   Mark: 0x43 'C' = Aufnahme, 0x58 'X' = Ende
 *     +1   1   Capture Type: 1 timing, 2 bits, 3 xtiming
 *     +2   2   Location (uint16 — Byte 2 ist NICHT die Seite)
 *     +4   1   Zahl der Index-Signale
 *     +5   4*N Index-Signale
 *     ..   4   Groesse der Aufnahmedaten
 *     ..   N   Aufnahmedaten
 *
 * Die Eintragsgroesse haengt also von der Zahl der Index-Signale ab.
 * Mit fester Groesse lief der alte Leser schon beim ersten Eintrag aus
 * dem Tritt — deshalb fand er auf einer echten Datei null Aufnahmen.
 *
 * Zur SEITE: die Referenz kennt in der Location keine getrennte
 * Seitenangabe; bei zweiseitigen Medien steckt sie in der Location
 * selbst. Dieser Leser fuehrt `side` deshalb konstant als 0 und legt
 * die volle Location in `track_number` ab, statt eine Aufteilung zu
 * erfinden. Was der Baum nicht belegt, behauptet er hier auch nicht.
 */
static a2r_error_t parse_rwcp_chunk(a2r_context_t *ctx,
                                    const uint8_t *data, size_t size) {
    if (!ctx || !data) return A2R_ERR_NULL_PARAM;
    if (size < 16) return A2R_ERR_BAD_CHUNK;

    /* Kopf: Version, Aufloesung, Reserve. */
    ctx->resolution_ps = read_le32(&data[1]);

    const uint8_t *const anfang = data + 16;
    const uint8_t *const end    = data + size;

    /* Erster Durchlauf: welche Locations kommen vor?
     *
     * `A2R_MAX_TRACKS` ist 160 (Viertelspuren). Eine Location darueber
     * wird UEBERSPRUNGEN und nicht auf den Bereich zurechtgebogen —
     * eine zurechtgebogene Spurnummer waere eine erfundene Angabe. */
    uint8_t gesehen[A2R_MAX_TRACKS];
    memset(gesehen, 0, sizeof(gesehen));

    const uint8_t *scan = anfang;
    while (scan < end) {
        uint8_t mark = scan[0];
        if (mark == 0x58) break;                 /* 'X' — Ende */
        if (mark != 0x43) break;                 /* nichts Bekanntes */
        if (scan + 5 > end) break;

        uint16_t location  = (uint16_t)(scan[2] | ((uint16_t)scan[3] << 8));
        uint8_t  idx_count = scan[4];

        const uint8_t *nach_idx = scan + 5 + (size_t)idx_count * 4u;
        if (nach_idx + 4 > end) break;
        uint32_t data_len = read_le32(nach_idx);

        const uint8_t *naechster = nach_idx + 4 + data_len;
        if (naechster > end) break;

        if (location < A2R_MAX_TRACKS) gesehen[location] = 1;
        scan = naechster;
    }

    ctx->track_count = 0;
    for (int k = 0; k < A2R_MAX_TRACKS; k++)
        if (gesehen[k]) ctx->track_count++;

    if (ctx->track_count == 0) return A2R_ERR_NO_FLUX;

    if (ctx->track_count > SIZE_MAX / sizeof(a2r_track_t))
        return A2R_ERR_ALLOC;

    ctx->tracks = calloc(ctx->track_count, sizeof(a2r_track_t));
    if (!ctx->tracks) return A2R_ERR_ALLOC;

    /* Die Locations der Reihe nach den Plaetzen zuordnen, damit der
     * zweite Durchlauf sie wiederfindet. */
    for (int k = 0, n = 0; k < A2R_MAX_TRACKS; k++) {
        if (!gesehen[k]) continue;
        ctx->tracks[n].track_number = (uint8_t)k;
        ctx->tracks[n].side = 0;
        n++;
    }

    /* Aufloesung in Nanosekunden. 0 heisst „steht nicht in der Datei" —
     * dann gilt weiterhin der Nennwert. */
    double ns_je_tick = (ctx->resolution_ps > 0)
                      ? (double)ctx->resolution_ps / 1000.0
                      : (double)A2R_TICK_NS;

    /* Zweiter Durchlauf: Aufnahmen einsortieren. */
    const uint8_t *ptr = anfang;
    while (ptr < end) {
        uint8_t mark = ptr[0];
        if (mark != 0x43) break;
        if (ptr + 5 > end) break;

        uint8_t  typ       = ptr[1];
        uint16_t location  = (uint16_t)(ptr[2] | ((uint16_t)ptr[3] << 8));
        uint8_t  idx_count = ptr[4];

        const uint8_t *nach_idx = ptr + 5 + (size_t)idx_count * 4u;
        if (nach_idx + 4 > end) break;
        uint32_t data_len = read_le32(nach_idx);
        const uint8_t *nutz = nach_idx + 4;
        const uint8_t *naechster = nutz + data_len;
        if (naechster > end) break;

        a2r_track_t *ziel = NULL;
        for (uint8_t n = 0; n < ctx->track_count; n++) {
            if (ctx->tracks[n].track_number == location) {
                ziel = &ctx->tracks[n];
                break;
            }
        }

        if (ziel && ziel->capture_count < A2R_MAX_CAPTURES &&
            data_len > 0 && data_len <= A2R_MAX_FLUX_BYTES_PER_CAPTURE) {
            a2r_capture_t *cap = &ziel->captures[ziel->capture_count];
            cap->capture_type = typ;
            cap->data_length  = data_len;
            cap->tick_count   = 0;

            cap->data = malloc(data_len);
            if (cap->data) {
                memcpy(cap->data, nutz, data_len);

                /* Flusswerte: 0xFF ist ein Ueberlauf und wird zum
                 * FOLGENDEN Wert addiert; der alte Rumpf zaehlte
                 * stattdessen nur das Folgebyte. */
                uint64_t ticks = 0;
                uint32_t traeger = 0;
                for (uint32_t k = 0; k < data_len; k++) {
                    if (cap->data[k] == 0xFF) { traeger += 0xFF; continue; }
                    ticks += traeger + cap->data[k];
                    traeger = 0;
                }
                ticks += traeger;

                cap->tick_count  = (uint32_t)((ticks > 0xFFFFFFFFu)
                                              ? 0xFFFFFFFFu : ticks);
                cap->duration_us = ((double)ticks * ns_je_tick) / 1000.0;
                cap->rpm         = a2r_duration_to_rpm(cap->duration_us);

                ctx->total_flux_bytes += data_len;
                ctx->total_captures++;

                if (ctx->min_rpm == 0.0 || cap->rpm < ctx->min_rpm)
                    ctx->min_rpm = cap->rpm;
                if (cap->rpm > ctx->max_rpm)
                    ctx->max_rpm = cap->rpm;
            }
            ziel->capture_count++;
        }

        ptr = naechster;
    }

    return A2R_OK;
}

/** Parse META chunk */
static a2r_error_t parse_meta_chunk(a2r_context_t *ctx,
                                    const uint8_t *data, size_t size) {
    if (!ctx || !data) return A2R_ERR_NULL_PARAM;
    if (size == 0 || size > A2R_MAX_META_SIZE) return A2R_OK;
    
    /* Count entries */
    uint16_t count = 0;
    const uint8_t *ptr = data;
    const uint8_t *end = data + size;
    
    while (ptr < end) {
        const uint8_t *tab = memchr(ptr, '\t', end - ptr);
        if (!tab) break;
        const uint8_t *nl = memchr(tab, '\n', end - tab);
        if (!nl) nl = end;
        count++;
        ptr = nl + 1;
    }
    
    if (count == 0) return A2R_OK;

    /* Overflow check: count * sizeof(a2r_meta_entry_t) */
    if (count > SIZE_MAX / sizeof(a2r_meta_entry_t)) return A2R_ERR_ALLOC;

    /* Allocate entries */
    ctx->metadata = calloc(count, sizeof(a2r_meta_entry_t));
    if (!ctx->metadata) return A2R_ERR_ALLOC;
    
    /* Parse entries */
    ptr = data;
    ctx->meta_count = 0;
    
    while (ptr < end && ctx->meta_count < count) {
        const uint8_t *tab = memchr(ptr, '\t', end - ptr);
        if (!tab) break;
        
        const uint8_t *nl = memchr(tab, '\n', end - tab);
        if (!nl) nl = end;
        
        a2r_meta_entry_t *entry = &ctx->metadata[ctx->meta_count];
        
        /* Copy key */
        size_t key_len = tab - ptr;
        if (key_len >= sizeof(entry->key)) key_len = sizeof(entry->key) - 1;
        memcpy(entry->key, ptr, key_len);
        entry->key[key_len] = '\0';
        
        /* Copy value */
        size_t val_len = nl - (tab + 1);
        if (val_len >= sizeof(entry->value)) val_len = sizeof(entry->value) - 1;
        memcpy(entry->value, tab + 1, val_len);
        entry->value[val_len] = '\0';
        
        ctx->meta_count++;
        ptr = nl + 1;
    }
    
    return A2R_OK;
}

/** Parse SLVD chunk (v3 solved/decoded data) */
static a2r_error_t parse_slvd_chunk(a2r_context_t *ctx,
                                    const uint8_t *data, size_t size) {
    if (!ctx || !data || !ctx->tracks) return A2R_ERR_NULL_PARAM;
    
    const uint8_t *ptr = data;
    const uint8_t *end = data + size;
    
    while (ptr + 6 <= end) {
        uint8_t track = ptr[0];
        uint8_t side = ptr[1];
        uint32_t nibble_len = read_le32(&ptr[2]);
        
        if (track == 0xFF) break;
        if (ptr + 6 + nibble_len > end) break;
        
        /* Find matching track */
        for (uint8_t i = 0; i < ctx->track_count; i++) {
            if (ctx->tracks[i].track_number == track &&
                ctx->tracks[i].side == side) {
                
                ctx->tracks[i].has_solved = true;
                ctx->tracks[i].nibble_count = nibble_len;
                /* Sanity cap on nibble data size */
                if (nibble_len > A2R_MAX_NIBBLE_BYTES) break;
                ctx->tracks[i].nibbles = malloc(nibble_len);
                
                if (ctx->tracks[i].nibbles) {
                    memcpy(ctx->tracks[i].nibbles, ptr + 6, nibble_len);
                }
                break;
            }
        }
        
        ptr += 6 + nibble_len;
    }
    
    return A2R_OK;
}

/*============================================================================
 * Public API Implementation
 *============================================================================*/

a2r_context_t *a2r_open(const char *path) {
    if (!path) return NULL;
    
    FILE *f = fopen(path, "rb");
    if (!f) return NULL;
    
    /* Get file size */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long file_size = ftell(f);
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    if (file_size < A2R_HEADER_SIZE + 8) {
        fclose(f);
        return NULL;
    }
    
    /* Read entire file */
    uint8_t *file_data = malloc(file_size);
    if (!file_data) {
        fclose(f);
        return NULL;
    }
    
    if (fread(file_data, 1, file_size, f) != (size_t)file_size) {
        free(file_data);
        fclose(f);
        return NULL;
    }
    fclose(f);
    
    /* Verify header */
    if (memcmp(file_data + 4, A2R_HEADER_SUFFIX, 4) != 0) {
        free(file_data);
        return NULL;
    }
    
    uint8_t version = 0;
    if (memcmp(file_data, A2R_MAGIC_V2, 4) == 0) {
        version = 2;
    } else if (memcmp(file_data, A2R_MAGIC_V3, 4) == 0) {
        version = 3;
    } else {
        free(file_data);
        return NULL;
    }
    
    /* Allocate context */
    a2r_context_t *ctx = calloc(1, sizeof(a2r_context_t));
    if (!ctx) {
        free(file_data);
        return NULL;
    }
    
    strncpy(ctx->path, path, sizeof(ctx->path) - 1);
    ctx->path[sizeof(ctx->path) - 1] = '\0';
    ctx->version = version;
    ctx->file_data = file_data;
    ctx->file_size = file_size;
    
    /* Parse chunks */
    const uint8_t *ptr = file_data + A2R_HEADER_SIZE;
    const uint8_t *end = file_data + file_size;
    bool has_info = false;
    bool has_flux = false;
    
    while (ptr + 8 <= end) {
        chunk_header_t hdr;
        memcpy(hdr.id, ptr, 4);
        hdr.size = read_le32(ptr + 4);
        
        if (ptr + 8 + hdr.size > end) break;
        
        const uint8_t *chunk_data = ptr + 8;
        
        if (memcmp(hdr.id, A2R_CHUNK_INFO, 4) == 0) {
            parse_info_chunk(chunk_data, hdr.size, version, &ctx->info);
            has_info = true;
        } else if (memcmp(hdr.id, A2R_CHUNK_STRM, 4) == 0 && version == 2) {
            parse_strm_chunk(ctx, chunk_data, hdr.size);
            has_flux = true;
        } else if (memcmp(hdr.id, A2R_CHUNK_RWCP, 4) == 0 && version == 3) {
            parse_rwcp_chunk(ctx, chunk_data, hdr.size);
            has_flux = true;
        } else if (memcmp(hdr.id, A2R_CHUNK_SLVD, 4) == 0 && version == 3) {
            parse_slvd_chunk(ctx, chunk_data, hdr.size);
        } else if (memcmp(hdr.id, A2R_CHUNK_META, 4) == 0) {
            parse_meta_chunk(ctx, chunk_data, hdr.size);
        }
        
        ptr += 8 + hdr.size;
    }
    
    if (!has_info || !has_flux) {
        a2r_close(ctx);
        return NULL;
    }
    
    return ctx;
}

void a2r_close(a2r_context_t *ctx) {
    if (!ctx) return;
    
    /* Free tracks */
    if (ctx->tracks) {
        for (uint8_t i = 0; i < ctx->track_count; i++) {
            a2r_track_t *track = &ctx->tracks[i];
            
            /* Free captures */
            for (uint8_t j = 0; j < track->capture_count; j++) {
                free(track->captures[j].data);
            }
            
            /* Free nibbles */
            free(track->nibbles);
        }
        free(ctx->tracks);
    }
    
    /* Free metadata */
    free(ctx->metadata);
    
    /* Free file data */
    free(ctx->file_data);
    
    free(ctx);
}

a2r_error_t a2r_read_track(a2r_context_t *ctx, 
                           uint8_t quarter_track,
                           uint8_t side,
                           a2r_track_t *track) {
    if (!ctx || !track) return A2R_ERR_NULL_PARAM;
    if (!ctx->tracks) return A2R_ERR_NO_FLUX;
    
    memset(track, 0, sizeof(*track));
    
    /* Find track */
    for (uint8_t i = 0; i < ctx->track_count; i++) {
        if (ctx->tracks[i].track_number == quarter_track &&
            ctx->tracks[i].side == side) {
            
            /* Copy track info */
            track->track_number = ctx->tracks[i].track_number;
            track->side = ctx->tracks[i].side;
            track->capture_count = ctx->tracks[i].capture_count;
            track->has_solved = ctx->tracks[i].has_solved;
            
            /* Copy captures (deep copy data) */
            for (uint8_t j = 0; j < track->capture_count; j++) {
                a2r_capture_t *src = &ctx->tracks[i].captures[j];
                a2r_capture_t *dst = &track->captures[j];
                
                dst->capture_type = src->capture_type;
                dst->data_length = src->data_length;
                dst->tick_count = src->tick_count;
                dst->duration_us = src->duration_us;
                dst->rpm = src->rpm;
                
                if (src->data && src->data_length > 0) {
                    dst->data = malloc(src->data_length);
                    if (dst->data) {
                        memcpy(dst->data, src->data, src->data_length);
                    }
                }
            }
            
            /* Copy nibbles if present */
            if (ctx->tracks[i].has_solved && ctx->tracks[i].nibbles) {
                track->nibble_count = ctx->tracks[i].nibble_count;
                track->nibbles = malloc(track->nibble_count);
                if (track->nibbles) {
                    memcpy(track->nibbles, ctx->tracks[i].nibbles, 
                           track->nibble_count);
                }
            }
            
            return A2R_OK;
        }
    }
    
    return A2R_ERR_TRACK_RANGE;
}

void a2r_free_track(a2r_track_t *track) {
    if (!track) return;
    
    for (uint8_t i = 0; i < track->capture_count; i++) {
        free(track->captures[i].data);
        track->captures[i].data = NULL;
    }
    
    free(track->nibbles);
    track->nibbles = NULL;
    
    memset(track, 0, sizeof(*track));
}

a2r_error_t a2r_decode_flux(const a2r_capture_t *capture,
                            a2r_flux_sample_t *samples,
                            uint32_t max_samples,
                            uint32_t *out_count) {
    if (!capture || !samples || !out_count) return A2R_ERR_NULL_PARAM;
    if (!capture->data) return A2R_ERR_NO_FLUX;
    
    *out_count = 0;
    double time_ns = 0.0;
    const uint8_t *data = capture->data;
    uint32_t len = capture->data_length;
    
    for (uint32_t i = 0; i < len && *out_count < max_samples; i++) {
        uint32_t tick;
        bool is_extended = false;
        
        if (data[i] == 0xFF && i + 1 < len) {
            /* Extended timing: 0xFF followed by count */
            tick = data[++i];
            is_extended = true;
        } else if (data[i] == 0x00) {
            /* Sync byte, skip */
            continue;
        } else {
            tick = data[i];
        }
        
        time_ns += tick * A2R_TICK_NS;
        
        samples[*out_count].tick = tick;
        samples[*out_count].time_ns = time_ns;
        samples[*out_count].is_extended = is_extended;
        (*out_count)++;
    }
    
    return A2R_OK;
}

a2r_error_t a2r_flux_to_nibbles(const a2r_capture_t *capture,
                                double bit_time_ns,
                                uint8_t *nibbles,
                                uint32_t max_nibbles,
                                uint32_t *out_count) {
    if (!capture || !nibbles || !out_count) return A2R_ERR_NULL_PARAM;
    if (!capture->data) return A2R_ERR_NO_FLUX;
    
    /* Default Apple II bit timing: 4µs = 4000ns */
    if (bit_time_ns <= 0.0) bit_time_ns = 4000.0;
    
    *out_count = 0;
    
    const uint8_t *data = capture->data;
    uint32_t len = capture->data_length;
    
    /* Accumulate flux intervals and decode to bits */
    double accum_ns = 0.0;
    uint8_t current_byte = 0;
    int bit_count = 0;
    
    for (uint32_t i = 0; i < len; i++) {
        uint32_t tick;
        
        if (data[i] == 0xFF && i + 1 < len) {
            tick = data[++i];
        } else if (data[i] == 0x00) {
            continue;
        } else {
            tick = data[i];
        }
        
        double interval_ns = tick * A2R_TICK_NS;
        accum_ns += interval_ns;
        
        /* Determine number of bit cells */
        int bit_cells = (int)((accum_ns + bit_time_ns / 2) / bit_time_ns);
        accum_ns -= bit_cells * bit_time_ns;
        
        /* Generate bits */
        for (int b = 0; b < bit_cells && *out_count < max_nibbles; b++) {
            /* Last bit before flux transition is always 1 */
            int bit_value = (b == bit_cells - 1) ? 1 : 0;
            
            current_byte = (current_byte << 1) | bit_value;
            bit_count++;
            
            /* Apple II nibbles are 8 bits with high bit set */
            if (bit_count == 8) {
                nibbles[(*out_count)++] = current_byte;
                current_byte = 0;
                bit_count = 0;
            }
        }
    }
    
    return A2R_OK;
}

a2r_error_t a2r_get_info(const a2r_context_t *ctx, a2r_info_t *info) {
    if (!ctx || !info) return A2R_ERR_NULL_PARAM;
    *info = ctx->info;
    return A2R_OK;
}

a2r_error_t a2r_get_metadata(const a2r_context_t *ctx,
                             const char *key,
                             char *value,
                             size_t value_size) {
    if (!ctx || !key || !value || value_size == 0) return A2R_ERR_NULL_PARAM;
    
    for (uint16_t i = 0; i < ctx->meta_count; i++) {
        if (strcmp(ctx->metadata[i].key, key) == 0) {
            safe_strcpy(value, ctx->metadata[i].value, value_size);
            return A2R_OK;
        }
    }
    
    value[0] = '\0';
    return A2R_ERR_NULL_PARAM;  /* Key not found */
}

bool a2r_is_valid_file(const char *path) {
    if (!path) return false;
    
    FILE *f = fopen(path, "rb");
    if (!f) return false;
    
    uint8_t header[8];
    bool valid = false;
    
    if (fread(header, 1, 8, f) == 8) {
        if ((memcmp(header, A2R_MAGIC_V2, 4) == 0 ||
             memcmp(header, A2R_MAGIC_V3, 4) == 0) &&
            memcmp(header + 4, A2R_HEADER_SUFFIX, 4) == 0) {
            valid = true;
        }
    }
    
    fclose(f);
    return valid;
}

uint8_t a2r_get_file_version(const char *path) {
    if (!path) return 0;
    
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    
    uint8_t header[8];
    uint8_t version = 0;
    
    if (fread(header, 1, 8, f) == 8) {
        if (memcmp(header + 4, A2R_HEADER_SUFFIX, 4) == 0) {
            if (memcmp(header, A2R_MAGIC_V2, 4) == 0) version = 2;
            else if (memcmp(header, A2R_MAGIC_V3, 4) == 0) version = 3;
        }
    }
    
    fclose(f);
    return version;
}

const char *a2r_error_string(a2r_error_t err) {
    if (err >= A2R_ERR_COUNT) return "Unknown error";
    return error_strings[err];
}

const char *a2r_disk_type_string(uint8_t disk_type) {
    /* MF-868: die Schranke stand fest bei 4 und blieb es, als die
     * Tabelle wuchs — dann waere Typ 6 wieder „Unknown" gewesen, mit
     * dem Eintrag daneben. Sie kommt jetzt aus der Tabelle selbst. */
    const size_t n = sizeof(disk_type_strings) / sizeof(disk_type_strings[0]);
    if ((size_t)disk_type >= n) return "Unknown";
    return disk_type_strings[disk_type];
}

a2r_error_t a2r_get_raw_timings(const a2r_capture_t *capture,
                                double *timings,
                                uint32_t max_timings,
                                uint32_t *out_count) {
    if (!capture || !timings || !out_count) return A2R_ERR_NULL_PARAM;
    if (!capture->data) return A2R_ERR_NO_FLUX;
    
    *out_count = 0;
    const uint8_t *data = capture->data;
    uint32_t len = capture->data_length;
    
    for (uint32_t i = 0; i < len && *out_count < max_timings; i++) {
        uint32_t tick;
        
        if (data[i] == 0xFF && i + 1 < len) {
            tick = data[++i];
        } else if (data[i] == 0x00) {
            continue;
        } else {
            tick = data[i];
        }
        
        timings[(*out_count)++] = tick * A2R_TICK_NS;
    }
    
    return A2R_OK;
}

a2r_error_t a2r_fuse_captures(const a2r_capture_t *captures,
                              uint8_t count,
                              a2r_capture_t *fused,
                              uint8_t *weak_mask,
                              size_t mask_size) {
    if (!captures || !fused || count == 0) return A2R_ERR_NULL_PARAM;
    if (count == 1) {
        /* Single capture, just copy */
        *fused = captures[0];
        fused->data = malloc(captures[0].data_length);
        if (fused->data) {
            memcpy(fused->data, captures[0].data, captures[0].data_length);
        }
        return A2R_OK;
    }
    
    /* Use first capture as reference */
    const a2r_capture_t *ref = &captures[0];
    fused->capture_type = ref->capture_type;
    fused->data_length = ref->data_length;
    fused->tick_count = ref->tick_count;
    fused->duration_us = ref->duration_us;
    fused->rpm = ref->rpm;
    
    fused->data = malloc(ref->data_length);
    if (!fused->data) return A2R_ERR_ALLOC;
    
    /* For each byte position, use majority vote.
     *
     * counts[] and max_count widened from uint8_t to uint32_t (parallel
     * to recovery Finding 06). `count` is uint8_t today so the bug is
     * dormant, but the same future-proofing rationale applies: if the
     * function signature is widened for a stress harness or external
     * recovery wizard, a uint8_t counter wraps silently at 256 identical
     * inputs and turns the majority winner into a near-loser. */
    for (uint32_t i = 0; i < ref->data_length; i++) {
        uint32_t counts[256] = {0};
        uint8_t  max_val   = 0;
        uint32_t max_count = 0;

        for (uint8_t c = 0; c < count; c++) {
            if (i < captures[c].data_length) {
                uint8_t val = captures[c].data[i];
                counts[val]++;
                if (counts[val] > max_count) {
                    max_count = counts[val];
                    max_val   = val;
                }
            }
        }

        fused->data[i] = max_val;

        /* Mark as weak if not unanimous */
        if (weak_mask && i / 8 < mask_size) {
            if (max_count < count) {
                weak_mask[i / 8] |= (1 << (i % 8));
            }
        }
    }
    
    return A2R_OK;
}

a2r_error_t a2r_analyze_protection(const a2r_track_t *track,
                                   bool *has_weak,
                                   bool *has_timing,
                                   bool *has_halftrack) {
    if (!track) return A2R_ERR_NULL_PARAM;
    
    /* Initialize outputs */
    if (has_weak) *has_weak = false;
    if (has_timing) *has_timing = false;
    if (has_halftrack) *has_halftrack = false;
    
    /* Check for half/quarter tracks */
    if (has_halftrack && (track->track_number % 4) != 0) {
        *has_halftrack = true;
    }
    
    /* Need multiple captures to detect weak bits */
    if (has_weak && track->capture_count > 1) {
        /* Compare captures for variations */
        const a2r_capture_t *ref = &track->captures[0];
        
        for (uint8_t c = 1; c < track->capture_count; c++) {
            const a2r_capture_t *cmp = &track->captures[c];
            uint32_t min_len = ref->data_length < cmp->data_length ? 
                               ref->data_length : cmp->data_length;
            
            uint32_t diff_count = 0;
            for (uint32_t i = 0; i < min_len; i++) {
                if (ref->data[i] != cmp->data[i]) diff_count++;
            }
            
            /* More than 1% difference indicates weak bits */
            if (diff_count * 100 > min_len) {
                *has_weak = true;
                break;
            }
        }
    }
    
    /* Check for timing protection (unusual RPM variance) */
    if (has_timing && track->capture_count > 0) {
        double min_rpm = track->captures[0].rpm;
        double max_rpm = track->captures[0].rpm;
        
        for (uint8_t c = 1; c < track->capture_count; c++) {
            if (track->captures[c].rpm < min_rpm)
                min_rpm = track->captures[c].rpm;
            if (track->captures[c].rpm > max_rpm)
                max_rpm = track->captures[c].rpm;
        }
        
        /* More than 2% RPM variance indicates timing protection */
        if (max_rpm > 0 && (max_rpm - min_rpm) / max_rpm > 0.02) {
            *has_timing = true;
        }
    }
    
    return A2R_OK;
}

/*============================================================================
 * Unit Tests
 *============================================================================*/

/*
 * MF-851: hier stand ein `#ifdef UFT_UNIT_TESTS`-Block mit 6
 * Zusagen. `UFT_UNIT_TESTS` wird im ganzen Baum NIRGENDS definiert —
 * weder im qmake-`.pro`, noch in einer `CMakeLists.txt`, noch als
 * Compilerschalter. Er wurde nie uebersetzt und konnte nie rot
 * werden; dieselbe Klasse wie MF-830 und MF-596 (P3-89).
 *
 * Die Faelle laufen jetzt unter
 *     tests/test_a2r_hilfsfunktionen_leben.c
 * und damit in CI. Alle waren inhaltlich richtig — was den Befund
 * nicht kleiner macht: richtig und ungeprueft ist nicht dasselbe wie
 * richtig und bewacht.
 */

