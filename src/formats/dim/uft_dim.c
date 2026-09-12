/**
 * @file uft_dim.c
 * @brief DIM Disk Image Plugin (Sharp X68000)
 *
 * DIM is the standard disk image format for the Sharp X68000 series.
 * It consists of a 256-byte header followed by raw sector data.
 *
 * Header layout:
 *   Offset  Size  Description
 *   0x00    1     Media type (0x00=2HD, 0x01=2HS, 0x02=2HC, 0x09=2HQ, ...)
 *   0x01-AA       Reserved/track flags
 *   0xAB   13     "DIFC HEADER  " — die Kennung des Formats
 *   0xB8-FF       Reserved
 *
 * ── Befund 1 (MF-1019): die Kennung wurde nie geprueft ────────────────
 *
 * Hier stand „0xAB 1 Overtrack flag", und weder `dim_probe()` noch
 * `dim_open()` sahen dort hin. Bei 0xAB steht aber die **Kennung des
 * Formats**, und darin sind sich drei Stellen einig:
 *
 *   MAMEs `dim_format::identify()`  liest 16 Byte ab **0xAB** und
 *     vergleicht `strncmp(h, "DIFC HEADER", 11)`
 *   `src/formats/pc98/dim.c:37`    `/* 0xAB..0xB7 = "DIFC HEADER  "
 *     (13 bytes) *\/` und prueft es
 *   `src/formats/misc/dcp_dcu.c:51` nennt dieselbe Kennung
 *
 * Ohne diese Pruefung nahm die Sonde **jede** Datei an, deren erstes
 * Byte einer von sieben Medienwerten ist und die gross genug ist —
 * beim Medienwert 0x00 also jede hinreichend grosse Datei, die mit
 * einer Null beginnt.
 *
 * **Die richtige Pruefung lag in der Datei, die niemand ruft** —
 * `src/formats/pc98/dim.c` steht in `docs/orphan_baseline.txt`. Das ist
 * in dieser Runde der dritte Fall dieser Gestalt nach `udi` (MF-1015,
 * Pruefsumme) und `scl` (MF-1014, TR-DOS-Layout).
 *
 * ── Befund 2 (MF-1019): `open` prueft die Dateigroesse nicht ──────────
 *
 * `dim_probe()` verlangt `file_size >= 256 + cyl*heads*spt*ss`,
 * `dim_open()` verlangt nichts davon. Wer eine DIM ueber `open`
 * anfasst, bekam fuer eine zu kurze Datei die volle angesagte
 * Geometrie und Sektoren, die nicht in der Datei stehen.
 *
 * ── AUFGELOEST (MF-1037): die Medientabelle, und warum sie strittig war
 *
 * MF-1019 liess sie offen, weil drei Umsetzungen drei verschiedene
 * Tabellen hatten (gefuehrt als P3-325). **Der Grund dafuer ist jetzt
 * gemessen: zwei der drei sind Tabellen fuer ZWEI VERSCHIEDENE
 * Formate.**
 *
 * Die vierte Quelle ist die Formatbeschreibung im Kopf von hxcfes
 * `libhxcfe/sources/loaders/dim_x68k_loader/dim_x68k_format.h` —
 * **Dokumentation**, Kanal *Spec* nach MF-695: gelesen wird die
 * Beschreibung, keine Zeile Code. Sie nennt genau **vier**
 * DIM-Medienbytes, mit Kapazitaet:
 *
 *     DIM    DCP  Format  Geometrie
 *     0x00 = 0x02 (2HS)   ( 8 sec/trk 1232k)
 *     0x01 = 0x02 (2HS)   ( 9 sec/trk 1440k)
 *     0x02 = 0x01 (2HC)   (15 sec/trk 1200k) [80/2/15/512]
 *     0x03 = 0x09 (2HQ)   (18 sec/trk 1440k) IBM 1.44MB 2HD format
 *
 * — und listet die **DCP**-Medienbytes getrennt daneben, darunter
 * `0x11 = 2HD-BASIC` und `0x19 = 2DD-BASIC`. Genau diese Werte standen
 * in UFTs DIM-Tabelle. **Sie gehoeren einer anderen Nummerierung**, und
 * damit ist erklaert, warum die drei Tabellen nie zusammenpassten.
 *
 * **Und das ist nicht nur gelesen, sondern gemessen.** hxcfes
 * `X68000_DIM`-Loader wurde mit je einer Pruefdatei befragt (Kopf mit
 * gueltigem `"DIFC HEADER  "`, 2 MB Nutzlast), Sektorzahl aus
 * `hxcfe -infos`:
 *
 *     Medienbyte   hxcfe meldet
 *     0x00         1232 Sektoren  (= 77 x 2 x  8)
 *     0x01         1440 Sektoren  (= 80 x 2 x  9)
 *     0x02         2400 Sektoren  (= 80 x 2 x 15)
 *     0x03         2880 Sektoren  (= 80 x 2 x 18)
 *     0x04 0x05 0x08 0x09 0x11 0x19 0x21 0xFF
 *                  "No loader support the file" — ABGEWIESEN
 *
 * Damit steht die Tabelle:
 *
 *     0x00  77 x 2 x  8 x 1024 = 1 261 568  (1232k, "2HS")
 *     0x01  80 x 2 x  9 x 1024 = 1 474 560  (1440k, "2HS")
 *     0x02  80 x 2 x 15 x  512 = 1 228 800  (1200k, "2HC")
 *     0x03  80 x 2 x 18 x  512 = 1 474 560  (1440k, "2HQ", IBM 1.44MB)
 *
 * **Was UFT vorher hatte:** 0x00 bis 0x03 **alle** auf 77 x 2 x 8 x
 * 1024. Fuer 0x00 ist das richtig, fuer die drei anderen falsch — und
 * **wie falsch, ist gemessen**, nicht gerechnet. Am unveraenderten
 * Vorzustand, je Medienbyte eine Pruefdatei mit selbstbenennenden
 * Sektoren ("UFT-K Cnn Hh Snn "):
 *
 *     0x00  1 261 824 Byte  probe 88  open   0  77x2x8x1024  RICHTIG
 *     0x01  1 474 816 Byte  probe 88  open   0  77x2x8x1024  FALSCH
 *     0x02  1 229 056 Byte  probe  0  open -25               ABGEWIESEN
 *     0x03  1 474 816 Byte  probe 88  open   0  77x2x8x1024  FALSCH
 *
 * **Zwei von vier wurden also angenommen und falsch zerlegt, mit
 * Konfidenz 88.** Spur (40,1) — die aeusserste Spur der zweiten Seite
 * — lieferte in beiden Faellen den Sektor "UFT-K C36 H0 S01": einen
 * Block von der **anderen Seite** und vier Zylinder daneben. Und weil
 * 77 x 2 x 8 x 1024 nur 1 261 568 der 1 474 560 Nutzbytes abdeckt,
 * fielen **212 992 Byte** still weg.
 *
 * Genau **eine** Datei hat die Groessenpruefung gerettet: 0x02, weil
 * 1 229 056 kleiner ist als die verlangten 1 261 824. Bei 0x01 und 0x03
 * ist die Datei **groesser** als die falsche Rechnung verlangt, und
 * `fs < erwartet` sieht ein Zuviel nicht. Das ist der Grund, warum der
 * Schutzsatz aus MF-1019 — "ist ein Eintrag falsch, passt die Rechnung
 * nicht zur Datei, und sie wird abgewiesen statt falsch zerlegt" — nur
 * in **eine** Richtung hielt. Er stand hier als Ersatz fuer die
 * Klaerung; er war nur die halbe Sicherung.
 *
 * **Und 0x09, 0x11, 0x19 werden jetzt abgewiesen.** Fuer sie als
 * DIM-Medienbytes gibt es keinen Beleg: hxcfes Loader weist sie
 * gemessen ab, und seine Beschreibung fuehrt 0x11/0x19 unter DCP. MAMEs
 * `dim_dsk.cpp` kennt `case 9` (18 spt) und `case 17` (26 spt) — es
 * behandelt die beiden Familien in **einer** Funktion, und genau daraus
 * ist die Vermischung entstanden.
 *
 * **Einmal wird MAME begruendet ueberstimmt, und zwar mit einer
 * Messung.** Fuer 0x03 sagt `dim_dsk.cpp` `spt = 9, size = 3` (also
 * 9 x 1024), hxcfes Beschreibung sagt 18 x 512 — **beide ergeben
 * 1 474 560 Byte**, die Dateigroesse kann es also nicht entscheiden.
 * Entschieden hat es hxcfes **ausgefuehrter** Loader: 2880 Sektoren.
 * Dieselbe Lage wie MF-1015 (`udi`), nur diesmal mit einem Lauf statt
 * einem Argument. Dass MAME fuer 0x01 UND 0x03 dasselbe (9 x 1024)
 * sagt, passt dazu: ein Wert waere dann bedeutungslos.
 *
 * Nebenbei traegt MAME einen zweiten Widerspruch in sich: es setzt
 * `track_total = 77` fuer **jeden** Typ, was fuer 0x01/0x02/0x03 nicht
 * zu den Kapazitaeten seiner eigenen Sektorgroessen passt.
 *
 *
 * Referenzen (EINFRIER-REGEL MF-363/498, Bedingung c):
 *   hxcfe `dim_x68k_loader/dim_x68k_format.h` (GPL-2) — nur der
 *     Kopfkommentar gelesen (Kanal Spec); der Lader AUSGEFUEHRT
 *   MAME `formats/dim_dsk.cpp` (BSD-3-Clause, Olivier Galibert), in
 *     `neue-ideen/formats.zip` — Kennung bei 0xAB, Daten ab 0x100
 *   `src/formats/pc98/dim.c` (eigener Baum) — dieselbe Kennung an
 *     derselben Stelle, 13 Byte lang
 *
 * Vorher stand hier nur „X68000 DIM format specification" ohne
 * Fundstelle — eine Referenz, die man nicht nachlesen kann, ist
 * keine.
 */

#include "uft/uft_format_common.h"

/* ============================================================================
 * Constants
 * ============================================================================ */

#define DIM_HEADER_SIZE     256
#define DIM_SIG_OFF         0xAB
#define DIM_SIG             "DIFC HEADER"
#define DIM_SIG_LEN         11      /* MAME vergleicht 11 Byte */
#define DIM_MAX_SECTOR_SIZE 1024
#define DIM_MAX_CYLINDERS   82
#define DIM_MAX_SPT         18

/* Media type codes */
/* MF-1037: **vier** Medienbytes, und die Namen folgen der
 * Beschreibung. Die frueheren `DIM_MEDIA_2HQ 0x09`,
 * `DIM_MEDIA_2DD_8 0x11` und `DIM_MEDIA_2DD_9 0x19` sind
 * entfallen: das sind DCP-Medienbytes, nicht DIM. */
#define DIM_MEDIA_2HS_8     0x00   /* 1232k,  8 spt x 1024 */
#define DIM_MEDIA_2HS_9     0x01   /* 1440k,  9 spt x 1024 */
#define DIM_MEDIA_2HC       0x02   /* 1200k, 15 spt x  512 */
#define DIM_MEDIA_2HQ       0x03   /* 1440k, 18 spt x  512 */

/* Die Kennung bei 0xAB. MAME vergleicht 11 Byte („DIFC HEADER"),
 * `src/formats/pc98/dim.c` 13 („DIFC HEADER  " mit zwei Leerzeichen).
 * Genommen werden die 11, weil sie die schwaechere Annahme sind: eine
 * Datei mit 11 passenden Byte wird angenommen, auch wenn die beiden
 * Leerzeichen fehlen. */
static bool dim_has_signature(const uint8_t *hdr)
{
    return memcmp(hdr + DIM_SIG_OFF, DIM_SIG, DIM_SIG_LEN) == 0;
}

/* ============================================================================
 * Geometry lookup
 * ============================================================================ */

/**
 * @brief Geometrie aus dem Medienbyte — vier Werte, alle gemessen.
 *
 * Quelle: die Formatbeschreibung in hxcfes `dim_x68k_format.h`
 * (Dokumentation, Kanal *Spec*), abgenommen an hxcfes **ausgefuehrtem**
 * `X68000_DIM`-Loader; die Zahlen und die Messung stehen im Dateikopf.
 *
 * Alles andere wird ABGEWIESEN. Vorher standen hier auch 0x09, 0x11 und
 * 0x19 — das sind **DCP**-Medienbytes, eine andere Nummerierung, und
 * hxcfes Loader weist sie gemessen ab.
 */
static bool dim_get_geometry(uint8_t media, uint8_t *cyl, uint8_t *heads,
                             uint8_t *spt, uint16_t *sector_size)
{
    switch (media) {
        case DIM_MEDIA_2HS_8:   /* 0x00 — 1232k */
            *cyl = 77; *heads = 2; *spt = 8;  *sector_size = 1024;
            return true;
        case DIM_MEDIA_2HS_9:   /* 0x01 — 1440k */
            *cyl = 80; *heads = 2; *spt = 9;  *sector_size = 1024;
            return true;
        case DIM_MEDIA_2HC:     /* 0x02 — 1200k, Geometrie woertlich in
                                 * der Beschreibung: [80/2/15/512] */
            *cyl = 80; *heads = 2; *spt = 15; *sector_size = 512;
            return true;
        case DIM_MEDIA_2HQ:     /* 0x03 — 1440k, IBM-1.44MB-Aufteilung */
            *cyl = 80; *heads = 2; *spt = 18; *sector_size = 512;
            return true;
        default:
            /* MF-1037: keine Geometrie ohne Beleg. Insbesondere 0x09,
             * 0x11 und 0x19 — sie standen hier, gehoeren aber der
             * DCP-Nummerierung, und hxcfes Loader weist sie ab. */
            return false;
    }
}

/* ============================================================================
 * Plugin data
 * ============================================================================ */

typedef struct {
    FILE*       file;
    uint8_t     media_type;
    uint8_t     cylinders;
    uint8_t     heads;
    uint8_t     sectors_per_track;
    uint16_t    sector_size;
} dim_data_t;

/* ============================================================================
 * probe
 * ============================================================================ */

bool dim_probe(const uint8_t *data, size_t size, size_t file_size,
               int *confidence)
{
    if (!data || size < DIM_HEADER_SIZE) return false;

    /* MF-1019, Befund 1: die Kennung bei 0xAB — vorher ungeprueft. */
    if (!dim_has_signature(data)) return false;

    uint8_t media = data[0];
    uint8_t cyl, heads, spt;
    uint16_t ss;

    if (!dim_get_geometry(media, &cyl, &heads, &spt, &ss))
        return false;

    /* Validate file size: header + expected data */
    uint32_t expected = DIM_HEADER_SIZE +
                        (uint32_t)cyl * heads * spt * ss;
    if (file_size < expected)
        return false;

    /* MF-729/MF-1019: mit der Kennung ist das ein getroffenes Merkmal
     * (80..100), nicht mehr „nur die Groesse". Vorher stand hier 45. */
    *confidence = 88;
    return true;
}

/* ============================================================================
 * open
 * ============================================================================ */

static uft_error_t dim_open(uft_disk_t *disk, const char *path,
                             bool read_only)
{
    FILE *f = fopen(path, read_only ? "rb" : "r+b");
    if (!f) return UFT_ERROR_FILE_OPEN;

    uint8_t hdr[DIM_HEADER_SIZE];
    if (fread(hdr, 1, DIM_HEADER_SIZE, f) != DIM_HEADER_SIZE) {
        fclose(f);
        return UFT_ERROR_IO;
    }

    /* MF-1019, Befund 1: dieselbe Kennung wie in der Sonde. */
    if (!dim_has_signature(hdr)) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    uint8_t cyl, heads, spt;
    uint16_t ss;
    if (!dim_get_geometry(hdr[0], &cyl, &heads, &spt, &ss)) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    /* MF-1019, Befund 2: `open` prueft die Dateigroesse jetzt genauso
     * wie die Sonde. Eine zu kurze Datei bekam vorher die volle
     * angesagte Geometrie und Sektoren, die nicht darin stehen.
     *
     * Und das ist gleichzeitig die Sicherung gegen die ungeklaerte
     * Medientabelle (siehe Dateikopf): ist ein Eintrag falsch, passt
     * die Rechnung nicht zur Datei, und sie wird abgewiesen statt
     * falsch zerlegt. */
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return UFT_ERROR_IO; }
    long fs = ftell(f);
    if (fs < 0) { fclose(f); return UFT_ERROR_IO; }
    uint32_t erwartet = DIM_HEADER_SIZE
                      + (uint32_t)cyl * heads * spt * ss;
    if ((uint32_t)fs < erwartet) {
        fclose(f);
        return UFT_ERROR_FORMAT_INVALID;
    }

    dim_data_t *pdata = calloc(1, sizeof(dim_data_t));
    if (!pdata) { fclose(f); return UFT_ERROR_NO_MEMORY; }

    pdata->file = f;
    pdata->media_type = hdr[0];
    pdata->cylinders = cyl;
    pdata->heads = heads;
    pdata->sectors_per_track = spt;
    pdata->sector_size = ss;

    disk->plugin_data = pdata;
    disk->geometry.cylinders = cyl;
    disk->geometry.heads = heads;
    disk->geometry.sectors = spt;
    disk->geometry.sector_size = ss;
    disk->geometry.total_sectors = (uint32_t)cyl * heads * spt;

    return UFT_OK;
}

/* ============================================================================
 * close
 * ============================================================================ */

static void dim_close(uft_disk_t *disk)
{
    dim_data_t *pdata = disk->plugin_data;
    if (pdata) {
        if (pdata->file) fclose(pdata->file);
        free(pdata);
        disk->plugin_data = NULL;
    }
}

/* ============================================================================
 * read_track
 * ============================================================================ */

static uft_error_t dim_read_track(uft_disk_t *disk, int cyl, int head,
                                   uft_track_t *track)
{
    dim_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    if (cyl >= pdata->cylinders || head >= pdata->heads)
        return UFT_ERROR_INVALID_STATE;

    uft_track_init(track, cyl, head);

    /* Data starts after 256-byte header, tracks are sequential */
    uint32_t track_index = (uint32_t)cyl * pdata->heads + (uint32_t)head;
    long offset = DIM_HEADER_SIZE +
                  (long)(track_index * pdata->sectors_per_track *
                         pdata->sector_size);

    if (fseek(pdata->file, offset, SEEK_SET) != 0)
        return UFT_ERROR_IO;

    uint8_t buf[DIM_MAX_SECTOR_SIZE];

    for (int s = 0; s < pdata->sectors_per_track; s++) {
        if (fread(buf, 1, pdata->sector_size, pdata->file) !=
            pdata->sector_size)
            return UFT_ERROR_IO;

        uft_format_add_sector(track, (uint8_t)s, buf,
                              pdata->sector_size,
                              (uint8_t)cyl, (uint8_t)head);
    }

    return UFT_OK;
}

/* ============================================================================
 * write_track
 * ============================================================================ */

static uft_error_t dim_write_track(uft_disk_t *disk, int cyl, int head,
                                    const uft_track_t *track)
{
    dim_data_t *pdata = disk->plugin_data;
    if (!pdata || !pdata->file) return UFT_ERROR_INVALID_STATE;
    if (disk->read_only) return UFT_ERROR_NOT_SUPPORTED;
    if (cyl >= pdata->cylinders || head >= pdata->heads)
        return UFT_ERROR_INVALID_STATE;

    uint32_t track_index = (uint32_t)cyl * pdata->heads + (uint32_t)head;
    long offset = DIM_HEADER_SIZE +
                  (long)(track_index * pdata->sectors_per_track *
                         pdata->sector_size);

    for (size_t s = 0; s < track->sector_count &&
         (int)s < pdata->sectors_per_track; s++) {
        if (fseek(pdata->file, offset + (long)s * pdata->sector_size,
                  SEEK_SET) != 0)
            return UFT_ERROR_IO;
        const uint8_t *data = track->sectors[s].data;
        uint8_t pad[DIM_MAX_SECTOR_SIZE];
        if (!data || track->sectors[s].data_len == 0) {
            memset(pad, 0xE5, pdata->sector_size);
            data = pad;
        }
        if (fwrite(data, 1, pdata->sector_size, pdata->file) !=
            pdata->sector_size)
            return UFT_ERROR_IO;
    }
    return UFT_OK;
}

/* ============================================================================
 * Plugin registration
 * ============================================================================ */

static const uft_plugin_feature_t uft_format_plugin_dim_features[] = {
    { "Read", UFT_FEATURE_SUPPORTED, NULL },
    { "Write", UFT_FEATURE_SUPPORTED, NULL },
    { "Create", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Flux", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Timing", UFT_FEATURE_UNSUPPORTED, NULL },
    { "Weak Bits", UFT_FEATURE_UNSUPPORTED, NULL },
    { "MultiRev", UFT_FEATURE_UNSUPPORTED, NULL },
};

const uft_format_plugin_t uft_format_plugin_dim = {
    .name         = "DIM",
    .description  = "Sharp X68000 Disk Image",
    .extensions   = "dim;xdf",
    .version      = 0x00010000,
    .format       = UFT_FORMAT_DSK,
    .capabilities = UFT_FORMAT_CAP_READ | UFT_FORMAT_CAP_WRITE | UFT_FORMAT_CAP_VERIFY,
    .probe        = dim_probe,
    .open         = dim_open,
    .close        = dim_close,
    .read_track   = dim_read_track,
    .write_track  = dim_write_track,
    .verify_track = uft_generic_verify_track,
    /* MF-1037: bleibt DERIVED, und das ist eine Entscheidung, keine
     * Unterlassung. Es gibt keine Spezifikation des Urhebers — was
     * es gibt, ist die BESCHREIBUNG eines Dritten (hxcfe) plus ein
     * ausgefuehrter Lader. Das ist genau „De-facto-Standard ohne
     * formale Spec“. Anders als bei `pri` (MF-1036), wo der Text vom
     * Urheber selbst stammt und deshalb OFFICIAL_FULL traegt. */
    .spec_status = UFT_SPEC_DERIVED,  /* V415-PLAN PLUGIN.spec_status (MF-262) */
    .features = uft_format_plugin_dim_features,  /* V415-PLAN PLUGIN.features (MF-263) */
    .feature_count = sizeof(uft_format_plugin_dim_features) / sizeof(uft_format_plugin_dim_features[0]),
};

UFT_REGISTER_FORMAT_PLUGIN(dim)
