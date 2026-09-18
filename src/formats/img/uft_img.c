/**
 * @file uft_img.c
 * @brief UnifiedFloppyTool - IMG/IMA Format Plugin
 * 
 * Generic PC disk image format:
 * - Flat file mit Sektoren in CHS-Reihenfolge
 * - Unterstützt 160KB bis 2.88MB
 * - Sector interleave: S0-S8 (oder S0-S17 etc.) pro Track
 * 
 * @author UFT Team
 * @date 2025
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_format_probe.h"    /* MF-1231: uft_format_variant_t */
#include "uft/uft_format_common.h"   /* UFT_MAX_SPT */
#include "uft/uft_log.h"             /* honest padding warning (MF-465) */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// IMG Constants
// ============================================================================

#define IMG_SECTOR_SIZE  512

// Bekannte Größen
typedef struct {
    size_t size;
    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors;
    const char* name;
} img_geometry_entry_t;

/* ── Die Aufzaehlung steht EINMAL (MF-1231) ──────────────────────────
 *
 * Bis MF-1231 stand sie nur hier, als `known_geometries[]`, und die
 * Oberflaeche hielt daneben eine zweite Liste: `m_formatInfo["IMG"]` in
 * `src/formattab.cpp` nannte fuenf Groessen — 360K, 720K, 1.2M, 1.44M,
 * 2.88M. Gemessen fuehrt diese Tafel **dreizehn**; 160K, 180K, 320K,
 * die DMF-Groesse und die vier ED-Spezialformate fehlten dort, und
 * `DMF` stand in der Oberflaeche sogar als eigenes „Format" ohne
 * Plugin, obwohl es die Zeile mit 1 720 320 Byte hier ist.
 *
 * Zwei Listen derselben Sache driften — das ist in diesem Baum
 * mehrfach belegt (MF-1015 drei Pruefsummen, MF-1026 drei
 * Victor-Geometrien, MF-1177 der Spurhaushalt an drei Stellen). Die
 * Zeilen stehen deshalb jetzt als X-Makro und werden in **beide**
 * Gestalten ausgefaltet: in die Geometrietafel, die die Sonde befragt,
 * und in die Variantentafel, die die Oberflaeche beim Speichern
 * anzeigt. Eine neue Groesse einzutragen heisst weiterhin: EINE Zeile.
 *
 * Der Kurzname ist das, was im Auswahlfeld steht; der lange Text ist
 * die Beschreibung, die es schon gab.
 *
 * MF-872: die ED-Spezialformate mit 41-44 Sektoren je Spur.
 *
 * Referenz, zwei unabhaengige Quellen:
 *   FLOPFIX `version.txt:40` — „Sind die Spezialformate aktiviert,
 *     so sind nun auch die ED-Formate mit 41, 42, 43 und 44
 *     Sektoren zugaenglich."
 *   FreeDOS FORMAT 0.92 (GPL-2-only, nur Zahlen entnommen) fuehrt
 *     mit FD3360/FD3486 bis 42 SpT — deckt die untere Haelfte
 *     unabhaengig ab.
 *
 * Die Groessen sind gerechnet, nicht abgeschrieben:
 *   SpT x 2 Seiten x 80 Spuren x 512 Byte.
 * Die Rechnung reproduziert die bestehende Zeile darueber
 * (36 SpT -> 2949120) exakt; das ist die Begruendung fuer die neuen.
 *
 * Gemessen, bevor sie eingetragen wurden (MF-784: Groessengleichheit
 * ist keine Geometriegleichheit): keine der vier Zahlen kommt sonst
 * irgendwo im Baum vor, und der generische Rueckfall unten faengt
 * sie nicht — `sectors_options[]` kennt 41-44 nicht, und bei 8 oder
 * 10 SpT lieferte er Zylinderzahlen weit ueber 84.
 *
 * Was sie VORHER beanspruchte: bei einem echten PC-Bootsektor
 * meldete allein `DMK` diese vier Groessen, mit Konfidenz 55 — ein
 * TRS-80-Format fuer ein PC-Abbild. Nachher meldet `IMG` 90. */
#define IMG_GEOMETRIEN(X)                                                   \
    X(163840,   40, 1,  8, "160K",      "160KB 5.25\" SS/DD")               \
    X(184320,   40, 1,  9, "180K",      "180KB 5.25\" SS/DD")               \
    X(327680,   40, 2,  8, "320K",      "320KB 5.25\" DS/DD")               \
    X(368640,   40, 2,  9, "360K",      "360KB 5.25\" DS/DD")               \
    X(737280,   80, 2,  9, "720K",      "720KB 3.5\" DS/DD")                \
    X(1228800,  80, 2, 15, "1.2M",      "1.2MB 5.25\" DS/HD")               \
    X(1474560,  80, 2, 18, "1.44M",     "1.44MB 3.5\" DS/HD")               \
    X(1720320,  80, 2, 21, "1.68M DMF", "1.68MB 3.5\" DMF")                 \
    X(2949120,  80, 2, 36, "2.88M",     "2.88MB 3.5\" DS/ED")               \
    X(3358720,  80, 2, 41, "ED 41",     "3.28MB 3.5\" ED (41 SpT)")         \
    X(3440640,  80, 2, 42, "ED 42",     "3.36MB 3.5\" ED (42 SpT)")         \
    X(3522560,  80, 2, 43, "ED 43",     "3.44MB 3.5\" ED (43 SpT)")         \
    X(3604480,  80, 2, 44, "ED 44",     "3.52MB 3.5\" ED (44 SpT)")

static const img_geometry_entry_t known_geometries[] = {
#define IMG_X_GEO(sz, c, h, s, kurz, text) { sz, c, h, s, text },
    IMG_GEOMETRIEN(IMG_X_GEO)
#undef IMG_X_GEO
    { 0, 0, 0, 0, NULL }
};

/* Die Variantentafel aus denselben Zeilen.
 *
 * `can_write` ist fuer alle dreizehn wahr, und das ist gemessen, nicht
 * angenommen: `img_write_track()` rechnet seinen Versatz aus der beim
 * Oeffnen ermittelten Geometrie und kennt keine Sonderbehandlung je
 * Groesse — was gelesen werden kann, kann auch geschrieben werden.
 *
 * Die Schreibvorgabe ist **1.44M**, weil sie die einzige Groesse ist,
 * die jedes heutige 3,5-Zoll-Laufwerk schreiben kann; die vier
 * ED-Spezialformate brauchen einen Controller, der 41-44 Sektoren je
 * Spur formatiert. Genau eine Zeile traegt die Vorgabe, und
 * `tests/test_varianten_aus_dem_plugin.c` haelt das fest. */
static const uft_format_variant_t img_variants[] = {
#define IMG_X_VAR(sz, c, h, s, kurz, text)                                  \
    { .name = kurz, .description = text,                                    \
      .base_format = UFT_FORMAT_IMG,                                        \
      .min_size = (sz), .max_size = (sz), .exact_sizes = { (sz) },          \
      .cylinders = (c), .heads = (h),                                       \
      .sectors_min = (s), .sectors_max = (s),                               \
      .sector_size = IMG_SECTOR_SIZE,                                       \
      .validate = NULL,                                                     \
      .can_read = true, .can_write = true, .write_note = NULL,              \
      .is_write_default = ((sz) == 1474560) },
    IMG_GEOMETRIEN(IMG_X_VAR)
#undef IMG_X_VAR
};

// ============================================================================
// Plugin Data
// ============================================================================

typedef struct {
    FILE*    file;
    size_t   file_size;
} img_data_t;

// ============================================================================
// Geometry Detection
// ============================================================================

static bool img_detect_geometry(size_t file_size, uft_geometry_t* geo) {
    // Exakte Matches
    for (int i = 0; known_geometries[i].size != 0; i++) {
        if (file_size == known_geometries[i].size) {
            geo->cylinders = known_geometries[i].cylinders;
            geo->heads = known_geometries[i].heads;
            geo->sectors = known_geometries[i].sectors;
            geo->sector_size = IMG_SECTOR_SIZE;
            geo->total_sectors = geo->cylinders * geo->heads * geo->sectors;
            geo->double_step = false;
            return true;
        }
    }
    
    // Generisch: Versuche Geometrie zu erraten
    if (file_size % IMG_SECTOR_SIZE != 0) {
        return false;  // Muss durch Sektorgröße teilbar sein
    }
    
    size_t total_sectors = file_size / IMG_SECTOR_SIZE;
    
    // Typische Werte probieren
    static const int sectors_options[] = { 18, 9, 15, 36, 21, 8, 10 };
    static const int heads_options[] = { 2, 1 };
    
    for (int h = 0; h < 2; h++) {
        for (int s = 0; s < 7; s++) {
            int heads = heads_options[h];
            int sectors = sectors_options[s];
            
            if (total_sectors % (heads * sectors) == 0) {
                int cylinders = total_sectors / (heads * sectors);
                if (cylinders >= 35 && cylinders <= 84) {
                    geo->cylinders = cylinders;
                    geo->heads = heads;
                    geo->sectors = sectors;
                    geo->sector_size = IMG_SECTOR_SIZE;
                    geo->total_sectors = total_sectors;
                    geo->double_step = (cylinders == 40);
                    return true;
                }
            }
        }
    }
    
    return false;
}

// ============================================================================
// Probe
// ============================================================================

bool img_probe(const uint8_t* data, size_t size, size_t file_size, 
                      int* confidence) {
    *confidence = 0;
    
    uft_geometry_t geo;
    if (!img_detect_geometry(file_size, &geo)) {
        return false;
    }
    
    *confidence = 40;  // Größe passt — Band „nur die Groesse" (MF-729)

    /* ── MF-1144: hier wurden die Stufen ZUGEWIESEN, nicht gesammelt ──
     *
     * Vorher stand eine Kette aus vier `if`, von denen jedes
     * `*confidence` ueberschrieb — in dieser Reihenfolge:
     *
     *     if (Sprungbefehl)      *confidence = 60;
     *     if (Bootsignatur)      *confidence = 80;
     *     if (has_oem)           *confidence = 85;   <-- greift IMMER
     *     if (bps == 512)        *confidence = 90;
     *
     * `has_oem` prueft allein, ob die Bytes 3..10 DRUCKBARES ASCII sind.
     * Damit ueberschrieb die schwaechste Erkenntnis die staerkste: eine
     * Datei OHNE Sprungbefehl und OHNE Bootsignatur bekam **85** — das
     * Band „Merkmal getroffen" (80..100) — weil acht Byte Text darin
     * standen.
     *
     * **Gemessen war die Folge, dass IMG die Abbilder fremder Formate
     * gewinnt.** Ueber den echten `uft_disk_open()` verloren **11 von
     * 12** kopflosen Formaten ihr EIGENES Abbild an IMG bzw. DSK_X820,
     * und IMG meldete dabei eine andere Teilung mit derselben Summe:
     *
     *     204 800 Byte  ssd       : 80 x 1 x 10 x 256 -> IMG 50 x 1 x 8 x 512
     *     655 360 Byte  trd       : 80 x 2 x 16 x 256 -> IMG 80 x 2 x 8 x 512
     *     409 600 Byte  nanowasp  : 40 x 2 x 10 x 512 -> IMG 50 x 2 x 8 x 512
     *     315 392 Byte  micropolis: 77 x 1 x 16 x 256 -> IMG 77 x 1 x 8 x 512
     *
     * Der einzige Fall, in dem das Format sich selbst durchsetzte, war
     * 89 600 Byte — und dort sagt IMGs Sonde `nein`. Das ist die
     * Ursache, nicht ein Zufall.
     *
     * **Warum keine Eichung es fing:** Eichung 1 (MF-729) fuettert
     * NULLEN — Bytes 3..10 sind dann 0x00 und nicht druckbar. Eichung 2
     * fuettert Zufall, und ihre Regel gilt ihrem eigenen Kopf nach fuer
     * Band **50..79**; 85 liegt darueber. Das Band 80..100 ist von
     * keiner Eichung gedeckt, und reiner TEXT kommt in keiner vor —
     * dieselbe Gestalt wie MF-1000: ein Tor, das schmaler ist als sein
     * Gegenstand, meldet zuverlaessig null.
     *
     * Jetzt ist die **Bootsignatur das Tor zum Merkmalsband**, und das
     * ist die Sache selbst: `0x55AA` bei 510/511 ist die Signatur eines
     * PC-Bootsektors. Alles andere sind Strukturmerkmale und heben
     * innerhalb ihres Bandes.
     *
     * Ein echtes FAT12-Abbild verliert dadurch nichts: es hat
     * Sprungbefehl, Signatur, druckbares OEM-Feld und `bps == 512` und
     * kommt damit auf **89**. Ein roher Sektorabzug ohne Bootsektor
     * bleibt bei 40..60 und tritt der spezifischen Sonde den Vortritt
     * ab — genau die Rangfolge, die MF-729 gewollt hat. */
    if (size >= 512) {
        int struktur = 0;

        /* Sprungbefehl am Anfang eines Bootsektors */
        if (data[0] == 0xEB || data[0] == 0xE9) struktur++;

        /* OEM-Feld (Bytes 3..10) druckbar — ein Hinweis, kein Merkmal:
         * acht druckbare Byte hat auch jede Textdatei. */
        bool has_oem = true;
        for (int i = 3; i < 11; i++) {
            if (data[i] < 0x20 || data[i] > 0x7E) { has_oem = false; break; }
        }
        if (has_oem) struktur++;

        /* Sektorgroesse im BPB */
        uint16_t bps = (uint16_t)(data[11] | (data[12] << 8));
        if (bps == 512) struktur++;

        const bool bootsig = (data[510] == 0x55 && data[511] == 0xAA);

        if (bootsig) {
            /* Merkmal getroffen; die Strukturmerkmale heben innerhalb
             * des Bandes. 3 x 3 = 9, also hoechstens 89. */
            *confidence = 80 + struktur * 3;
        } else if (struktur >= 2) {
            *confidence = 60;   /* Struktur gelesen, ohne Signatur */
        }
        /* EIN Strukturmerkmal allein hebt NICHT, und das ist gemessen:
         * die erste Fassung dieser Leiter gab dafuer 50, und damit
         * gewann IMG weiter gegen `trd` (45) und `nanowasp` (40) — ein
         * druckbares OEM-Feld ist keine „gelesene Struktur", es ist ein
         * Hinweis. Es bleibt bei 40, „nur die Groesse". */
    }

    return *confidence > 0;
}

// ============================================================================
// Open
// ============================================================================

static uft_error_t img_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) {
        return UFT_ERROR_FILE_OPEN;
    }
    
    // Größe ermitteln - H1 FIX: ftell() Fehlerprüfung
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    long pos = ftell(f);
    if (pos < 0) {
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    size_t file_size = (size_t)pos;
    
    if (fseek(f, 0, SEEK_SET) != 0) {
        fclose(f);
        return UFT_ERROR_FILE_SEEK;
    }
    
    // Geometrie ermitteln
    if (!img_detect_geometry(file_size, &disk->geometry)) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }
    
    // Plugin-Daten
    img_data_t* pdata = calloc(1, sizeof(img_data_t));
    if (!pdata) {
        fclose(f);
        return UFT_ERROR_NO_MEMORY;
    }
    
    pdata->file = f;
    pdata->file_size = file_size;
    disk->plugin_data = pdata;
    
    return UFT_OK;
}

// ============================================================================
// Close
// ============================================================================

static void img_close(uft_disk_t* disk) {
    if (!disk || !disk->plugin_data) return;
    
    img_data_t* pdata = disk->plugin_data;
    if (pdata->file) {
        fclose(pdata->file);
    }
    
    free(pdata);
    disk->plugin_data = NULL;
}

// ============================================================================
// Create
// ============================================================================

static uft_error_t img_create(uft_disk_t* disk, const char* path,
                               const uft_geometry_t* geometry) {
    if (geometry->sector_size != 512) {
        return UFT_ERROR_INVALID_ARG;
    }
    
    FILE* f = fopen(path, "wb");
    if (!f) {
        return UFT_ERROR_FILE_OPEN;
    }
    
    // Mit Nullen füllen
    size_t total_size = (size_t)geometry->cylinders * geometry->heads * 
                        geometry->sectors * geometry->sector_size;
    
    uint8_t zero[512] = {0};
    size_t sectors = total_size / 512;
    
    for (size_t i = 0; i < sectors; i++) {
        if (fwrite(zero, 512, 1, f) != 1) {
            fclose(f);
            return UFT_ERROR_FILE_WRITE;
        }
    }
    
    fclose(f);
    
    return img_open(disk, path, false);
}

// ============================================================================
// Flush
// ============================================================================

static uft_error_t img_flush(uft_disk_t* disk) {
    if (!disk || !disk->plugin_data) return UFT_ERROR_NULL_POINTER;
    
    img_data_t* pdata = disk->plugin_data;
    if (pdata->file) {
        fflush(pdata->file);
    }
    
    return UFT_OK;
}

// ============================================================================
// Read Track
// ============================================================================

static uft_error_t img_read_track(uft_disk_t* disk, int cylinder, int head,
                                   uft_track_t* track) {
    if (!disk || !track) return UFT_ERROR_NULL_POINTER;
    
    img_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_FILE_READ;
    
    uft_track_init(track, cylinder, head);
    
    // Bounds-Check (H7 FIX)
    if (cylinder < 0 || cylinder >= (int)disk->geometry.cylinders ||
        head < 0 || head >= (int)disk->geometry.heads) {
        return UFT_ERROR_OUT_OF_RANGE;
    }
    
    // PC Layout: CHS in Reihenfolge (C0H0S1-Sn, C0H1S1-Sn, C1H0S1-Sn, ...)
    // Sektoren sind 1-basiert in der ID!
    // K2 FIX: size_t casts für Overflow-Schutz
    size_t track_offset = ((size_t)cylinder * (size_t)disk->geometry.heads + (size_t)head) * 
                          (size_t)disk->geometry.sectors * (size_t)IMG_SECTOR_SIZE;
    
    if (fseek(pdata->file, track_offset, SEEK_SET) != 0) {
        return UFT_ERROR_FILE_SEEK;
    }
    
    for (int s = 0; s < disk->geometry.sectors; s++) {
        uft_sector_t sector = {0};
        
        // PC-Sektoren sind 1-basiert!
        sector.id.cylinder = cylinder;
        sector.id.head = head;
        sector.id.sector = s + 1;  // 1-basiert
        sector.id.size_code = 2;   // 512 Bytes
        sector.id.crc_ok = true;
        
        sector.data = malloc(IMG_SECTOR_SIZE);
        if (!sector.data) {
            return UFT_ERROR_NO_MEMORY;
        }
        
        if (fread(sector.data, IMG_SECTOR_SIZE, 1, pdata->file) != 1) {
            free(sector.data);
            return UFT_ERROR_FILE_READ;
        }
        
        sector.data_size = IMG_SECTOR_SIZE;
        sector.data_len  = IMG_SECTOR_SIZE;  /* mirror to the modern field so
                                              * consumers that check data_len
                                              * (post-MF-321 standard) see the
                                              * data — else a cross-format write
                                              * reads it as empty (no-op). */
        sector.status = UFT_SECTOR_OK;
        
        uft_error_t err = uft_track_add_sector(track, &sector);
        free(sector.data);
        
        if (UFT_FAILED(err)) {
            return err;
        }
    }
    
    track->status = UFT_TRACK_OK;
    
    return UFT_OK;
}

// ============================================================================
// Write Track
// ============================================================================

static uft_error_t img_write_track(uft_disk_t* disk, int cylinder, int head,
                                    const uft_track_t* track) {
    if (!disk || !track) return UFT_ERROR_NULL_POINTER;
    if (disk->read_only) return UFT_ERROR_DISK_PROTECTED;
    
    img_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_FILE_WRITE;
    
    // Bounds-Check (H7 FIX)
    if (cylinder < 0 || cylinder >= (int)disk->geometry.cylinders ||
        head < 0 || head >= (int)disk->geometry.heads) {
        return UFT_ERROR_OUT_OF_RANGE;
    }
    
    // K2 FIX: size_t casts
    size_t track_offset = ((size_t)cylinder * (size_t)disk->geometry.heads + (size_t)head) * 
                          (size_t)disk->geometry.sectors * (size_t)IMG_SECTOR_SIZE;
    
    if (fseek(pdata->file, track_offset, SEEK_SET) != 0) {
        return UFT_ERROR_FILE_SEEK;
    }
    
    /* Sektoren in aufsteigender Reihenfolge ihrer Nummer AUF DER DISKETTE.
     *
     * Vorher wurden sie als 1..N gesucht (`uft_track_find_sector(track, s)`)
     * und jeder Fehlgriff still mit Nullen aufgefüllt. Das setzt voraus, dass
     * die Quelle so nummeriert wie eine IBM-PC-Diskette. Eine Quelle mit
     * anderer Basis — Apple 0..15, Amiga 0..10, Commodore 0..N-1 (ARCH-20) —
     * verlor damit ihren ersten Sektor und bekam einen Null-Sektor dazu,
     * ohne dass irgendwo etwas davon stand. Dieselbe Falle greift bei jeder
     * Quelle mit Lücken oder Versatz in der Nummerierung, etwa einer IMD mit
     * Sektor-Map (MF-465). */
    static const uint8_t zeros[IMG_SECTOR_SIZE] = {0};

    const uft_sector_t *order[UFT_MAX_SPT];
    size_t n = 0;
    for (size_t i = 0; i < track->sector_count && n < UFT_MAX_SPT; i++) {
        const uft_sector_t *s = &track->sectors[i];
        size_t j = n++;
        while (j > 0 && order[j - 1]->id.sector > s->id.sector) {
            order[j] = order[j - 1];
            j--;
        }
        order[j] = s;
    }

    if (n < (size_t)disk->geometry.sectors) {
        UFT_WARN("IMG: Spur %d/%d liefert %zu von %u Sektoren — der Rest wird "
                 "als Nullen geschrieben und ist KEIN gelesener Inhalt",
                 cylinder, head, n, (unsigned)disk->geometry.sectors);
    }

    for (size_t s = 0; s < (size_t)disk->geometry.sectors; s++) {
        const uft_sector_t* sector = (s < n) ? order[s] : NULL;

        if (sector && sector->data &&
            (sector->data_len >= IMG_SECTOR_SIZE ||
             sector->data_size >= IMG_SECTOR_SIZE)) {
            if (fwrite(sector->data, IMG_SECTOR_SIZE, 1, pdata->file) != 1) {
                return UFT_ERROR_FILE_WRITE;
            }
        } else {
            // M3 FIX: Verwende static buffer
            if (fwrite(zeros, IMG_SECTOR_SIZE, 1, pdata->file) != 1) {
                return UFT_ERROR_FILE_WRITE;
            }
        }
    }

    return UFT_OK;
}

// ============================================================================
// Metadata
// ============================================================================

static uft_error_t img_read_metadata(uft_disk_t* disk, const char* key, 
                                      char* value, size_t max_len) {
    if (!disk || !key || !value) return UFT_ERROR_NULL_POINTER;
    
    img_data_t* pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_FILE_READ;
    
    if (strcmp(key, "volume_name") == 0) {
        // FAT Volume Label lesen (Offset 43 im Bootsektor, oder Root-Dir)
        uint8_t boot[512];
        
        if (fseek(pdata->file, 0, SEEK_SET) != 0) {
            return UFT_ERROR_FILE_SEEK;
        }
        
        if (fread(boot, 512, 1, pdata->file) != 1) {
            return UFT_ERROR_FILE_READ;
        }
        
        // Volume Label bei Offset 43 (11 Bytes, space-padded)
        if (boot[38] == 0x29) {  // Extended boot signature
            char label[12];
            memcpy(label, &boot[43], 11);
            label[11] = '\0';
            
            // Trailing spaces entfernen
            for (int i = 10; i >= 0 && label[i] == ' '; i--) {
                label[i] = '\0';
            }
            
            // K3 FIX: snprintf statt strncpy für garantierte Null-Terminierung
            snprintf(value, max_len, "%s", label);
            return UFT_OK;
        }
        
        snprintf(value, max_len, "NO NAME");
        return UFT_OK;
    }
    
    if (strcmp(key, "filesystem") == 0) {
        uint8_t boot[128];
        
        if (fseek(pdata->file, 0, SEEK_SET) != 0) {
            return UFT_ERROR_FILE_SEEK;
        }
        
        if (fread(boot, 128, 1, pdata->file) != 1) {
            return UFT_ERROR_FILE_READ;
        }
        
        // FAT12/16 Type bei Offset 54 oder 82
        // K3 FIX: snprintf statt strncpy
        if (memcmp(&boot[54], "FAT12", 5) == 0) {
            snprintf(value, max_len, "FAT12");
        } else if (memcmp(&boot[54], "FAT16", 5) == 0) {
            snprintf(value, max_len, "FAT16");
        } else if (memcmp(&boot[82], "FAT32", 5) == 0) {
            snprintf(value, max_len, "FAT32");
        } else {
            snprintf(value, max_len, "Unknown");
        }
        
        return UFT_OK;
    }
    
    if (strcmp(key, "oem_name") == 0) {
        uint8_t boot[16];
        
        if (fseek(pdata->file, 0, SEEK_SET) != 0) {
            return UFT_ERROR_FILE_SEEK;
        }
        
        if (fread(boot, 16, 1, pdata->file) != 1) {
            return UFT_ERROR_FILE_READ;
        }
        
        char oem[9];
        memcpy(oem, &boot[3], 8);
        oem[8] = '\0';
        // K3 FIX: snprintf
        snprintf(value, max_len, "%s", oem);
        
        return UFT_OK;
    }
    
    return UFT_ERROR_NOT_SUPPORTED;
}

// ============================================================================
// Plugin Definition
// ============================================================================

static const uft_plugin_feature_t uft_format_plugin_img_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_SUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_img = {
    .name = "IMG",
    .description = "Generic PC Disk Image",
    .extensions = "img;ima;dsk;vfd;flp",
    .version = 0x00010000,
    .format = UFT_FORMAT_IMG,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_CREATE,
    
    .probe = img_probe,
    .open = img_open,
    .close = img_close,
    .create = img_create,
    .flush = img_flush,
    .read_track = img_read_track,
    .write_track = img_write_track,
    .detect_geometry = NULL,
    /* MF-1231: dieselben Zeilen wie `known_geometries[]`, siehe oben. */
    .variants = img_variants,
    .variant_count = sizeof(img_variants) / sizeof(img_variants[0]),
    .read_metadata = img_read_metadata,
    .write_metadata = NULL,
    
    .init = NULL,
    .shutdown = NULL,
    .private_data = NULL,
    .spec_status = UFT_SPEC_DERIVED,  /* Raw sector dump; no formal spec but universal de-facto PC standard */
    .features = uft_format_plugin_img_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_img_features) / sizeof(uft_format_plugin_img_features[0]),
};
