/**
 * @file uft_gcr_ops.c
 * @brief C64/1541 GCR Operations Implementation
 * 
 * Verhalten nach der nibtools-Dokumentation und -Quelle (Pete Rittwage,
 * c64preservation.com) — EIGENSTAENDIGE Implementierung, kein Port.
 *
 * MF-635: hier stand "Based on nibtools gcr.c". Das ist eine
 * Ableitungserklaerung, und die Lizenz der Quelle waere fuer diesen
 * GPL-2.0-or-later-Baum eine Lizenzfrage. Ein Aehnlichkeitsaudit gegen
 * gcr.c @0abdc11 hat sie entschieden:
 *
 * MF-1008 berichtigt die Lizenzangabe, die hier stand ("seit
 * 2025-01-30 unter GPL-3"): nibtools steht unter der **Apache License
 * 2.0** — Eigentuemer-Feststellung 2026-09-10, Quelle
 * `nibtools-extra/LICENSE`. Am Ergebnis aendert das nichts, und zwar
 * aus einem Grund, der hierhin gehoert: **Apache-2.0 ist mit GPL-2
 * ebenso unvereinbar wie GPL-3-only**, nur auf anderem Weg (Patent-
 * und Beendigungsklauseln statt Versionsschritt). Der Schluss "waere
 * eine Lizenzfrage, also Audit" traegt unveraendert — die frueher
 * genannte Lizenz war falsch, die Vorsicht nicht.
 *
 *   strip_runs        nibtools kompaktiert in-place ueber source/buffer-
 *                     Zeiger mit run-Zaehler und Parametern
 *                     (length_max, minrun, target); hier: eigener
 *                     Ausgabepuffer, Lauflaengen-Scan, getrennte Zweige
 *                     fuer Sync und Gap, Parameter (min_sync, min_gap)
 *   kill_partial_sync nibtools: vier feste 1000er-Felder und ein
 *                     locked-Automat mit 10-Bit-Sync-Idiom; hier: eine
 *                     Suchschleife ueber gcr_find_sync/_end
 *   reduce_runs       drueben eine do/while-Schleife um strip_runs mit
 *                     fuenf Parametern; hier ein Einzeiler mit vier
 *
 * Woertliche Kommentar-Echos: EINES, und das ist der Fachbegriff
 * "header checksum". Keine der auffaelligen nibtools-Konstanten
 * (0x4b "Original Format Pattern", die Tri-Bit-/Low-Frequency-Notizen)
 * kommt hier vor.
 *
 * Was UEBERNOMMEN ist, ist das Vokabular: die Zerlegung folgt nibtools'
 * Funktionsnamen mit gcr_-Praefix (check_errors, compare_sectors,
 * kill_partial_sync, lengthen_sync, reduce_gaps/runs, strip_runs). Das
 * ist gewollt und benannt — die Rumpfe sind es nicht.
 *
 * Oracle fuer diese Datei: nibscan aus rittwage/nibtools, hardwarefrei
 * baubar (SCOUT-20).
 * 
 * @author UFT Project
 * @date 2026-01-16
 */

#include "uft/formats/c64/uft_gcr_ops.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================================
 * GCR Encoding Tables
 * ============================================================================ */

/** Nibble to GCR encoding table */
static const uint8_t gcr_encode_table[16] = {
    0x0A, 0x0B, 0x12, 0x13, 0x0E, 0x0F, 0x16, 0x17,
    0x09, 0x19, 0x1A, 0x1B, 0x0D, 0x1D, 0x1E, 0x15
};

/** GCR to high nibble decode table */
static const uint8_t gcr_decode_high[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x80, 0x00, 0x10, 0xFF, 0xC0, 0x40, 0x50,
    0xFF, 0xFF, 0x20, 0x30, 0xFF, 0xF0, 0x60, 0x70,
    0xFF, 0x90, 0xA0, 0xB0, 0xFF, 0xD0, 0xE0, 0xFF
};

/** GCR to low nibble decode table */
static const uint8_t gcr_decode_low[32] = {
    0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x00, 0x01, 0xFF, 0x0C, 0x04, 0x05,
    0xFF, 0xFF, 0x02, 0x03, 0xFF, 0x0F, 0x06, 0x07,
    0xFF, 0x09, 0x0A, 0x0B, 0xFF, 0x0D, 0x0E, 0xFF
};

/** Sectors per track for 1541 (track 1-42) */
static const int sector_map[43] = {
    0,
    21, 21, 21, 21, 21, 21, 21, 21, 21, 21,  /*  1 - 10 */
    21, 21, 21, 21, 21, 21, 21, 19, 19, 19,  /* 11 - 20 */
    19, 19, 19, 19, 18, 18, 18, 18, 18, 18,  /* 21 - 30 */
    17, 17, 17, 17, 17, 17, 17, 17, 17, 17,  /* 31 - 40 */
    17, 17                                    /* 41 - 42 */
};

/** Track capacity for each density */
static const size_t track_capacity[4] = { 6250, 6666, 7142, 7692 };

/* ============================================================================
 * GCR Table Access
 * ============================================================================ */

const uint8_t *gcr_get_encode_table(void)
{
    return gcr_encode_table;
}

const uint8_t *gcr_get_decode_high_table(void)
{
    return gcr_decode_high;
}

const uint8_t *gcr_get_decode_low_table(void)
{
    return gcr_decode_low;
}

/* ============================================================================
 * GCR Encode/Decode
 * ============================================================================ */

/**
 * @brief Encode bytes to GCR
 */
size_t gcr_encode(const uint8_t *plain, size_t plain_size, uint8_t *gcr)
{
    if (!plain || !gcr || plain_size == 0) return 0;
    
    size_t out_pos = 0;
    
    /* Process 4 bytes at a time */
    for (size_t i = 0; i < plain_size; i += 4) {
        /* Get 4 input bytes (pad with 0 if needed) */
        uint8_t b0 = plain[i];
        uint8_t b1 = (i + 1 < plain_size) ? plain[i + 1] : 0;
        uint8_t b2 = (i + 2 < plain_size) ? plain[i + 2] : 0;
        uint8_t b3 = (i + 3 < plain_size) ? plain[i + 3] : 0;
        
        /* Encode high and low nibbles to GCR */
        uint8_t g0 = gcr_encode_table[b0 >> 4];
        uint8_t g1 = gcr_encode_table[b0 & 0x0F];
        uint8_t g2 = gcr_encode_table[b1 >> 4];
        uint8_t g3 = gcr_encode_table[b1 & 0x0F];
        uint8_t g4 = gcr_encode_table[b2 >> 4];
        uint8_t g5 = gcr_encode_table[b2 & 0x0F];
        uint8_t g6 = gcr_encode_table[b3 >> 4];
        uint8_t g7 = gcr_encode_table[b3 & 0x0F];
        
        /* Pack 8 5-bit values into 5 bytes */
        gcr[out_pos++] = (g0 << 3) | (g1 >> 2);
        gcr[out_pos++] = (g1 << 6) | (g2 << 1) | (g3 >> 4);
        gcr[out_pos++] = (g3 << 4) | (g4 >> 1);
        gcr[out_pos++] = (g4 << 7) | (g5 << 2) | (g6 >> 3);
        gcr[out_pos++] = (g6 << 5) | g7;
    }
    
    return out_pos;
}

/**
 * @brief Decode GCR to bytes
 */
size_t gcr_decode(const uint8_t *gcr, size_t gcr_size, uint8_t *plain, size_t *error_count)
{
    if (!gcr || !plain || gcr_size < 5) return 0;
    
    size_t out_pos = 0;
    size_t errors = 0;
    
    /* Process 5 bytes at a time */
    for (size_t i = 0; i + 4 < gcr_size; i += 5) {
        /* Extract 8 5-bit GCR values */
        uint8_t g0 = (gcr[i] >> 3) & 0x1F;
        uint8_t g1 = ((gcr[i] << 2) | (gcr[i+1] >> 6)) & 0x1F;
        uint8_t g2 = (gcr[i+1] >> 1) & 0x1F;
        uint8_t g3 = ((gcr[i+1] << 4) | (gcr[i+2] >> 4)) & 0x1F;
        uint8_t g4 = ((gcr[i+2] << 1) | (gcr[i+3] >> 7)) & 0x1F;
        uint8_t g5 = (gcr[i+3] >> 2) & 0x1F;
        uint8_t g6 = ((gcr[i+3] << 3) | (gcr[i+4] >> 5)) & 0x1F;
        uint8_t g7 = gcr[i+4] & 0x1F;
        
        /* Decode to nibbles */
        uint8_t h0 = gcr_decode_high[g0];
        uint8_t l0 = gcr_decode_low[g1];
        uint8_t h1 = gcr_decode_high[g2];
        uint8_t l1 = gcr_decode_low[g3];
        uint8_t h2 = gcr_decode_high[g4];
        uint8_t l2 = gcr_decode_low[g5];
        uint8_t h3 = gcr_decode_high[g6];
        uint8_t l3 = gcr_decode_low[g7];
        
        /* Check for errors */
        if (h0 == 0xFF || l0 == 0xFF) errors++;
        if (h1 == 0xFF || l1 == 0xFF) errors++;
        if (h2 == 0xFF || l2 == 0xFF) errors++;
        if (h3 == 0xFF || l3 == 0xFF) errors++;
        
        /* Combine nibbles (use 0 for errors) */
        plain[out_pos++] = (h0 & 0xF0) | (l0 & 0x0F);
        plain[out_pos++] = (h1 & 0xF0) | (l1 & 0x0F);
        plain[out_pos++] = (h2 & 0xF0) | (l2 & 0x0F);
        plain[out_pos++] = (h3 & 0xF0) | (l3 & 0x0F);
    }
    
    if (error_count) *error_count = errors;
    return out_pos;
}

/**
 * @brief Check GCR for errors
 */
size_t gcr_check_errors(const uint8_t *gcr, size_t size)
{
    if (!gcr || size < 2) return 0;
    
    size_t errors = 0;
    
    for (size_t i = 0; i < size - 1; i++) {
        /* Extract 5-bit GCR values at each position */
        uint8_t high = (gcr[i] >> 3) & 0x1F;
        uint8_t low = ((gcr[i] << 2) | (gcr[i+1] >> 6)) & 0x1F;
        
        if (gcr_decode_high[high] == 0xFF) errors++;
        if (gcr_decode_low[low] == 0xFF) errors++;
    }
    
    return errors;
}

/* ============================================================================
 * Sync Operations
 * ============================================================================ */

/**
 * @brief Find sync mark
 */
int gcr_find_sync(const uint8_t *gcr, size_t size, size_t start)
{
    if (!gcr || size == 0) return -1;
    
    for (size_t i = start; i < size - 1; i++) {
        /* Sync is 0xFF with MSB set in next byte = 9 one-bits on a byte
         * boundary. Byte-aligned by construction; the 1541 itself needs 10
         * one-bits at any bit offset (uft_gcr_ops.h, definition B). */
        if (gcr[i] == GCR_SYNC_BYTE && (gcr[i+1] & 0x80)) {
            return (int)i;
        }
    }
    
    return -1;
}

/**
 * @brief Find end of sync
 */
size_t gcr_find_sync_end(const uint8_t *gcr, size_t size, size_t sync_start)
{
    if (!gcr || sync_start >= size) return sync_start;
    
    size_t pos = sync_start;
    while (pos < size && gcr[pos] == GCR_SYNC_BYTE) {
        pos++;
    }
    
    return pos;
}

/**
 * @brief Count BYTE-ALIGNED sync marks in a track
 *
 * Definition (A) in uft_gcr_ops.h: a 0xFF byte followed by a byte with its MSB
 * set, i.e. 9 one-bits starting on a byte boundary. This is deliberately NOT
 * the 1541 hardware condition (>= 10 one-bits at any bit position) — see
 * KNOWN_ISSUES FMT-14 for the measured divergence on protected media.
 */
int gcr_count_syncs_bytealigned(const uint8_t *gcr, size_t size)
{
    if (!gcr || size == 0) return 0;
    
    int count = 0;
    size_t pos = 0;
    
    while (pos < size) {
        int sync = gcr_find_sync(gcr, size, pos);
        if (sync < 0) break;
        
        count++;
        pos = gcr_find_sync_end(gcr, size, sync) + 1;
    }
    
    return count;
}

/**
 * @brief Get longest sync
 */
size_t gcr_longest_sync(const uint8_t *gcr, size_t size, size_t *position)
{
    if (!gcr || size == 0) return 0;
    
    size_t longest = 0;
    size_t longest_pos = 0;
    size_t pos = 0;
    
    while (pos < size) {
        int sync = gcr_find_sync(gcr, size, pos);
        if (sync < 0) break;
        
        size_t end = gcr_find_sync_end(gcr, size, sync);
        size_t len = end - sync;
        
        if (len > longest) {
            longest = len;
            longest_pos = sync;
        }
        
        pos = end + 1;
    }
    
    if (position) *position = longest_pos;
    return longest;
}

/**
 * @brief Lengthen sync marks
 */
int gcr_lengthen_syncs(uint8_t *gcr, size_t size, int min_length)
{
    if (!gcr || size == 0 || min_length <= 0) return 0;
    
    int lengthened = 0;
    size_t pos = 0;
    
    while (pos < size) {
        int sync = gcr_find_sync(gcr, size, pos);
        if (sync < 0) break;
        
        size_t end = gcr_find_sync_end(gcr, size, sync);
        size_t len = end - sync;
        
        if (len < (size_t)min_length && end < size) {
            /* Try to extend sync */
            while (len < (size_t)min_length && end < size) {
                gcr[end] = GCR_SYNC_BYTE;
                end++;
                len++;
                lengthened++;
            }
        }
        
        pos = end + 1;
    }
    
    return lengthened;
}

/**
 * @brief Kill partial syncs
 */
int gcr_kill_partial_syncs(uint8_t *gcr, size_t size, int min_length)
{
    if (!gcr || size == 0 || min_length <= 0) return 0;
    
    int killed = 0;
    size_t pos = 0;
    
    while (pos < size) {
        int sync = gcr_find_sync(gcr, size, pos);
        if (sync < 0) break;
        
        size_t end = gcr_find_sync_end(gcr, size, sync);
        size_t len = end - sync;
        
        if (len < (size_t)min_length) {
            /* Replace with gap */
            for (size_t i = sync; i < end; i++) {
                gcr[i] = GCR_GAP_BYTE;
            }
            killed++;
        }
        
        pos = end + 1;
    }
    
    return killed;
}

/* ============================================================================
 * Gap Operations
 * ============================================================================ */

/**
 * @brief Find gap mark
 */
int gcr_find_gap(const uint8_t *gcr, size_t size, size_t start)
{
    if (!gcr || size == 0) return -1;
    
    for (size_t i = start; i < size - 2; i++) {
        /* Gap is repeated identical bytes (usually 0x55) */
        if (gcr[i] == gcr[i+1] && gcr[i] == gcr[i+2] && gcr[i] != GCR_SYNC_BYTE) {
            return (int)i;
        }
    }
    
    return -1;
}

/**
 * @brief Get longest gap
 */
size_t gcr_longest_gap(const uint8_t *gcr, size_t size, size_t *position, uint8_t *gap_byte)
{
    if (!gcr || size == 0) return 0;
    
    size_t longest = 0;
    size_t longest_pos = 0;
    uint8_t longest_byte = 0;
    
    size_t run_start = 0;
    size_t run_len = 1;
    
    for (size_t i = 1; i < size; i++) {
        if (gcr[i] == gcr[i-1] && gcr[i] != GCR_SYNC_BYTE) {
            run_len++;
        } else {
            if (run_len > longest && gcr[i-1] != GCR_SYNC_BYTE) {
                longest = run_len;
                longest_pos = run_start;
                longest_byte = gcr[i-1];
            }
            run_start = i;
            run_len = 1;
        }
    }
    
    /* Check final run */
    if (run_len > longest && gcr[size-1] != GCR_SYNC_BYTE) {
        longest = run_len;
        longest_pos = run_start;
        longest_byte = gcr[size-1];
    }
    
    if (position) *position = longest_pos;
    if (gap_byte) *gap_byte = longest_byte;
    return longest;
}

/**
 * @brief Strip sync/gap runs to minimum
 */
size_t gcr_strip_runs(uint8_t *gcr, size_t size, int min_sync, int min_gap)
{
    if (!gcr || size == 0) return 0;
    
    uint8_t *output = malloc(size);
    if (!output) return size;
    
    size_t out_pos = 0;
    size_t i = 0;
    
    while (i < size) {
        /* Check for sync run */
        if (gcr[i] == GCR_SYNC_BYTE) {
            int run_len = 0;
            while (i + run_len < size && gcr[i + run_len] == GCR_SYNC_BYTE) {
                run_len++;
            }
            
            /* Keep minimum sync bytes */
            int keep = (run_len < min_sync) ? run_len : min_sync;
            for (int j = 0; j < keep; j++) {
                output[out_pos++] = GCR_SYNC_BYTE;
            }
            i += run_len;
        }
        /* Check for gap run */
        else if (i + 1 < size && gcr[i] == gcr[i+1]) {
            uint8_t gap_byte = gcr[i];
            int run_len = 0;
            while (i + run_len < size && gcr[i + run_len] == gap_byte) {
                run_len++;
            }
            
            /* Keep minimum gap bytes */
            int keep = (run_len < min_gap) ? run_len : min_gap;
            for (int j = 0; j < keep; j++) {
                output[out_pos++] = gap_byte;
            }
            i += run_len;
        }
        else {
            output[out_pos++] = gcr[i++];
        }
    }
    
    memcpy(gcr, output, out_pos);
    free(output);
    
    return out_pos;
}

/**
 * @brief Reduce sync/gap runs
 */
size_t gcr_reduce_runs(uint8_t *gcr, size_t size, int max_sync, int max_gap)
{
    return gcr_strip_runs(gcr, size, max_sync, max_gap);
}

/**
 * @brief Reduce gaps to minimum
 */
size_t gcr_reduce_gaps(uint8_t *gcr, size_t size)
{
    return gcr_strip_runs(gcr, size, GCR_MIN_SYNC, 4);
}

/* ============================================================================
 * Track Cycle Detection
 * ============================================================================ */

/**
 * @brief Detect track cycle
 */
bool gcr_detect_cycle(const uint8_t *gcr, size_t size, size_t min_length, gcr_cycle_t *result)
{
    if (!gcr || !result || size < min_length * 2) {
        if (result) result->found = false;
        return false;
    }
    
    memset(result, 0, sizeof(gcr_cycle_t));
    
    /* Find first sync */
    int first_sync = gcr_find_sync(gcr, size, 0);
    if (first_sync < 0) return false;
    
    size_t data_start = gcr_find_sync_end(gcr, size, first_sync);
    
    /* Try to find matching pattern */
    for (size_t cycle_len = min_length; cycle_len + data_start < size; cycle_len++) {
        int match_count = 0;
        int check_count = 0;
        
        /* Check if pattern repeats */
        for (size_t i = 0; i < 100 && data_start + i + cycle_len < size; i += 10) {
            check_count++;
            if (gcr[data_start + i] == gcr[data_start + i + cycle_len]) {
                match_count++;
            }
        }
        
        if (check_count > 0 && match_count * 100 / check_count > 90) {
            result->found = true;
            result->cycle_start = first_sync;
            result->cycle_length = cycle_len;
            result->data_start = data_start;
            result->quality = match_count * 100 / check_count;
            return true;
        }
    }
    
    return false;
}

/**
 * @brief Extract track data using cycle
 */
size_t gcr_extract_cycle(const uint8_t *gcr, size_t size, const gcr_cycle_t *cycle, uint8_t *output)
{
    if (!gcr || !cycle || !output || !cycle->found) return 0;
    
    size_t length = cycle->cycle_length;
    if (cycle->data_start + length > size) {
        length = size - cycle->data_start;
    }
    
    memcpy(output, gcr + cycle->data_start, length);
    return length;
}

/* ============================================================================
 * Track Comparison
 * ============================================================================ */

/**
 * @brief Compare two tracks
 */
size_t gcr_compare_tracks(const uint8_t *track1, size_t len1,
                          const uint8_t *track2, size_t len2,
                          bool same_disk, gcr_compare_result_t *result)
{
    if (!track1 || !track2) {
        if (result) result->byte_diffs = (size_t)-1;
        return (size_t)-1;
    }
    
    gcr_compare_result_t res = {0};
    
    size_t min_len = (len1 < len2) ? len1 : len2;
    size_t max_len = (len1 > len2) ? len1 : len2;
    
    /* Count byte differences */
    for (size_t i = 0; i < min_len; i++) {
        if (track1[i] != track2[i]) {
            res.byte_diffs++;
        }
    }
    
    /* Add length difference */
    res.byte_diffs += (max_len - min_len);
    
    /* Calculate similarity */
    if (max_len > 0) {
        res.similarity = 100.0f * (1.0f - (float)res.byte_diffs / max_len);
        if (res.similarity < 0) res.similarity = 0;
    }
    
    res.same_format = (len1 == len2);
    
    snprintf(res.description, sizeof(res.description),
             "%zu byte differences, %.1f%% similar",
             res.byte_diffs, res.similarity);
    
    if (result) *result = res;
    return res.byte_diffs;
}

/**
 * @brief Compare sectors between two tracks
 */
size_t gcr_compare_sectors(const uint8_t *track1, size_t len1,
                           const uint8_t *track2, size_t len2,
                           int track, const uint8_t *id1, const uint8_t *id2,
                           gcr_compare_result_t *result)
{
    /* For now, use byte-level comparison */
    /* Full implementation would decode and compare individual sectors */
    return gcr_compare_tracks(track1, len1, track2, len2, false, result);
}

/* ============================================================================
 * Track Verification
 * ============================================================================ */

/* ============================================================================
 * Satzkoepfe und Datenbloecke -- die echte Zerlegung (MF-1013)
 *
 * Aufbau einer 1541-Spur, je Sektor:
 *
 *   Sync (>= 5 x 0xFF)
 *   Kopfblock   10 GCR-Byte -> 8 Klarbyte:
 *               [0] 0x08  [1] Pruefsumme  [2] Sektor  [3] Spur
 *               [4] ID2   [5] ID1         [6] 0x0F    [7] 0x0F
 *   Luecke
 *   Sync
 *   Datenblock  325 GCR-Byte -> 260 Klarbyte:
 *               [0] 0x07  [1..256] Daten  [257] Pruefsumme  [258..259] 0
 *   Luecke
 *
 * Die Pruefsummen sind XOR -- Kopf: Spur ^ Sektor ^ ID1 ^ ID2
 * (`gcr_calc_header_checksum`), Daten: XOR ueber die 256 Byte
 * (`gcr_calc_data_checksum`).
 *
 * **Warum das ohne fremdes Werkzeug beweisbar ist:** die Pruefsummen
 * stehen auf der DISKETTE, nicht in unserem Code. Wird eine echte
 * Aufnahme dekodiert und gehen alle Pruefsummen auf, ist das eine
 * Aussage ueber die Wirklichkeit -- derselbe Weg wie MF-869 beim
 * FM-Pfad ("Die CRCs stehen auf der Diskette selbst, 1979
 * geschrieben"). Ein Rundlauf gegen unser eigenes `gcr_encode` waere
 * dagegen die Selbstbestaetigung aus MF-992.
 * ============================================================================ */

#define GCR_HEADER_GCR_LEN  10      /* 8 Klarbyte   */
#define GCR_DATA_GCR_LEN    325     /* 260 Klarbyte */

/**
 * @brief Dekodiert den Kopfblock an `pos` (hinter einem Sync).
 * @return true, wenn dort ein Kopfblock steht (Marke 0x08).
 */
static bool gcr_read_header(const uint8_t *gcr, size_t size, size_t pos,
                            gcr_sector_verify_t *out, size_t *gcr_fehler)
{
    if (!gcr || !out || pos + GCR_HEADER_GCR_LEN > size) return false;

    uint8_t klar[8];
    size_t fehler = 0;
    if (gcr_decode(gcr + pos, GCR_HEADER_GCR_LEN, klar, &fehler) < 8)
        return false;
    if (klar[0] != 0x08) return false;

    /* Erst ab hier steht fest, dass hier wirklich ein Kopfblock liegt --
     * vorher waeren die Dekodierfehler die eines Blocks, den es nicht
     * gibt (jeder Sync wird zunaechst als Kopf probiert). */
    if (gcr_fehler) *gcr_fehler += fehler;

    out->sector_in_header = klar[2];
    out->track_in_header  = klar[3];
    out->id[0] = klar[5];               /* ID1 */
    out->id[1] = klar[4];               /* ID2 */
    out->header_checksum = klar[1];
    out->sector = klar[2];

    uint8_t soll = gcr_calc_header_checksum(klar[3], klar[2], out->id);
    out->header_ok = (soll == klar[1]) && (fehler == 0);
    return true;
}

/**
 * @brief Dekodiert den Datenblock an `pos` (hinter einem Sync).
 * @param daten Ziel fuer 256 Byte (darf NULL sein)
 * @return true, wenn dort ein Datenblock steht (Marke 0x07).
 */
static bool gcr_read_data(const uint8_t *gcr, size_t size, size_t pos,
                          uint8_t *daten, gcr_sector_verify_t *out,
                          size_t *gcr_fehler)
{
    if (!gcr || !out || pos + GCR_DATA_GCR_LEN > size) return false;

    uint8_t klar[260];
    size_t fehler = 0;
    if (gcr_decode(gcr + pos, GCR_DATA_GCR_LEN, klar, &fehler) < 260)
        return false;
    if (klar[0] != 0x07) return false;

    if (gcr_fehler) *gcr_fehler += fehler;

    out->data_checksum = klar[257];
    uint8_t soll = gcr_calc_data_checksum(klar + 1);
    out->data_ok = (soll == klar[257]) && (fehler == 0);

    if (daten) memcpy(daten, klar + 1, SECTOR_SIZE);
    return true;
}

/**
 * @brief Sucht den naechsten Blockanfang ab `von`.
 * @return Versatz hinter dem Sync, oder `size` wenn keiner mehr kommt.
 */
static size_t gcr_naechster_block(const uint8_t *gcr, size_t size, size_t von)
{
    int sync = gcr_find_sync(gcr, size, von);
    if (sync < 0) return size;
    return gcr_find_sync_end(gcr, size, (size_t)sync);
}

/**
 * @brief Verify track data
 *
 * MF-1013: hier stand `header_ok = true;` und `sectors_good++`
 * **bedingungslos**, sobald die obere Nibble wie `0x50` aussah -- kein
 * Kopf dekodiert, keine Pruefsumme. Und der Parameter `disk_id` wurde
 * **nie benutzt**: eine Signatur, die eine Pruefung zusagte, die nicht
 * stattfand (P3-319).
 *
 * Jetzt wird jeder Kopf- und Datenblock wirklich dekodiert und gegen
 * die Pruefsummen auf der Diskette gehalten. Ist `disk_id` gesetzt,
 * wird die ID aus dem Kopf damit verglichen und eine Abweichung als
 * `header_errors` gezaehlt -- die Pruefung, die der Parameter schon
 * immer versprochen hat.
 *
 * @return Zahl der gefundenen Fehler (Kopf + Daten + GCR).
 */
int gcr_verify_track(const uint8_t *gcr, size_t size, int track,
                     const uint8_t *disk_id, gcr_verify_result_t *result)
{
    if (!gcr || !result || size == 0) return -1;

    memset(result, 0, sizeof(gcr_verify_result_t));

    size_t gcr_fehler = 0;      /* Dekodierfehler in echten Bloecken */
    size_t pos = gcr_naechster_block(gcr, size, 0);
    while (pos < size && result->sectors_found < 21) {
        gcr_sector_verify_t kopf;
        memset(&kopf, 0, sizeof(kopf));

        if (gcr_read_header(gcr, size, pos, &kopf, &gcr_fehler)) {
            /* Spurnummer: der Aufrufer sagt, welche er erwartet. Eine
             * Abweichung ist ein Kopffehler, kein Grund zu schweigen. */
            if (track > 0 && kopf.track_in_header != (uint8_t)track)
                kopf.header_ok = false;

            /* Der ID-Vergleich, den der Parameter zusagt. */
            if (disk_id
                && (kopf.id[0] != disk_id[0] || kopf.id[1] != disk_id[1]))
                kopf.header_ok = false;

            size_t dpos = gcr_naechster_block(gcr, size,
                                              pos + GCR_HEADER_GCR_LEN);
            if (dpos < size)
                (void)gcr_read_data(gcr, size, dpos, NULL, &kopf,
                                    &gcr_fehler);

            int idx = result->sectors_found++;
            result->sectors[idx] = kopf;
            if (!kopf.header_ok) result->header_errors++;
            if (!kopf.data_ok)   result->data_errors++;
            if (kopf.header_ok && kopf.data_ok) result->sectors_good++;

            if (dpos < size) {
                size_t weiter = gcr_naechster_block(gcr, size,
                                                    dpos + GCR_DATA_GCR_LEN);
                if (weiter > pos) { pos = weiter; continue; }
            }
        }

        size_t weiter = gcr_naechster_block(gcr, size, pos + 1);
        if (weiter <= pos) break;
        pos = weiter;
    }

    /* MF-1013: hier stand `result->gcr_errors = gcr_check_errors(gcr,
     * size)`. Das ist eine sinnlose Zahl an dieser Stelle:
     * `gcr_check_errors()` prueft JEDEN Byteversatz auf gueltige
     * 5-Bit-Codes, und in einem byteweise ausgerichteten Strom sind die
     * meisten Versaetze schon von der Bauart her ungueltig. Gemessen an
     * Spur 18 der Korpus-Aufnahme (7142 Byte, 19 fehlerfreie Sektoren)
     * kamen so **5212 „Fehler"** heraus (gemessen ueber `gcr_verify_track` selbst,
     * nicht ueber eine Wegwerf-Schleife) — bei einer Spur, deren
     * saemtliche Pruefsummen aufgehen.
     *
     * Gezaehlt werden jetzt die Dekodierfehler, die in den WIRKLICHEN
     * Bloecken auftreten -- `gcr_read_header`/`gcr_read_data` addieren
     * sie in `gcr_fehler`, und zwar erst, nachdem die Blockmarke (0x08
     * bzw. 0x07) den Block als solchen bestaetigt hat. Eine Zahl, die
     * niemand deuten kann, ist keine Messung. */
    result->gcr_errors = (int)gcr_fehler;
    return result->header_errors + result->data_errors + result->gcr_errors;
}

/**
 * @brief Extract sector data
 *
 * MF-1013: hier stand ein `memset(output, 0, SECTOR_SIZE)` mit
 * `return 0` und dem Kommentar "This is a simplified implementation".
 * Der Aufrufer bekam 256 Nullbytes **und Erfolg** -- erfundene Daten,
 * nicht bloss fehlende. Gefuehrt als P3-319; Klasse MF-864
 * (`flux_decode_fm()`, dessen Sektorteil aus zwei Kommentaren bestand),
 * hier schaerfer, weil Daten geliefert statt weggelassen wurden.
 *
 * Jetzt: die Blockkette ablaufen, den Kopf mit der gesuchten
 * Sektornummer finden, den folgenden Datenblock dekodieren, beide
 * Pruefsummen gegen die Werte auf der DISKETTE halten.
 *
 * @return 0 wenn der Sektor gefunden wurde UND beide Pruefsummen
 *         aufgehen; -1 wenn er nicht gefunden wurde; -2 wenn er
 *         gefunden wurde, aber eine Pruefsumme nicht stimmt. Im Fall
 *         -2 stehen die Daten trotzdem in `output` -- "Kein Bit
 *         verloren" -- und `verify` sagt, welche Pruefsumme fiel.
 */
int gcr_extract_sector(const uint8_t *gcr, size_t size, int sector,
                       uint8_t *output, gcr_sector_verify_t *verify)
{
    if (!gcr || !output || size == 0) return -1;

    gcr_sector_verify_t eigen;
    gcr_sector_verify_t *v = verify ? verify : &eigen;
    memset(v, 0, sizeof(*v));
    v->sector = sector;

    size_t pos = gcr_naechster_block(gcr, size, 0);
    while (pos < size) {
        gcr_sector_verify_t kopf;
        memset(&kopf, 0, sizeof(kopf));

        if (gcr_read_header(gcr, size, pos, &kopf, NULL)
            && kopf.sector_in_header == (uint8_t)sector) {
            size_t dpos = gcr_naechster_block(gcr, size,
                                              pos + GCR_HEADER_GCR_LEN);
            if (dpos < size) {
                *v = kopf;
                if (gcr_read_data(gcr, size, dpos, output, v, NULL)) {
                    return (v->header_ok && v->data_ok) ? 0 : -2;
                }
            }
            /* Kopf gefunden, Daten nicht -- weitersuchen: die Spur ist
             * ein Ring und kann den Sektor ein zweites Mal tragen. */
        }

        size_t weiter = gcr_naechster_block(gcr, size, pos + 1);
        if (weiter <= pos) break;
        pos = weiter;
    }

    return -1;
}

/* ============================================================================
 * Track Utilities
 * ============================================================================ */

/**
 * @brief Check if track is empty
 */
bool gcr_is_empty_track(const uint8_t *gcr, size_t size)
{
    if (!gcr || size == 0) return true;
    
    /* Check for mostly zeros or mostly same byte */
    int zero_count = 0;
    int same_count = 0;
    uint8_t first = gcr[0];
    
    for (size_t i = 0; i < size; i++) {
        if (gcr[i] == 0) zero_count++;
        if (gcr[i] == first) same_count++;
    }
    
    return (zero_count > (int)size * 90 / 100) || 
           (same_count > (int)size * 95 / 100 && first != GCR_SYNC_BYTE);
}

/**
 * @brief Check if track is killer track
 */
bool gcr_is_killer_track(const uint8_t *gcr, size_t size)
{
    if (!gcr || size == 0) return false;
    
    int sync_count = 0;
    for (size_t i = 0; i < size; i++) {
        if (gcr[i] == GCR_SYNC_BYTE) sync_count++;
    }
    
    return (sync_count > (int)size * 90 / 100);
}

/**
 * @brief Detect track density
 */
int gcr_detect_density(const uint8_t *gcr, size_t size)
{
    if (!gcr || size == 0) return 3;
    
    /* Estimate based on track size */
    if (size < 6400) return 0;
    if (size < 6900) return 1;
    if (size < 7400) return 2;
    return 3;
}

/**
 * @brief Get expected capacity
 */
size_t gcr_expected_capacity(int track)
{
    if (track < 1 || track > 42) return 0;
    
    if (track <= 17) return track_capacity[3];
    if (track <= 24) return track_capacity[2];
    if (track <= 30) return track_capacity[1];
    return track_capacity[0];
}

/**
 * @brief Get sectors per track
 */
int gcr_sectors_per_track(int track)
{
    if (track < 1 || track > 42) return 0;
    return sector_map[track];
}

/**
 * @brief Calculate data checksum
 */
uint8_t gcr_calc_data_checksum(const uint8_t *data)
{
    if (!data) return 0;
    
    uint8_t checksum = 0;
    for (int i = 0; i < SECTOR_SIZE; i++) {
        checksum ^= data[i];
    }
    return checksum;
}

/**
 * @brief Calculate header checksum
 */
uint8_t gcr_calc_header_checksum(uint8_t track, uint8_t sector, const uint8_t *id)
{
    uint8_t checksum = track ^ sector;
    if (id) {
        checksum ^= id[0] ^ id[1];
    }
    return checksum;
}

/* ============================================================================
 * CRC Functions
 * ============================================================================ */

/** CRC32 lookup table */
static uint32_t crc32_table[256];
static bool crc32_table_init = false;

static void init_crc32_table(void)
{
    if (crc32_table_init) return;
    
    for (int i = 0; i < 256; i++) {
        uint32_t crc = i;
        for (int j = 0; j < 8; j++) {
            crc = (crc >> 1) ^ ((crc & 1) ? 0xEDB88320 : 0);
        }
        crc32_table[i] = crc;
    }
    crc32_table_init = true;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t size)
{
    init_crc32_table();
    
    crc = ~crc;
    for (size_t i = 0; i < size; i++) {
        crc = crc32_table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return ~crc;
}

/**
 * @brief Calculate CRC32 of track
 */
uint32_t gcr_crc_track(const uint8_t *gcr, size_t size, int track)
{
    if (!gcr || size == 0) return 0;
    
    /* For now, CRC the raw GCR data */
    /* Full implementation would decode sectors first */
    return crc32_update(0, gcr, size);
}

/**
 * @brief Calculate CRC32 of directory
 */
uint32_t gcr_crc_directory(const uint8_t *gcr, size_t size)
{
    return gcr_crc_track(gcr, size, 18);
}
