/**
 * @file victor9k.c
 * @brief Victor 9000 / Sirius 1 disk format implementation
 *
 * ── WARNUNG: die Geometrie unten ist FALSCH (MF-1026) ───────────────
 *
 * Diese Datei hat **keinen Aufrufer** — sie steht in
 * `docs/orphan_baseline.txt:212` und gehoert zur `FloppyDevice`-Schicht
 * (75 Dateien ohne Tuer). Sie wird deshalb nicht repariert, sondern
 * beschriftet: **erst der Ersatz, dann die Loeschung** (MF-699).
 *
 * `victor9k_get_sectors_for_track()` gibt fuer die Spuren 38..79 flach
 * **15** Sektoren zurueck. Das ergibt 1285 Sektoren = 657920 Byte — eine
 * Groesse, die **keine** Victor-Diskette hat. Gemessen gegen MAME
 * `formats/victor9k_dsk.cpp` (BSD-3-Clause, Curt Coder):
 *
 *   Kopf 0: 19/18/17/16/15/14/13/12 mit Grenzen bei 3/15/26/37/47/59/70
 *           — Summe **1224** Sektoren = 626688 Byte
 *   Kopf 1: 18/17/16/15/14/13/12/11 mit Grenzen bei 7/18/29/39/51/62/74
 *           — Summe **1167** Sektoren
 *   zusammen 2391 Sektoren = **1224192** Byte (MAMEs `formats[]` nennt
 *           genau diese 2391)
 *
 * Damit lagen im Baum **drei** verschiedene Victor-Geometrien. Die
 * gepruefte und benutzte steht in `src/formats/victor9k/uft_victor9k.c`,
 * festgenagelt von `tests/test_victor9k_gegen_mame.c`. Wer diese Datei
 * je verdrahtet, nimmt die Tafeln von dort — nicht die hier.
 */

#include "uft/formats/victor9k.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

int victor9k_get_sectors_for_track(int track) {
    if (track < 4) return 19;
    if (track < 16) return 18;
    if (track < 27) return 17;
    if (track < 38) return 16;
    return 15;  // Tracks 38-79
}

static int get_track_offset(int track) {
    int offset = 0;
    for (int t = 0; t < track; t++) {
        offset += victor9k_get_sectors_for_track(t) * 512;
    }
    return offset;
}

int victor9k_probe(const uint8_t *data, size_t size) {
    // Victor 9000: 80 tracks with variable sectors
    // Total: ~800KB per side
    // Calculate expected size
    int expected = 0;
    for (int t = 0; t < 80; t++) {
        expected += victor9k_get_sectors_for_track(t) * 512;
    }
    
    if (size == (size_t)expected) return 85;
    if (size == (size_t)(expected * 2)) return 85;  // Double-sided
    return 0;
}

int victor9k_open(Victor9kDevice *dev, const char *path) {
    if (!dev || !path) return -1;
    
    FILE *f = fopen(path, "rb");
    if (!f) return -1;
    
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return -1; }
    size_t size = ftell(f);
    fclose(f);

    int expected_ss = 0;
    for (int t = 0; t < 80; t++) {
        expected_ss += victor9k_get_sectors_for_track(t) * 512;
    }
    
    dev->tracks = 80;
    dev->heads = (size == (size_t)(expected_ss * 2)) ? 2 : 1;
    dev->flux_supported = true;
    dev->internal_ctx = strdup(path);
    
    return 0;
}

int victor9k_close(Victor9kDevice *dev) {
    if (dev && dev->internal_ctx) {
        free(dev->internal_ctx);
        dev->internal_ctx = NULL;
    }
    return 0;
}

int victor9k_read_sector(Victor9kDevice *dev, uint32_t t, uint32_t h, uint32_t s, uint8_t *buf) {
    if (!dev || !buf || !dev->internal_ctx) return -1;
    if (t >= dev->tracks || h >= dev->heads) return -1;
    
    int max_sectors = victor9k_get_sectors_for_track(t);
    if (s >= (uint32_t)max_sectors) return -1;
    
    FILE *f = fopen((const char*)dev->internal_ctx, "rb");
    if (!f) return -1;
    
    // Calculate offset
    int side_size = 0;
    for (int tr = 0; tr < 80; tr++) {
        side_size += victor9k_get_sectors_for_track(tr) * 512;
    }
    
    size_t offset = (h * side_size) + get_track_offset(t) + (s * 512);
    if (fseek(f, offset, SEEK_SET) != 0) { fclose(f); return -1; }
    size_t read = fread(buf, 1, 512, f);
    fclose(f);
    
    return (read == 512) ? 0 : -1;
}
