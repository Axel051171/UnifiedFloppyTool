/**
 * @file uft_nib.c
 * @brief Apple II NIB nibble format core
 * @version 3.8.0
 */
#include "uft/uft_format_common.h"
#include "uft/formats/apple/uft_apple_gcr.h"

#define NIB_TRACKS 35
#define NIB_TRACK_SIZE 6656
#define NIB_FILE_SIZE 232960

/* MF-948: hier stand eine EIGENE 64-Byte-Umsetzungstabelle.
 *
 * Sie war die dritte Kopie derselben Abbildung im Baum (P3-234, sieben
 * Fundstellen). Byteidentisch mit den anderen — der Schaden lag nicht
 * in der Tabelle, sondern in der Rechnung daneben. Beides ist jetzt
 * `src/formats/apple/uft_apple_gcr.c`: eine benannte Referenz
 * (`Beneath Apple DOS`, Kap. 3) und eine Messung gegen das Oracle
 * `to_woz2` (560 von 560 Sektoren byteidentisch, MF-715). */


typedef struct { uint8_t* data; } nib_data_t;

bool nib_probe(const uint8_t* data, size_t size, size_t file_size, int* confidence) {
    if (file_size != NIB_FILE_SIZE) return false;
    *confidence = 45;  /* MF-729: nur die Groesse */

    /* NIB: 35 tracks × 6656 bytes. Each track has Apple II GCR sync
     * bytes (0xFF runs) and address field markers (D5 AA 96). */
    if (size >= 6656) {
        int ff_count = 0, d5_count = 0;
        for (size_t i = 0; i < 6656; i++) {
            if (data[i] == 0xFF) ff_count++;
            if (i + 2 < 6656 && data[i] == 0xD5 && data[i+1] == 0xAA && data[i+2] == 0x96)
                d5_count++;
        }
        /* Good NIB track has ~1000+ sync bytes and ~16 address fields */
        if (d5_count >= 10 && ff_count > 500) *confidence = 95;
        else if (ff_count > 200) *confidence = 88;
    }
    return true;
}

static uft_error_t nib_open(uft_disk_t* disk, const char* path, bool read_only) {
    FILE* f = fopen(path, "rb");
    if (!f) return UFT_ERR_FILE_OPEN;
    
    nib_data_t* p = calloc(1, sizeof(nib_data_t));
    if (!p) { fclose(f); return UFT_ERR_MEMORY; }
    p->data = malloc(NIB_FILE_SIZE);
    if (!p->data) { free(p); fclose(f); return UFT_ERR_MEMORY; }
    if (fread(p->data, 1, NIB_FILE_SIZE, f) != NIB_FILE_SIZE) { free(p->data); free(p); fclose(f); return UFT_ERR_IO; }
    fclose(f);
    
    disk->plugin_data = p;
    disk->geometry.cylinders = NIB_TRACKS;
    disk->geometry.heads = 1;
    disk->geometry.sectors = 16;
    disk->geometry.sector_size = 256;
    disk->geometry.total_sectors = (uint32_t)disk->geometry.cylinders * 16;
    return UFT_OK;
}

static void nib_close(uft_disk_t* disk) {
    nib_data_t* p = disk->plugin_data;
    if (p) { free(p->data); free(p); disk->plugin_data = NULL; }
}

/* MF-948: liest ueber `uft_apple_gcr_scan_track()`.
 *
 * Hier standen `find_addr`, `find_data` und `decode_sector` — eine
 * private Zweitfassung des Apple-GCR-Abtasters neben der
 * oracle-geprueften Einheit. Drei Fehler auf einmal, alle STILL:
 *
 *  1. Die zwei niederwertigen Bits jedes Bytes kamen VERTAUSCHT heraus.
 *     Apple-6-and-2 legt jede Zweiergruppe bitverdreht ins Hilfsbyte;
 *     die Rueckvertauschung fehlte. Gemessen an 64 Sektoren:
 *     **8192 von 16384 Bytes falsch — genau 50 %**, waehrend die
 *     gepruefte Einheit 0 von 16384 falsch hatte.
 *
 *     Und die Pruefsumme merkt davon NICHTS: sie laeuft ueber den
 *     Nibble-Strom, also VOR dem Zusammensetzen. Alle 64 Sektoren
 *     wurden mit „Pruefsumme ok" gemeldet. Das ist stille Veraenderung
 *     im Wortsinn (DESIGN_PRINCIPLES).
 *
 *     Der Baum WUSSTE es: `src/flux/uft_flux_decoder.c:1591` traegt die
 *     Warnung woertlich — „Undo it, or every data byte's low 2 bits
 *     come out swapped." Zwei von drei Umsetzungen hatten sie, die
 *     dritte nicht. Genau der Preis, den P3-234 an der Doppelung
 *     benennt.
 *
 *  2. Kein Umlauf. Eine Apple-Spur ist ein RING; `find_addr` suchte nur
 *     bis `i + 14 < len`. Ein Sektor auf dem Umbruch war verloren.
 *
 *  3. Eine Blindzone: `while (pos < NIB_TRACK_SIZE - 400)` brach die
 *     Suche 400 Bytes vor dem Spurende ab.
 *
 * Alle drei sind mit der Verdrahtung weg — die gepruefte Einheit liest
 * als Ring und kennt die Bitverdrehung. Ein NIB-Spurpuffer IST ein
 * MSB-zuerst gepackter Bitstrom (jedes Diskettenbyte acht Bits), also
 * passt er ohne Umformung in ihren Vertrag. */
static uft_error_t nib_read_track(uft_disk_t* disk, int cyl, int head, uft_track_t* track) {
    /* MF-519: negative Koordinaten abweisen, BEVOR mit ihnen
     * gerechnet oder indiziert wird. */
    if (cyl < 0 || head < 0) return UFT_ERROR_INVALID_PARAM;

    nib_data_t* p = disk->plugin_data;
    if (!p || !p->data || head != 0 || cyl >= NIB_TRACKS) return UFT_ERR_INVALID_STATE;

    uft_track_init(track, cyl, head);
    const uint8_t* tdata = p->data + (size_t)cyl * NIB_TRACK_SIZE;

    /* Platz fuer mehr als eine Formatierung: 16 Sektoren sind der
     * Normalfall, 13 der DOS-3.2-Fall; doppelt so viel faengt eine
     * Spur mit Zusatzfeldern ab, ohne dass etwas still wegfaellt. */
    uft_a2_sector_t sek[UFT_A2_SECTORS_16 * 2];
    int n = uft_apple_gcr_scan_track(tdata, (uint32_t)NIB_TRACK_SIZE * 8u,
                                     sek, sizeof(sek) / sizeof(sek[0]));
    if (n < 0) return UFT_ERROR_INVALID_PARAM;

    for (int i = 0; i < n; i++) {
        if (sek[i].track != (uint8_t)cyl) continue;

        /* Ein Feld in einer Kodierung, die die Einheit nicht beherrscht
         * (DOS-3.2-Bootsektor, MF-721), traegt KEINE Bytes. Es wird
         * uebergangen statt geraten — der alte Weg haette hier Inhalt
         * geliefert, der nirgends auf der Diskette steht. */
        if (!sek[i].has_data || sek[i].alt_encoding) continue;

        /* `sec` stammt aus dem Adressfeld — es IST die Nummer auf der
         * Diskette und darf nicht verschoben werden (ARCH-20). */
        uft_format_add_sector_with_id(track, sek[i].sector, sek[i].data,
                                      UFT_A2_SECTOR_SIZE, cyl, head);
        if (!sek[i].data_checksum_ok && track->sector_count > 0)
            uft_sector_set_crc(&track->sectors[track->sector_count - 1], false);
    }
    return UFT_OK;
}

static const uft_plugin_feature_t uft_format_plugin_nib_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_nib = {
    /* MF-635: die Beschreibung sagt jetzt ausdruecklich, fuer WELCHE Maschine
     * dieses Plugin da ist. `.nib` ist ein Name mit zwei Bedeutungen — Apple II
     * Nibble (hier) und Commodore MNIB (nicht gelesen). Die Probe unten
     * verlangt exakt NIB_FILE_SIZE = 232960 Byte; eine Commodore-NIB-Datei
     * faellt durch und wurde bis MF-635 in der Oberflaeche trotzdem unter
     * "Commodore 64/128" angeboten. Ein Name, zwei Bedeutungen, eine falsche
     * Zusage — dieselbe Klasse wie MF-559. */
    .name = "NIB", .description = "Apple II Nibble (nicht Commodore MNIB)",
    .extensions = "nib",
    .format = UFT_FORMAT_NIB, .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_VERIFY,
    .probe = nib_probe, .open = nib_open, .close = nib_close, .read_track = nib_read_track,
    .verify_track = uft_generic_verify_track,
    .spec_status = UFT_SPEC_REVERSE_ENGINEERED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_nib_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_nib_features) / sizeof(uft_format_plugin_nib_features[0]),
};
UFT_REGISTER_FORMAT_PLUGIN(nib)
