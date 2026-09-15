/**
 * @file test_hfe_drehzahl.c
 * @brief Die Drehzahl stand fest auf 300 — auch fuer 360-U/min-Medien (MF-1166)
 *
 * ── Der Befund ──────────────────────────────────────────────────────────
 *
 * `uftc_convert_sectors_to_hfe()` rechnet die Spurlaenge seit MF-539
 * richtig aus Bitrate und Drehzahl — und verdrahtet die Drehzahl fest:
 *
 *     const int track_cells =
 *         (int)((uint32_t)bitrate * 1000u * 60u / 300u * 2u);
 *     ...
 *     hdr->drive_rpm = 300;
 *
 * Der Kommentar darueber zeigt die Herleitung und ist richtig — NUR fuer
 * 300 U/min. Die Geometriewahl derselben Funktion waehlt aber einen Fall,
 * der 360 braucht:
 *
 *     } else if (src_size <= 1228800) {
 *         cylinders = 80; sectors = 15; bitrate = 500;
 *         iface = HFE_IF_IBMPC_HD;
 *
 * 80 x 2 x 15 x 512 = 1 228 800 Byte ist die 1,2-MB-Diskette im
 * 5,25-Zoll-HD-Laufwerk, und die dreht mit 360 U/min. Gemessen am
 * Vorzustand bekommt sie eine Spur von 25 000 Byte statt 20 834 — 20 %
 * zu lang — und der Kopf behauptet 300 U/min.
 *
 * Die drei anderen Zweige derselben Wahl sind richtig: 360 K und 720 K
 * (250 kbit/s) und die 1,44-MB-Diskette (3,5 Zoll HD, 500 kbit/s) laufen
 * alle mit 300.
 *
 * ── Warum der vorhandene Rundlauf es nicht gefangen hat ─────────────────
 *
 * `tests/test_convert_img_hfe_roundtrip.c` prueft genau diesen Wandler —
 * mit `SPT 18` und `IMG_SZ 1474560`, also der 3,5-Zoll-Diskette, wo 300
 * stimmt. Der 15-Sektor-Zweig wird von keinem Test betreten. Dieselbe
 * Gestalt wie MF-1164: die richtige Zusage, eine Geometrie.
 *
 * ── Die Quelle, und eine Gegenprobe, die aufgeht ────────────────────────
 *
 * `cw2dmk/jv3.h` (GPL-2, NUR GELESEN — Kanal *Spec*) fuehrt die Spurlaengen
 * mit der Herleitung im Kommentar, als DATENbytes:
 *
 *     TRKSIZE_DD    6250    250kHz / 5 Hz [300rpm] / 8
 *     TRKSIZE_8DD  10416    500kHz / 6 Hz [360rpm] / 8
 *     TRKSIZE_5HD  10416    500kHz / 6 Hz [360rpm] / 8
 *     TRKSIZE_3HD  12500    500kHz / 5 Hz [300rpm] / 8
 *
 * UFTs Rechnung verdoppelt fuer die MFM-Zellen (der Kommentar bei MF-539
 * erklaert es: „MFM legt zwischen je zwei Datenbits ein Taktbit"). Unsere
 * Werte muessen also genau das Doppelte sein — und das ist die Gegenprobe,
 * die Fall 1 festhaelt:
 *
 *     5,25" HD : 2 x 10 416 = 20 832  gegen unsere 20 834   (Abrundung)
 *     3,5"  HD : 2 x 12 500 = 25 000  gegen unsere 25 000   (genau)
 *     DD       : 2 x  6 250 = 12 500  gegen unsere 12 500   (genau)
 *
 * Zwei unabhaengige Rechenwege, dieselben Zahlen. Die Differenz von 2 Byte
 * bei 5,25" ist die Ganzzahlteilung: 30 000 000 / 360 = 83 333 statt
 * 83 333,33. Beides trifft dieselbe Spurkapazitaet; die 4166 Byte
 * Unterschied zum heutigen Stand sind es nicht.
 *
 * ── Zugang ──────────────────────────────────────────────────────────────
 *
 * Der Kopf wird BYTEWEISE nach der HxC-Feldlage gelesen, nicht ueber
 * `hfe_header_t`. Ein Test, der seine Erwartung aus derselben Struktur
 * holt wie der Prueflin, kann die Struktur nicht pruefen (MF-1000).
 *
 * ── Rotbeweis ───────────────────────────────────────────────────────────
 *
 * Fall 1, 3 und 4 sind vor MF-1166 gruen (Arithmetik und die zwei
 * 300-U/min-Faelle). Fall 2 faellt.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_convert.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern uft_error_t uftc_convert_sectors_to_hfe(const uint8_t *src_data,
                                               size_t src_size,
                                               const char *dst_path,
                                               uft_format_t src_format,
                                               const uft_convert_options_ext_t *opts,
                                               uft_convert_result_t *result);

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-42s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* HxC-HFE-v1-Kopf, Feldlage aus der Formatbeschreibung — bewusst als
 * Versatzkonstanten und nicht ueber `hfe_header_t`. */
#define HFE_OFF_BITRATE      12u   /* LE16, kbit/s */
#define HFE_OFF_RPM          14u   /* LE16          */
#define HFE_OFF_IFACE        16u
#define HFE_OFF_LUT_BLOCK    18u   /* LE16, in 512-Byte-Bloecken */

#define SECSZ 512u
#define HEADS 2u

static uint8_t g_img[1474560u];              /* groesster Fall */
static uint8_t g_hfe[8u * 1024u * 1024u];

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

static size_t einlesen(const char *pfad, uint8_t *ziel, size_t cap)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return 0;
    size_t n = fread(ziel, 1, cap, f);
    fclose(f);
    return n;
}

/* Erwartete Spurlaenge je Seite: Zellen = bitrate * 1000 * 60 / rpm * 2,
 * Byte aufgerundet, dann auf 256 aufgerundet (HFE-Verschraenkung). */
static unsigned erwartete_spurlaenge(unsigned bitrate_kbps, unsigned rpm)
{
    unsigned zellen = bitrate_kbps * 1000u * 60u / rpm * 2u;
    unsigned bytes  = (zellen + 7u) / 8u;
    return ((bytes + 255u) / 256u) * 256u;
}

/* Wandelt ein selbstbenennendes IMG und gibt Kopfwerte plus LUT-Laenge
 * zurueck. 0 bei Fehlschlag. */
static int wandeln_und_lesen(unsigned cyls, unsigned spt,
                             uint16_t *rpm, uint16_t *bitrate,
                             uint8_t *iface, uint16_t *lut_len)
{
    const size_t groesse = (size_t)cyls * HEADS * spt * SECSZ;
    if (groesse > sizeof(g_img)) return 0;

    for (unsigned c = 0; c < cyls; c++)
        for (unsigned h = 0; h < HEADS; h++)
            for (unsigned s = 0; s < spt; s++) {
                size_t off = (((size_t)c * HEADS + h) * spt + s) * SECSZ;
                for (unsigned i = 0; i < SECSZ; i++)
                    g_img[off + i] = (uint8_t)(c * 31u + h * 17u + s * 7u + i * 3u);
            }

    uft_convert_options_ext_t o;
    uft_convert_result_t r;
    memset(&o, 0, sizeof(o));
    o.accept_data_loss = true;
    memset(&r, 0, sizeof(r));

    const char *pfad = "uft_hfe_rpm_probe.hfe";
    remove(pfad);
    if (uftc_convert_sectors_to_hfe(g_img, groesse, pfad,
                                    UFT_FORMAT_IMG, &o, &r) != UFT_OK)
        return 0;

    size_t n = einlesen(pfad, g_hfe, sizeof(g_hfe));
    remove(pfad);
    if (n < 1024u) return 0;
    if (memcmp(g_hfe, "HXCPICFE", 8) != 0) return 0;

    *rpm     = le16(g_hfe + HFE_OFF_RPM);
    *bitrate = le16(g_hfe + HFE_OFF_BITRATE);
    *iface   = g_hfe[HFE_OFF_IFACE];

    unsigned lut_block = le16(g_hfe + HFE_OFF_LUT_BLOCK);
    size_t   lut_off   = (size_t)lut_block * 512u;
    if (lut_off + 4u > n) return 0;
    *lut_len = le16(g_hfe + lut_off + 2u);   /* Eintrag 0: offset, length */
    return 1;
}

/* ── Fall 1: die Rechnung gegen cw2dmk ───────────────────────────────── */

TEST(die_zellrechnung_trifft_cw2dmk)
{
    /* Gruen vor UND nach MF-1166: reine Arithmetik. Sie steht zuerst, weil
     * jede weitere Zusage auf ihr ruht — und weil sie die FREMDE Quelle
     * einbezieht statt nur unsere eigene Formel zu wiederholen. */
    ASSERT(erwartete_spurlaenge(500u, 360u) == 20992u);   /* 20834 -> 256er */
    ASSERT(erwartete_spurlaenge(500u, 300u) == 25088u);   /* 25000 -> 256er */
    ASSERT(erwartete_spurlaenge(250u, 300u) == 12544u);   /* 12500 -> 256er */

    /* Gegenprobe: cw2dmk zaehlt DATENbytes, wir Zellbytes — also doppelt.
     * TRKSIZE_5HD 10416, TRKSIZE_3HD 12500, TRKSIZE_DD 6250. */
    unsigned zellen_5hd = 500u * 1000u * 60u / 360u * 2u;   /* 166666 */
    unsigned zellen_3hd = 500u * 1000u * 60u / 300u * 2u;   /* 200000 */
    unsigned zellen_dd  = 250u * 1000u * 60u / 300u * 2u;   /* 100000 */
    ASSERT((zellen_5hd + 7u) / 8u == 20834u);
    ASSERT((zellen_3hd + 7u) / 8u == 25000u);   /* == 2 * 12500 genau */
    ASSERT((zellen_dd  + 7u) / 8u == 12500u);   /* == 2 *  6250 genau */
    /* und 5HD liegt 2 Byte ueber 2*10416 — Ganzzahlteilung, nicht Fehler */
    ASSERT(20834u - 2u * 10416u == 2u);
}

/* ── Fall 2: ROTBEWEIS — 1,2 MB im 5,25-Zoll-HD-Laufwerk ────────────── */

TEST(fuenfeinviertelzoll_hd_laeuft_mit_360)
{
    /* Vor MF-1166 gemessen: rpm 300, LUT-Laenge 2 x 25088 = 50176. */
    uint16_t rpm = 0, bitrate = 0, lut_len = 0;
    uint8_t iface = 0;
    ASSERT(wandeln_und_lesen(80u, 15u, &rpm, &bitrate, &iface, &lut_len));

    ASSERT(bitrate == 500u);
    ASSERT(rpm     == 360u);
    ASSERT(lut_len == (uint16_t)(2u * erwartete_spurlaenge(500u, 360u)));
}

/* ── Fall 3: Gegenprobe — 1,44 MB bleibt bei 300 ─────────────────────── */

TEST(dreieinhalbzoll_hd_bleibt_bei_300)
{
    /* Gruen vor UND nach MF-1166. Ohne diesen Fall waere „alles gruen"
     * auch mit einem Wandler zu haben, der ueberall 360 einsetzt. */
    uint16_t rpm = 0, bitrate = 0, lut_len = 0;
    uint8_t iface = 0;
    ASSERT(wandeln_und_lesen(80u, 18u, &rpm, &bitrate, &iface, &lut_len));

    ASSERT(bitrate == 500u);
    ASSERT(rpm     == 300u);
    ASSERT(lut_len == (uint16_t)(2u * erwartete_spurlaenge(500u, 300u)));
}

/* ── Fall 4: Gegenprobe — 720 K bleibt 250 kbit/s bei 300 ────────────── */

TEST(doppelte_dichte_bleibt_250_bei_300)
{
    uint16_t rpm = 0, bitrate = 0, lut_len = 0;
    uint8_t iface = 0;
    ASSERT(wandeln_und_lesen(80u, 9u, &rpm, &bitrate, &iface, &lut_len));

    ASSERT(bitrate == 250u);
    ASSERT(rpm     == 300u);
    ASSERT(lut_len == (uint16_t)(2u * erwartete_spurlaenge(250u, 300u)));
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== HFE: die Drehzahl gehoert zum Medium (MF-1166) ===\n");
    RUN(die_zellrechnung_trifft_cw2dmk);
    RUN(fuenfeinviertelzoll_hd_laeuft_mit_360);
    RUN(dreieinhalbzoll_hd_bleibt_bei_300);
    RUN(doppelte_dichte_bleibt_250_bei_300);
    printf("\nErgebnis: %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
