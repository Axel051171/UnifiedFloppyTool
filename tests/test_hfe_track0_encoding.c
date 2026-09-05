/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_hfe_track0_encoding.c
 * @brief HFE: die abweichende Kodierung der Spur 0 wird angewendet (MF-897)
 *
 * ── Was gemessen wurde ────────────────────────────────────────────────────
 *
 * Der HFE-Kopf traegt vier Felder fuer eine Spur-0-Ausnahme:
 *
 *     uint8_t track0s0_altencoding;
 *     uint8_t track0s0_encoding;
 *     uint8_t track0s1_altencoding;
 *     uint8_t track0s1_encoding;
 *
 * Gemessen ueber alle Quellen aus `git ls-files` (ohne `src/samdisk/`):
 * **drei** Strukturen lesen die Felder ein — `src/formats/hfe/uft_hfe.c`,
 * `include/uft/flux/uft_hfe.h`, `include/uft/uft_hfe_format.h` — und
 * **keine einzige Verzweigung** im ganzen Baum reagiert darauf.
 * `hfe_read_track()` setzte fuer JEDE Spur unbedingt
 *
 *     track->encoding = hfe_to_uft_encoding(pdata->header.track_encoding);
 *
 * also die diskweite Kodierung.
 *
 * ── Warum das ein echter Lesefehler ist, kein Schoenheitsfehler ───────────
 *
 * Der Fall ist der Normalfall bei 8-Zoll- und manchen 5,25-Zoll-Medien:
 * Spur 0 in FM, der Rest in MFM (IBM-3740-Kompatibilitaet). Eine so
 * geschriebene HFE-Datei wurde auf Spur 0 mit MFM dekodiert — die
 * Sektoren der Spur 0 fehlen dann, still.
 *
 * ── Die Polaritaet: zwei Quellen, eine Luecke, und der Autor gewinnt ──────
 *
 * Der Baum trug bis MF-897 ZWEI EINANDER WIDERSPRECHENDE Kommentare zu
 * demselben Feld:
 *
 *   include/uft/flux/uft_hfe.h        "0xFF = use default encoding"
 *   include/uft/uft_hfe_format.h      setzte 0xFF, Vermerk "Disabled"
 *   src/formats/hfe/uft_hfe.c         "0xFF = alternate encoding"  <-- falsch
 *
 * (MF-900: hier standen Zeilennummern. Der Eingriff von MF-897 hat sie
 * selbst verschoben, die Verweise waren schon beim Committen falsch.
 * Fremdverweise nennen ab jetzt das Symbol — `track0s0_altencoding`.)
 *
 * Zwei unabhaengige Quellen wurden herangezogen, beide selbst nachgelesen:
 *
 *   1. **HxC (der Urheber des Formats)**, libhxcfe:
 *        hfe_loader.c  `if(!header.track0s0_altencoding)
 *                          currentside->track_encoding =
 *                              header.track0s0_encoding;`
 *        hfe_writer.c  setzt beim Abweichen `altencoding = 0x00` und
 *                      traegt die Kodierung ein; Vorgabe sonst 0xFF.
 *      -> **nur 0x00** schaltet den Ersatz ein.
 *
 *   2. **SAMdisk** (fremde Umsetzung, im Baum unter `src/samdisk/`),
 *      `hfe.cpp`, an `track0s0_altencoding` in `struct HFE_HEADER`:
 *        "0xff = ignore, otherwise use encoding below"
 *      -> **alles ausser 0xFF** schaltet den Ersatz ein.
 *      SAMdisks Schreiber (`WriteHFE`) setzt selbst immer 0xFF.
 *
 * Die beiden decken sich an den einzigen Werten, die in freier Wildbahn
 * vorkommen — 0x00 (Ersatz) und 0xFF (kein Ersatz) — und **widersprechen
 * sich fuer 0x01..0xFE**. Dort folgt UFT dem **Urheber**: kein Ersatz,
 * und ein Vermerk, dass die Quellen uneins sind. Eine Kodierung wegen
 * eines Bytes zu wechseln, dessen Bedeutung strittig ist, waere geraten.
 *
 * ── Was dieser Test NICHT prueft ──────────────────────────────────────────
 *
 * Das Verhalten an einer ECHTEN gemischt kodierten Aufnahme. Im Korpus
 * liegt keine: `gw_amigados.hfe` ist durchgehend MFM. Das Abbild hier ist
 * **synthetisch** und im Quelltext unten Byte fuer Byte aufgebaut — es
 * behauptet nicht, wie eine echte HFE aussieht, sondern nur, was der
 * Leser mit einem dokumentierten Kopfwert tun muss. Die Bedeutung des
 * Wertes stammt aus den zwei oben benannten fremden Quellen, nicht aus
 * dieser Datei.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include "fixtures/hfe_v1_fixture.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_hfe;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-34s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

/* HFE-Kodierungswerte, wie sie im Kopf stehen (`hfe_encoding_t` in
 * `src/formats/hfe/uft_hfe.c`). */
#define ENC_ISOIBM_MFM  0x00
#define ENC_AMIGA_MFM   0x01
#define ENC_ISOIBM_FM   0x02

/*
 * MF-903: hier stand derselbe HFE-v1-Kopfbauer wie in
 * `test_hfe_write_allowed.c` — rund 40 Zeilen doppelt, vom Code-Review
 * als Duplicated Code benannt. Er liegt jetzt in
 * `tests/fixtures/hfe_v1_fixture.h`, samt der Beschreibung des Aufbaus.
 */
static int schreibe_hfe(const char *pfad,
                        uint8_t diskweit,
                        uint8_t s0_alt, uint8_t s0_enc,
                        uint8_t s1_alt, uint8_t s1_enc)
{
    const uft_test_hfe_v1_t o = {
        /* tracks */ 2, /* sides */ 2,
        /* track_encoding */ diskweit,
        /* write_allowed  */ 0x00,
        s0_alt, s0_enc, s1_alt, s1_enc,
    };
    return uft_test_write_hfe_v1(pfad, &o);
}

/* MF-903: hier formatierte `snprintf` die Konstante 0xAF7 in den Namen
 * — eine Eindeutigkeit, die nie eingeloest wurde. Ein fester Name tut
 * dasselbe und behauptet weniger. */
static const char *tmp_pfad(void)
{
    return "uft_hfe_track0.hfe";
}

/* Liest EINE Spur ueber den echten Plugin-Pfad und liefert ihre Kodierung. */
static int spur_kodierung(const char *pfad, int cyl, int head, uft_encoding_t *out)
{
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    if (uft_format_plugin_hfe.open(&disk, pfad, true) != UFT_OK) return 0;

    uft_track_t track;
    memset(&track, 0, sizeof(track));
    uft_error_t rc = uft_format_plugin_hfe.read_track(&disk, cyl, head, &track);
    int ok = (rc == UFT_OK);
    if (ok) *out = track.encoding;
    uft_track_release(&track);
    uft_format_plugin_hfe.close(&disk);
    return ok;
}

/* ─────────────────────────────────────────────────────────────────────────
 *  1. Spur 0 Seite 0 mit Ersatz (0x00) -> FM, obwohl die Platte MFM sagt.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(spur0_seite0_ersatz_wird_angewendet)
{
    const char *p = tmp_pfad();
    ASSERT(schreibe_hfe(p, ENC_ISOIBM_MFM,
                        0x00, ENC_ISOIBM_FM,     /* Seite 0: Ersatz auf FM */
                        0xFF, 0xFF));            /* Seite 1: kein Ersatz    */

    uft_encoding_t enc = UFT_ENC_UNKNOWN;
    ASSERT(spur_kodierung(p, 0, 0, &enc));
    ASSERT(enc == UFT_ENC_FM);
    remove(p);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  2. Der Ersatz gilt SEITENWEISE, nicht fuer die ganze Spur 0.
 *
 *  Eine naheliegende Fehlumsetzung waere, den Ersatz der Seite 0 auf
 *  beide Seiten anzuwenden. Der Kopf fuehrt zwei getrennte Paare.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(spur0_seite1_ohne_ersatz_bleibt_diskweit)
{
    const char *p = tmp_pfad();
    ASSERT(schreibe_hfe(p, ENC_ISOIBM_MFM,
                        0x00, ENC_ISOIBM_FM,
                        0xFF, 0xFF));

    uft_encoding_t enc = UFT_ENC_UNKNOWN;
    ASSERT(spur_kodierung(p, 0, 1, &enc));
    ASSERT(enc == UFT_ENC_MFM);
    remove(p);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  3. Der Ersatz gilt NUR fuer Spur 0 — Spur 1 bleibt diskweit.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(spur1_bleibt_diskweit)
{
    const char *p = tmp_pfad();
    ASSERT(schreibe_hfe(p, ENC_ISOIBM_MFM,
                        0x00, ENC_ISOIBM_FM,
                        0x00, ENC_ISOIBM_FM));   /* beide Seiten der Spur 0 */

    uft_encoding_t enc = UFT_ENC_UNKNOWN;
    ASSERT(spur_kodierung(p, 1, 0, &enc));
    ASSERT(enc == UFT_ENC_MFM);
    ASSERT(spur_kodierung(p, 1, 1, &enc));
    ASSERT(enc == UFT_ENC_MFM);
    remove(p);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  4. Seite 1 kann ihren EIGENEN Ersatz haben, verschieden von Seite 0.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(beide_seiten_eigener_ersatz)
{
    const char *p = tmp_pfad();
    ASSERT(schreibe_hfe(p, ENC_ISOIBM_MFM,
                        0x00, ENC_ISOIBM_FM,      /* Seite 0 -> FM        */
                        0x00, ENC_AMIGA_MFM));    /* Seite 1 -> Amiga MFM */

    uft_encoding_t enc = UFT_ENC_UNKNOWN;
    ASSERT(spur_kodierung(p, 0, 0, &enc));
    ASSERT(enc == UFT_ENC_FM);
    ASSERT(spur_kodierung(p, 0, 1, &enc));
    ASSERT(enc == UFT_ENC_AMIGA_MFM);
    remove(p);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  5. 0xFF heisst KEIN Ersatz — auch wenn ein Kodierungsbyte danebensteht.
 *
 *  Das ist die Richtung, die der frueher falsche Kommentar an
 *  `track0s0_altencoding` in `uft_hfe.c`
 *  behauptete ("0xFF = alternate encoding"). Wer ihm folgte, wuerde hier
 *  FM melden, wo MFM steht.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(ff_heisst_kein_ersatz)
{
    const char *p = tmp_pfad();
    ASSERT(schreibe_hfe(p, ENC_ISOIBM_MFM,
                        0xFF, ENC_ISOIBM_FM,      /* 0xFF: NICHT anwenden */
                        0xFF, ENC_ISOIBM_FM));

    uft_encoding_t enc = UFT_ENC_UNKNOWN;
    ASSERT(spur_kodierung(p, 0, 0, &enc));
    ASSERT(enc == UFT_ENC_MFM);
    ASSERT(spur_kodierung(p, 0, 1, &enc));
    ASSERT(enc == UFT_ENC_MFM);
    remove(p);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  6. Bei 0x01..0xFE sind die beiden Quellen uneins — UFT folgt dem
 *     Urheber und wendet NICHTS an.
 *
 *  HxC (hfe_loader.c): nur 0x00 schaltet ein.
 *  SAMdisk (`HFE_HEADER` in hfe.cpp): alles ausser 0xFF schaltet ein.
 *
 *  Eine Kodierung wegen eines strittigen Bytes zu wechseln waere
 *  geraten. Der Vermerk darueber steht im Quelltext, nicht nur hier.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(strittiger_wert_wendet_nichts_an)
{
    const char *p = tmp_pfad();
    ASSERT(schreibe_hfe(p, ENC_ISOIBM_MFM,
                        0x01, ENC_ISOIBM_FM,
                        0x7F, ENC_ISOIBM_FM));

    uft_encoding_t enc = UFT_ENC_UNKNOWN;
    ASSERT(spur_kodierung(p, 0, 0, &enc));
    ASSERT(enc == UFT_ENC_MFM);
    ASSERT(spur_kodierung(p, 0, 1, &enc));
    ASSERT(enc == UFT_ENC_MFM);
    remove(p);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  7. Was UFT SELBST schreibt, erklaert keinen Ersatz.
 *
 *  `hfe_create()` legte den Kopf mit `hfe_header_t header = {0}` an und
 *  ruehrte die vier Spur-0-Felder nie an — sie blieben auf **0x00**, und
 *  0x00 heisst "Ersatz gilt". Jede von UFT geschriebene HFE erklaerte
 *  damit einen Ersatz auf Kodierung 0x00 (ISO MFM), den sie nie gemeint
 *  hat.
 *
 *  Das ist nicht nur intern unsauber: SAMdisk liest "alles ausser 0xFF"
 *  als Ersatz und wuerde Spur 0 einer UFT-HFE anders dekodieren als den
 *  Rest. Alle drei Referenz-Schreiber setzen 0xFF — gemessen an
 *  `tests/corpus_free/gw_amigados.hfe` (greaseweazle 1.23: 0xFF in allen
 *  vieren) und an SAMdisks Schreiber `WriteHFE`.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(eigener_schreiber_erklaert_keinen_ersatz)
{
    const char *p = "uft_hfe_created_897.hfe";
    remove(p);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    uft_geometry_t geo;
    memset(&geo, 0, sizeof(geo));
    geo.cylinders = 2; geo.heads = 2; geo.sectors = 9; geo.sector_size = 512;

    ASSERT(uft_format_plugin_hfe.create != NULL);
    ASSERT(uft_format_plugin_hfe.create(&disk, p, &geo) == UFT_OK);
    uft_format_plugin_hfe.close(&disk);

    FILE *f = fopen(p, "rb");
    ASSERT(f != NULL);
    uint8_t kopf[32];
    size_t gelesen = fread(kopf, 1, sizeof(kopf), f);
    fclose(f);
    ASSERT(gelesen == sizeof(kopf));

    ASSERT(kopf[22] == 0xFF);   /* track0s0_altencoding */
    ASSERT(kopf[23] == 0xFF);   /* track0s0_encoding    */
    ASSERT(kopf[24] == 0xFF);   /* track0s1_altencoding */
    ASSERT(kopf[25] == 0xFF);   /* track0s1_encoding    */
    remove(p);
}

int main(void)
{
    printf("HFE: abweichende Kodierung der Spur 0 (MF-897)\n");
    RUN(spur0_seite0_ersatz_wird_angewendet);
    RUN(spur0_seite1_ohne_ersatz_bleibt_diskweit);
    RUN(spur1_bleibt_diskweit);
    RUN(beide_seiten_eigener_ersatz);
    RUN(ff_heisst_kein_ersatz);
    RUN(strittiger_wert_wendet_nichts_an);
    RUN(eigener_schreiber_erklaert_keinen_ersatz);
    printf("\n  %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail ? 1 : 0;
}
