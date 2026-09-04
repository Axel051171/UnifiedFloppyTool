/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_stx_registrierter_leser.c
 * @brief Der STX-Leser, den Benutzer wirklich bekommen (MF-847).
 *
 * ── Warum dieser Test auf DIESEN Leser zielt ─────────────────────────────
 *
 * Der Baum enthaelt VIER STX-Leser:
 *
 *   src/formats/stx/uft_stx_plugin.c      179 Z.  <- REGISTRIERT
 *   src/formats/stx/uft_stx_air.c        ~950 Z.  Port von AIR, kein Aufrufer
 *   src/formats/stx/uft_stx_parser_v2.c            kein Aufrufer
 *   src/formats/atari/uft_stx_parser.c             kein Aufrufer
 *
 * Nur der erste haengt an `UFT_REGISTER_FORMAT_PLUGIN(stx)` und wird von
 * `uft_disk_open()` erreicht. Der vollstaendige AIR-Port — mit Fuzzy-Maske,
 * Timing-Saetzen, Spurbild und Standardspur-Behandlung — wird ausserhalb
 * seiner eigenen Datei nur von `tests/test_air_cross_validate.c` genannt.
 *
 * Das ist die im Baum belegte Lage „wo ein Port neben einem nativen Parser
 * steht, ist der Port richtig" — hier verschaerft dadurch, dass der Port
 * ABGEKOPPELT ist (dieselbe Bauform wie DMS vor MF-837).
 *
 * ── Die Referenz ─────────────────────────────────────────────────────────
 *
 * Pasti-Dateiaufbau nach Jean Louis-Guerin (Pasti-documentation.pdf), im
 * Baum umgesetzt in `uft_stx_air.c:227-266`, das seinerseits ein erklaerter
 * Port von AIR `pasti/PastiRead.cs` ist (GPL-3, MF-698).
 *
 *   Dateikopf, 16 Byte:
 *     0x00 4  "RSY\0"
 *     0x04 2  version (3)
 *     0x06 2  tool
 *     0x08 2  reserved
 *     0x0A 1  trackCount        <- EIN Byte
 *     0x0B 1  revision          <- eigenes Byte
 *     0x0C 4  reserved
 *
 *   Spursatz, 16 Byte:
 *     0x00 4  recordSize
 *     0x04 4  fuzzyCount        <- Bytes der Fuzzy-Maske
 *     0x08 2  sectorCount
 *     0x0A 2  flags
 *     0x0C 2  trackLength
 *     0x0E 1  trackNumber       <- Spur in Bit 0-6, SEITE in Bit 7
 *     0x0F 1  trackType
 *
 *   Danach, wenn Flag 0x01 (SECT_DESC) gesetzt ist:
 *     sectorCount * 16 Byte Sektordeskriptoren
 *     fuzzyCount   Byte Fuzzy-Maske          <- LIEGT DAZWISCHEN
 *     dann erst die Sektordaten; `dataOffset` zaehlt ab HIER
 *       (AIR: `uint track_data_start = bpos;`  PastiRead.cs:210,
 *        im Port `uft_stx_air.c:346` nach `pos += td.fuzzy_count`)
 */
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_stx;

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-44s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

static void w16(uint8_t *p, uint16_t v) { p[0] = v & 0xFF; p[1] = v >> 8; }
static void w32(uint8_t *p, uint32_t v)
{ p[0] = v & 0xFF; p[1] = (v >> 8) & 0xFF; p[2] = (v >> 16) & 0xFF; p[3] = v >> 24; }

#define FUZZY_N   8u
#define SEC_N     512u
#define MUSTER    0xA7    /* erkennbares Byte in den ECHTEN Sektordaten */
#define FUELL     0x5C    /* erkennbares Byte in der FUZZY-Maske        */

/**
 * Baut eine STX-Datei mit EINER geschuetzten Spur:
 * ein Sektor, davor eine Fuzzy-Maske von FUZZY_N Byte.
 *
 * @param revision  Wert fuer Byte 0x0B des Dateikopfs.
 * @param trknum    Wert fuer Byte 0x0E des Spursatzes (Spur|Seite<<7).
 * @param flags     Spur-Flags (0x01 = Sektordeskriptoren vorhanden).
 */
static uint8_t *baue_stx(size_t *out_len, uint8_t revision,
                         uint8_t trknum, uint16_t flags)
{
    /* Eine STANDARDspur (Flag 0x01 nicht gesetzt) traegt weder
     * Deskriptoren noch eine Fuzzy-Maske — die Sektordaten folgen dem
     * Spursatz unmittelbar. Das Abbild muss also der Spurart entsprechen,
     * sonst prueft der Test ein Gebilde, das es nicht gibt. */
    const int      mit_desc  = (flags & 0x01) != 0;
    const uint32_t vorspann  = mit_desc ? (16u + FUZZY_N) : 0u;
    const uint32_t track_rec = 16u + vorspann + SEC_N;
    const size_t   total     = 16u + track_rec;
    uint8_t *b = calloc(1, total);
    if (!b) return NULL;

    memcpy(b, "RSY\0", 4);
    w16(b + 4, 3);              /* version */
    b[10] = 1;                  /* trackCount — EIN Byte */
    b[11] = revision;

    uint8_t *t = b + 16;
    w32(t + 0, track_rec);
    w32(t + 4, mit_desc ? FUZZY_N : 0u);
    w16(t + 8, 1);              /* sectorCount */
    w16(t + 10, flags);
    w16(t + 12, 6250);          /* trackLength */
    t[14] = trknum;
    t[15] = 0;

    uint8_t *sd = t + 16;
    if (!mit_desc) {
        /* Standardspur: unmittelbar 512 Byte Nutzdaten. */
        memset(sd, MUSTER, SEC_N);
        *out_len = total;
        return b;
    }

    w32(sd + 0, 0);             /* dataOffset — ab track_data_start */
    w16(sd + 4, 0);             /* bitPosition */
    w16(sd + 6, 100);           /* readTime */
    sd[8]  = trknum & 0x7F;     /* ID track  */
    sd[9]  = 0;                 /* ID side   */
    sd[10] = 1;                 /* ID number */
    sd[11] = 2;                 /* ID size -> 128<<2 = 512 */
    w16(sd + 12, 0x1234);       /* ID CRC — die zwei Byte, die manche
                                 * Fassungen dieses Baums auslassen */
    sd[14] = 0;                 /* fdcFlags */
    sd[15] = 0;                 /* reserved */

    memset(sd + 16, FUELL, FUZZY_N);            /* Fuzzy-Maske   */
    memset(sd + 16 + FUZZY_N, MUSTER, SEC_N);   /* Sektordaten   */

    *out_len = total;
    return b;
}

static const char *TMP = "test_stx_reg.tmp.stx";

static int schreibe(const uint8_t *b, size_t n)
{
    FILE *f = fopen(TMP, "wb");
    if (!f) return 0;
    size_t w = fwrite(b, 1, n, f);
    fclose(f);
    return w == n;
}

/* ------------------------------------------------------------------ */

TEST(sektordaten_liegen_hinter_der_fuzzy_maske)
{
    /* DER ROTBEWEIS.
     *
     * `dataOffset` zaehlt ab dem Ende der Fuzzy-Maske. Wer sie nicht
     * ueberspringt, liest FUZZY_N Byte zu frueh — und bekommt die
     * Maske als Nutzdaten. Still, mit UFT_OK.
     *
     * Genau dafuer gibt es STX: Spuren MIT Fuzzy-Maske sind die
     * geschuetzten. Der Fehler trifft also nicht den Randfall, sondern
     * den Zweck des Formats. */
    size_t n; uint8_t *b = baue_stx(&n, 2, 0, 0x21 /* SECT_DESC|PROT */);
    ASSERT(b != NULL);
    ASSERT(schreibe(b, n));
    free(b);

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    ASSERT(uft_format_plugin_stx.open(&disk, TMP, true) == UFT_OK);

    uft_track_t tr;
    memset(&tr, 0, sizeof tr);
    ASSERT(uft_format_plugin_stx.read_track(&disk, 0, 0, &tr) == UFT_OK);
    ASSERT(tr.sector_count == 1);

    const uint8_t *d = tr.sectors[0].data;
    ASSERT(d != NULL);
    if (d[0] != MUSTER) {
        printf("\n      erstes Byte des Sektors: %02X (erwartet %02X)\n"
               "      -> %02X ist das Fuellbyte der FUZZY-MASKE;\n"
               "         der Leser hat %u Byte zu frueh angesetzt\n      ",
               d[0], MUSTER, FUELL, FUZZY_N);
        _fail++;
    }
    uft_track_cleanup(&tr);
    uft_format_plugin_stx.close(&disk);
    remove(TMP);
}

TEST(spurzahl_ist_ein_byte_nicht_zwei)
{
    /* Gegenprobe 1 — anderer Fehler, gleiche Datei.
     *
     * Byte 0x0A ist `trackCount`, Byte 0x0B ist `revision`. Wer an 0x0A
     * ein LE16 liest, faltet die Revision in die Spurzahl: bei
     * revision=2 wird aus 1 Spur 0x0201 = 513, nach der Klemme 200 —
     * und die gemeldete Geometrie hat 100 Zylinder statt 1. */
    size_t n; uint8_t *b = baue_stx(&n, 2, 0, 0x21);
    ASSERT(b != NULL);
    ASSERT(schreibe(b, n));
    free(b);

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    ASSERT(uft_format_plugin_stx.open(&disk, TMP, true) == UFT_OK);

    if (disk.geometry.cylinders != 1) {
        printf("\n      gemeldete Zylinder: %u (erwartet 1)\n"
               "      -> revision=%u ist in die Spurzahl gerutscht\n      ",
               disk.geometry.cylinders, 2u);
        _fail++;
    }
    uft_format_plugin_stx.close(&disk);
    remove(TMP);
}

TEST(die_seite_steckt_in_bit_7_von_byte_14)
{
    /* Gegenprobe 2.
     *
     * `trackNumber` traegt die Spur in Bit 0-6 und die SEITE in Bit 7
     * (AIR: `track = td.track_number & 0x7F; side = (td.track_number
     * >> 7) & 1;`, uft_stx_air.c:265-266). Eine Datei mit genau EINER
     * Spur, die Seite 1 / Spur 0 beschreibt, darf nicht als Seite 0
     * ausgeliefert werden.
     *
     * Der registrierte Leser ordnet Spursaetze nach ihrer REIHENFOLGE
     * in der Datei zu (`trk_idx = cyl*2 + head` gegen `track_offsets[]`)
     * und liest Byte 14 nie. */
    size_t n; uint8_t *b = baue_stx(&n, 0, 0x80 /* Seite 1, Spur 0 */, 0x21);
    ASSERT(b != NULL);
    ASSERT(schreibe(b, n));
    free(b);

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    ASSERT(uft_format_plugin_stx.open(&disk, TMP, true) == UFT_OK);

    uft_track_t s0, s1;
    memset(&s0, 0, sizeof s0); memset(&s1, 0, sizeof s1);
    uft_format_plugin_stx.read_track(&disk, 0, 0, &s0);
    uft_format_plugin_stx.read_track(&disk, 0, 1, &s1);

    if (!(s0.sector_count == 0 && s1.sector_count == 1)) {
        printf("\n      Seite 0: %u Sektoren, Seite 1: %u Sektoren\n"
               "      -> erwartet 0 und 1; Byte 14 (0x80) wird nicht "
               "gelesen\n      ",
               (unsigned)s0.sector_count, (unsigned)s1.sector_count);
        _fail++;
    }
    uft_track_cleanup(&s0); uft_track_cleanup(&s1);
    uft_format_plugin_stx.close(&disk);
    remove(TMP);
}

TEST(ohne_sect_desc_flag_keine_deskriptoren_annehmen)
{
    /* Gegenprobe 3.
     *
     * Ist Flag 0x01 NICHT gesetzt, folgen dem Spursatz unmittelbar
     * 512-Byte-Sektoren ohne Deskriptoren (AIR, PastiRead.cs:314-334;
     * im Port der `else`-Zweig bei uft_stx_air.c:506).
     *
     * Wer die Flags kassiert und immer Deskriptoren annimmt, deutet
     * Nutzdaten als Adressfelder. Hier bleibt der Test bewusst
     * ANSPRUCHSLOS: verlangt wird nur, dass nicht mit erfundenen
     * Adressen ein Sektor gemeldet wird, dessen Inhalt nirgends steht.
     * Ob eine Standardspur voll unterstuetzt wird, ist eine eigene
     * Frage — Ehrlichkeit vor Vollstaendigkeit. */
    size_t n; uint8_t *b = baue_stx(&n, 0, 0, 0x20 /* PROT, kein SECT_DESC */);
    ASSERT(b != NULL);
    ASSERT(schreibe(b, n));
    free(b);

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    ASSERT(uft_format_plugin_stx.open(&disk, TMP, true) == UFT_OK);

    uft_track_t tr;
    memset(&tr, 0, sizeof tr);
    uft_format_plugin_stx.read_track(&disk, 0, 0, &tr);

    /* Wenn ein Sektor gemeldet wird, muss sein Inhalt aus der Datei
     * stammen — nicht aus dem Deskriptorbereich. */
    if (tr.sector_count > 0 && tr.sectors[0].data &&
        tr.sectors[0].data[0] != MUSTER) {
        printf("\n      Sektor gemeldet, erstes Byte %02X (weder Muster "
               "%02X noch nichts)\n"
               "      -> Flag 0x01 wurde nicht geprueft\n      ",
               tr.sectors[0].data[0], MUSTER);
        _fail++;
    }
    uft_track_cleanup(&tr);
    uft_format_plugin_stx.close(&disk);
    remove(TMP);
}

/*
 * Baut eine Spur mit @p nsec Sektoren — fuer den Klemmfall.
 * Immer mit Deskriptoren, ohne Fuzzy-Maske (die ist hier nicht die Frage).
 */
static uint8_t *baue_stx_viele(size_t *out_len, uint16_t nsec)
{
    const uint32_t track_rec = 16u + (uint32_t)nsec * 16u
                             + (uint32_t)nsec * SEC_N;
    const size_t   total     = 16u + track_rec;
    uint8_t *b = calloc(1, total);
    if (!b) return NULL;

    memcpy(b, "RSY\0", 4);
    w16(b + 4, 3);
    b[10] = 1;

    uint8_t *tr = b + 16;
    w32(tr + 0, track_rec);
    w32(tr + 4, 0);                 /* keine Fuzzy-Maske */
    w16(tr + 8, nsec);
    w16(tr + 10, 0x21);             /* SECT_DESC | PROT */
    w16(tr + 12, 6250);
    tr[14] = 0;
    tr[15] = 0;

    uint8_t *sd   = tr + 16;
    uint8_t *daten = sd + (size_t)nsec * 16u;
    for (uint16_t i = 0; i < nsec; i++) {
        uint8_t *d = sd + (size_t)i * 16u;
        w32(d + 0, (uint32_t)i * SEC_N);   /* dataOffset */
        w16(d + 4, 0);
        w16(d + 6, 100);
        d[8]  = 0;
        d[9]  = 0;
        d[10] = (uint8_t)(i + 1);         /* R, 1-basiert */
        d[11] = 2;                        /* N -> 512 */
        w16(d + 12, 0x1234);
        d[14] = 0;
        d[15] = 0;
        memset(daten + (size_t)i * SEC_N, (uint8_t)(0x40 + i), SEC_N);
    }
    *out_len = total;
    return b;
}

TEST(zu_viele_sektoren_werden_gemeldet_nicht_verschwiegen)
{
    /* MF-854 / P3-58: mit der Verdrahtung des vollstaendigen Ports wurde
     * dessen Klemme (32 Deskriptoren je Spur) SCHARF. Sie ist eine
     * Grenze dieser Umsetzung, nicht des Formats — `sectorCount` ist ein
     * uint16, und „Sherman M4" fuehrt 70 Sektoren je Spur (DrCoolZic,
     * Atari Copy Protection Rev 1.4, Klasse NOS).
     *
     * „Kein Bit verloren" heisst nicht, dass nie etwas fehlt, sondern
     * dass Fehlendes BENANNT wird. Genau das wird hier geprueft — und
     * es war bis zu diesem Fall eine blosse Zusage: MF-854 hat die
     * Meldung eingebaut und nie bewiesen, dass sie feuert. */
    size_t n; uint8_t *b = baue_stx_viele(&n, 40);
    ASSERT(b != NULL);
    ASSERT(schreibe(b, n));
    free(b);

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    ASSERT(uft_format_plugin_stx.open(&disk, TMP, true) == UFT_OK);

    uft_track_t tr;
    memset(&tr, 0, sizeof tr);
    ASSERT(uft_format_plugin_stx.read_track(&disk, 0, 0, &tr) == UFT_OK);

    /* Es kommen 32 an — die Grenze der Umsetzung. */
    ASSERT(tr.sector_count == 32);

    /* Und der Verlust ist VERMERKT, nicht still. */
    if (tr.errors == 0) {
        printf("\n      40 angekuendigt, 32 geliefert, errors == 0\n"
               "      -> acht Sektoren sind still verschwunden\n      ");
        _fail++;
    }
    uft_track_cleanup(&tr);
    uft_format_plugin_stx.close(&disk);
    remove(TMP);
}

TEST(passende_sektorzahl_meldet_keinen_verlust)
{
    /* Gegenprobe: genau an der Grenze darf NICHTS gemeldet werden.
     * Ohne diesen Fall waere eine Fassung gruen, die immer warnt. */
    size_t n; uint8_t *b = baue_stx_viele(&n, 32);
    ASSERT(b != NULL);
    ASSERT(schreibe(b, n));
    free(b);

    uft_disk_t disk;
    memset(&disk, 0, sizeof disk);
    ASSERT(uft_format_plugin_stx.open(&disk, TMP, true) == UFT_OK);

    uft_track_t tr;
    memset(&tr, 0, sizeof tr);
    ASSERT(uft_format_plugin_stx.read_track(&disk, 0, 0, &tr) == UFT_OK);
    ASSERT(tr.sector_count == 32);
    ASSERT(tr.errors == 0);

    uft_track_cleanup(&tr);
    uft_format_plugin_stx.close(&disk);
    remove(TMP);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== STX: der registrierte Leser (MF-847) ===\n");
    RUN(sektordaten_liegen_hinter_der_fuzzy_maske);
    RUN(spurzahl_ist_ein_byte_nicht_zwei);
    RUN(die_seite_steckt_in_bit_7_von_byte_14);
    RUN(ohne_sect_desc_flag_keine_deskriptoren_annehmen);
    RUN(zu_viele_sektoren_werden_gemeldet_nicht_verschwiegen);
    RUN(passende_sektorzahl_meldet_keinen_verlust);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
