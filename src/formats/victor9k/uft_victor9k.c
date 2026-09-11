/**
 * @file uft_victor9k.c
 * @brief Victor 9000 / Sirius 1 — zonierte GCR-Geometrie
 *
 * Victor 9000 (Sirius 1) benutzt GCR mit **neun** Geschwindigkeitszonen
 * und hat damit eine variable Sektorzahl je Spur. 80 Spuren je Seite,
 * 512 Byte je Sektor, 1 oder 2 Seiten.
 *
 * ── Referenz ────────────────────────────────────────────────────────
 *
 * MAME `src/lib/formats/victor9k_dsk.cpp` / `.h`, **BSD-3-Clause**,
 * Copyright Curt Coder. Gelesen, nicht uebernommen — die Zonentafeln
 * unten sind Messwerte der Hardware, und die Vorlage nennt sie
 * woertlich:
 *
 *   `sectors_per_track[2][80]` (Z. 392-414) — ZWEI Tafeln, eine je Kopf
 *   `formats[]` (Z. 378-386)   — SSDD 1224 / DSDD 2391 Sektoren
 *   `find_size()` (Z. 136-149) — genau `sector_count * 512` Byte
 *   `build_sector_description()` (Z. 280-289) — `sector_id = i`
 *
 * Bestaetigt durch MAMEs eigene Konsistenz: `formats[]` fuehrt DSDD mit
 * **2391** Sektoren, und 1224 + 1167 = 2391 — die Summen der beiden
 * Kopftafeln ergeben genau die Formatangabe.
 *
 * ── Was hier bis MF-1026 stand, und was es bedeutete ────────────────
 *
 * Der Kopf beschrieb ACHT Zonen mit einer Tafel fuer BEIDE Koepfe und
 * `DS = 1253376`. Fuenf Folgen, gemessen:
 *
 *   1. **Jede echte zweiseitige Datei wurde abgewiesen.** Kopf 1 hat
 *      1167 statt 1224 Sektoren; richtig sind **1224192** Byte, nicht
 *      1253376 — eine Differenz von 57 Sektoren. `vic9k_open()`
 *      antwortete `UFT_ERROR_FORMAT_INVALID` (Klasse MF-1015).
 *   2. **Kopf 1 wurde mit der Tafel von Kopf 0 gelesen:** 57 von 80
 *      Spuren mit falscher Sektorzahl, **79 von 80** am falschen
 *      Versatz, bei Spur 79 um 28672 Byte.
 *   3. **Auf Kopf 0 lagen zwei Zonengrenzen um eins daneben** (Spur 48:
 *      15 statt 14, Spur 70: 12 statt 13). Da sich +1 und -1
 *      aufheben, blieb die Summe 1224 — die Groessenpruefung konnte es
 *      nie bemerken, und **22 Spuren (49..70) wurden 512 Byte zu hoch
 *      gelesen**. Spur 48 gab einen 15. Sektor aus, der Spur 49
 *      gehoert; Spur 70 lieferte ihren 13. Sektor nie.
 *   4. **Die Sektornummern waren 1..n statt 0..n-1**, weil
 *      `uft_format_add_sector()` laut eigenem Kopf einen 0-basierten
 *      INDEX nimmt und 1 addiert. MAME setzt `sector_id = i`. Gestalt
 *      von MF-1016 (`jv1`).
 *   5. **Ein Zylinder ausserhalb 0..79 kam als `UFT_OK` mit null
 *      Sektoren zurueck** — `vic9k_spt()` gab 0, die Schleife lief
 *      nicht, der Aufrufer bekam Erfolg fuer eine Spur, die es nicht
 *      gibt.
 *
 * Der alte Kopf nannte FluxEngine und MAME als Referenz, und
 * `.spec_status` stand auf `UFT_SPEC_REVERSE_ENGINEERED`. Beides
 * zusammen ist die Lage, vor der die EINFRIER-REGEL warnt: eine
 * Referenz war benannt, aber ihre Zahlen standen nicht im Code.
 *
 * Regressionsschutz: `tests/test_victor9k_gegen_mame.c`.
 */
#include "uft/uft_format_common.h"

#define VIC9K_TRACKS    80
#define VIC9K_SS        512
#define VIC9K_SIDE0_SECTORS 1224   /* Summe vic9k_spt[0][] */
#define VIC9K_SIDE1_SECTORS 1167   /* Summe vic9k_spt[1][] — NICHT 1224 */
#define VIC9K_SS_SIZE   626688     /* 1224 * 512, MAME formats[0] */
#define VIC9K_DS_SIZE   1224192    /* 2391 * 512, MAME formats[1] */

/* Sektoren je Spur, EINE Tafel JE KOPF.
 *
 * Woertlich aus MAME `victor9k_dsk.cpp:392-414`. Die beiden Tafeln sind
 * verschieden — das ist der Kern von MF-1026 —, und die Zonengrenzen
 * liegen nicht dort, wo der alte Kopf sie beschrieb: auf Kopf 0 wechselt
 * 15 -> 14 nach Spur **47** (nicht 48) und 13 -> 12 nach Spur **70**
 * (nicht 69). */
static const uint8_t vic9k_spt_tafel[2][VIC9K_TRACKS] = {
    {   /* Kopf 0 — Summe 1224 */
        19,19,19,19,                             /*  0.. 3 */
        18,18,18,18,18,18,18,18,18,18,18,18,     /*  4..15 */
        17,17,17,17,17,17,17,17,17,17,17,        /* 16..26 */
        16,16,16,16,16,16,16,16,16,16,16,        /* 27..37 */
        15,15,15,15,15,15,15,15,15,15,           /* 38..47 */
        14,14,14,14,14,14,14,14,14,14,14,14,     /* 48..59 */
        13,13,13,13,13,13,13,13,13,13,13,        /* 60..70 */
        12,12,12,12,12,12,12,12,12               /* 71..79 */
    },
    {   /* Kopf 1 — Summe 1167, eine Zone versetzt */
        18,18,18,18,18,18,18,18,                 /*  0.. 7 */
        17,17,17,17,17,17,17,17,17,17,17,        /*  8..18 */
        16,16,16,16,16,16,16,16,16,16,16,        /* 19..29 */
        15,15,15,15,15,15,15,15,15,15,           /* 30..39 */
        14,14,14,14,14,14,14,14,14,14,14,14,     /* 40..51 */
        13,13,13,13,13,13,13,13,13,13,13,        /* 52..62 */
        12,12,12,12,12,12,12,12,12,12,12,12,     /* 63..74 */
        11,11,11,11,11                           /* 75..79 */
    }
};

/** Sektoren auf (head, track); 0 heisst „gibt es nicht". */
static int vic9k_spt(int head, int track) {
    if (head < 0 || head > 1) return 0;
    if (track < 0 || track >= VIC9K_TRACKS) return 0;
    return vic9k_spt_tafel[head][track];
}

/** Byteversatz des ersten Sektors von (cyl, head).
 *
 * Nach MAME `get_image_offset()`: Kopf 1 beginnt hinter der GANZEN
 * Seite 0, danach summiert jede Seite mit ihrer EIGENEN Tafel. */
static long vic9k_track_offset(int cyl, int head) {
    long off = 0;
    if (head == 1)
        off += (long)VIC9K_SIDE0_SECTORS * VIC9K_SS;
    for (int t = 0; t < cyl; t++)
        off += (long)vic9k_spt(head, t) * VIC9K_SS;
    return off;
}

typedef struct {
    FILE    *file;
    uint8_t  heads;
} vic9k_pd_t;

/* ---- probe ---- */
static bool vic9k_probe(const uint8_t *d, size_t s, size_t fs, int *c) {
    (void)d; (void)s;
    if (fs == VIC9K_SS_SIZE || fs == VIC9K_DS_SIZE) {
        *c = 70;
        return true;
    }
    return false;
}

/* ---- open ---- */
static uft_error_t vic9k_open(uft_disk_t *disk, const char *path, bool ro) {
    FILE *f = fopen(path, ro ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long sz = ftell(f);
    if (sz < 0) { fclose(f); return UFT_ERROR_IO; }
    if (fseek(f, 0, SEEK_SET) != 0) { fclose(f); return UFT_ERROR_IO; }

    uint8_t heads;
    if (sz == VIC9K_SS_SIZE)      heads = 1;
    else if (sz == VIC9K_DS_SIZE) heads = 2;
    else { fclose(f); return UFT_ERROR_FORMAT_INVALID; }

    vic9k_pd_t *p = calloc(1, sizeof(vic9k_pd_t));
    if (!p) { fclose(f); return UFT_ERROR_NO_MEMORY; }
    p->file = f;
    p->heads = heads;

    disk->plugin_data = p;
    disk->geometry.cylinders = VIC9K_TRACKS;
    disk->geometry.heads     = heads;
    disk->geometry.sectors   = 19;           /* groesste Zone, Kopf 0 */
    disk->geometry.sector_size = VIC9K_SS;
    /* MF-1026: NICHT `1224 * heads`. Kopf 1 hat 1167 Sektoren, und
     * MAMEs Formattafel fuehrt DSDD mit genau 2391. */
    disk->geometry.total_sectors =
        (uint32_t)VIC9K_SIDE0_SECTORS
        + (heads == 2 ? (uint32_t)VIC9K_SIDE1_SECTORS : 0u);
    return UFT_OK;
}

/* ---- close ---- */
static void vic9k_close(uft_disk_t *disk) {
    vic9k_pd_t *p = disk->plugin_data;
    if (p) { if (p->file) fclose(p->file); free(p); disk->plugin_data = NULL; }
}

/* ---- read_track ---- */
static uft_error_t vic9k_read_track(uft_disk_t *disk, int cyl, int head,
                                     uft_track_t *track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. Eine Pruefung, die nur nach
     * oben schaut (`if (cyl >= tracks)`), laesst -1 durch — und
     * `track_data[-1]` ist ein Zugriff vor dem Feld. Gefunden an
     * opus_read_track() von tests/test_disk_open_fuzz.c. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    vic9k_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;

    /* MF-1026: die OBERE Schranke fehlte, und das war kein
     * Schoenheitsfehler. `vic9k_spt()` gab fuer einen Zylinder
     * ausserhalb 0..79 die Zahl 0, die Sektorschleife lief null Mal,
     * und der Aufrufer bekam `UFT_OK` mit einer leeren Spur — Erfolg
     * fuer eine Spur, die es nicht gibt. Dasselbe fuer Kopf 1 einer
     * einseitigen Datei: der Versatz lag hinter dem Dateiende, jeder
     * `fread` war kurz, und heraus kamen 0xE5-Fuellsektoren. Dank
     * MF-980 waren die als fehlend gekennzeichnet — aber „diese Spur
     * ist beschaedigt" ist eine andere Aussage als „diese Seite
     * existiert nicht". */
    if (cyl >= VIC9K_TRACKS || head >= p->heads)
        return UFT_ERROR_INVALID_PARAM;

    uft_track_init(track, cyl, head);

    int spt = vic9k_spt(head, cyl);
    long off = vic9k_track_offset(cyl, head);
    uint8_t buf[VIC9K_SS];

    for (int s = 0; s < spt; s++) {
        if (fseek(p->file, off + (long)s * VIC9K_SS, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        /* MF-980: der kurze Lesevorgang wird GEMERKT, nicht nur
         * gefuellt. `uft_format_add_sector*()` legt jeden Sektor mit
         * `status = UFT_SECTOR_OK` und „CRC gueltig" an — die Fuellung
         * war damit von echten Daten nicht zu unterscheiden. Die Bytes
         * bleiben stehen, sie gelten nur nicht mehr als Messwert. */
        const bool kurz = (fread(buf, 1, VIC9K_SS, p->file) != VIC9K_SS);
        if (kurz) memset(buf, 0xE5, VIC9K_SS);
        /* MF-1026: `uft_format_add_sector()` nimmt laut eigenem Kopf
         * einen 0-basierten INDEX und addiert 1 — der Kommentar dort
         * nennt Apple, Amiga und Commodore als Faelle, fuer die das
         * falsch ist. Victor 9000 gehoert dazu: MAME setzt
         * `sectors[i].sector_id = i`, also 0..n-1. Gestalt von
         * MF-1016 (`jv1`). */
        uft_format_add_sector_with_id(track, (uint8_t)s, buf, VIC9K_SS,
                                      (uint8_t)cyl, (uint8_t)head);
        if (kurz) uft_format_mark_last_missing(track);
    }
    return UFT_OK;
}

/* ---- write_track ---- */
static uft_error_t vic9k_write_track(uft_disk_t *disk, int cyl, int head,
                                      const uft_track_t *track) {
    /* MF-529: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. MF-519 hat das fuer
     * read_track getan und write_track uebersehen. Das ASan-Tor
     * der CI fand die Folge an d80_write_track: die Schranke
     * `cyl >= D80_TRACKS` laesst -1 durch, und d80_spt[-1] liest
     * vor der Tabelle.
     *
     * Beim SCHREIBEN wiegt das schwerer als beim Lesen: ein
     * falscher Index liefert nicht nur falsche Daten, er bestimmt,
     * WOHIN geschrieben wird. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    vic9k_pd_t *p = disk->plugin_data;
    if (!p || !p->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;

    /* MF-1026, und beim Schreiben wiegt es schwerer als beim Lesen:
     * ohne obere Schranke bestimmte ein falscher Zylinder, WOHIN
     * geschrieben wird — dieselbe Erwaegung, die MF-529 hier schon
     * einmal fuer die untere Schranke angestellt hat. */
    if (cyl >= VIC9K_TRACKS || head >= p->heads)
        return UFT_ERROR_INVALID_PARAM;

    int spt = vic9k_spt(head, cyl);
    long off = vic9k_track_offset(cyl, head);

    for (size_t s = 0; s < track->sector_count && (int)s < spt; s++) {
        if (fseek(p->file, off + (long)s * VIC9K_SS, SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[VIC9K_SS];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, VIC9K_SS);
            data = pad;
        }
        if (fwrite(data, 1, VIC9K_SS, p->file) != VIC9K_SS)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

/* ---- plugin descriptor ---- */
static const uft_plugin_feature_t uft_format_plugin_victor9k_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_victor9k = {
    .name = "Victor9K",
    .description = "Victor 9000 / Sirius 1 GCR",
    .extensions = "vic;v9k",
    .format = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe       = vic9k_probe,
    .open        = vic9k_open,
    .close       = vic9k_close,
    .read_track  = vic9k_read_track,
    .write_track = vic9k_write_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_victor9k_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_victor9k_features) / sizeof(uft_format_plugin_victor9k_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(victor9k)
