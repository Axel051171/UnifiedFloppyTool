/**
 * @file test_convert_atr_xfd.c
 * @brief ATR <-> XFD: der 13. Wandlungspfad, mit gemessener Grenze (MF-655)
 *
 * ── Was der Unterschied wirklich ist ─────────────────────────────────────
 *
 * Selbst gemessen am Korpus-Paar `atrcopy_dos2sd.atr` (92 176 B) und
 * `atrcopy_dos2sd.xfd` (92 160 B):
 *
 *     atr[16:] == xfd   ->   True
 *
 * XFD ist das ATR ohne seinen 16-Byte-Kopf. Nicht „im Wesentlichen":
 * byteweise, ohne Ausnahme.
 *
 * ── Warum das trotzdem nicht pauschal verlustfrei ist ────────────────────
 *
 * Der Kopf traegt zwei Angaben (`src/formats/atr/uft_atr.c:53-56`):
 *
 *     Byte 0-1   Magic 0x0296
 *     Byte 2-3   Paragraphen (niederwertig)   -- aus der Dateigroesse ableitbar
 *     Byte 4-5   SEKTORGROESSE                -- NICHT ableitbar
 *     Byte 6     Paragraphen (hoeherwertig)   -- ableitbar
 *     Byte 7-15  unbenutzt
 *
 * Die Sektorgroesse (128 / 256 / 512) kann XFD nicht speichern, und aus
 * der Dateigroesse folgt sie nicht: 184 320 Byte sind 1440 Sektoren zu
 * 128 **oder** 720 zu 256. Ein Rundlauf ATR->XFD->ATR ist deshalb nur
 * dann bitgleich, wenn der Kopf nichts enthaelt, was die Groesse nicht
 * schon sagt — also Sektorgroesse 128 und Byte 7-15 null.
 *
 * Genau diese Grenze prueft dieser Test, in beide Richtungen:
 *
 *   1. ATR -> XFD  muss die Nutzdaten bitgleich lassen (Kopf faellt weg)
 *   2. XFD -> ATR  muss einen Kopf erzeugen, der den Rundlauf schliesst
 *   3. der volle Rundlauf ATR -> XFD -> ATR muss bitgleich sein
 *   4. ein DD-ATR (Sektorgroesse 256) darf NICHT stillschweigend
 *      durchlaufen — dort ist die Angabe nicht rekonstruierbar
 *
 * Punkt 4 ist der eigentliche Grund fuer diesen Test. Ohne ihn waere der
 * Matrix-Eintrag eine Zusage, die fuer SD stimmt und fuer DD still
 * falsch ist — dieselbe Bauart wie die LOSSLESS-Zusage fuer SCP<->HFE,
 * die MF-527 zuruecknehmen musste.
 *
 * Alle Wandlungen laufen OHNE `accept_data_loss`, soweit sie als
 * verlustfrei gefuehrt werden. Das ist der Sinn der Einstufung.
 *
 * ── Warum XFD -> ATR ueber uft_convert_memory() geht ─────────────────────
 *
 * `uft_convert_file()` erkennt das Quellformat selbst. Bei XFD kann es das
 * nicht, und das ist kein Mangel, sondern die Natur des Formats: XFD ist
 * KOPFLOS. Gemessen am Korpus-XFD meldet der Verteiler:
 *
 *     Source format is ambiguous: 5 plugins claim this data at
 *     confidence 40; 'XFD' wins by registration order, not by evidence
 *
 * Das ist genau richtig. Eine Wandlung auszufuehren hiesse, den Dekoder des
 * Gewinners ueber die Bytes laufen zu lassen — und der Gewinner stand nur
 * durch die Reihenfolge fest. Der Verteiler weist ab und sagt im eigenen
 * Kommentar, was stattdessen zu tun ist: „the caller can name the format
 * explicitly".
 *
 * `uft_convert_memory()` nimmt `src_format` als Parameter. Das ist dieser
 * Weg — kein Umweg um das Tor, sondern der dafuer vorgesehene Eingang.
 * Seit MF-567 laeuft auch er durch das Preflight-Tor.
 *
 * Nebenbei bestaetigt der Test damit beide Ketten des Verteilers: der
 * Dateiweg (ATR -> XFD) und der Speicherweg (XFD -> ATR). Dass es zwei
 * gibt, war die Ursache von MF-567.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_convert.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "."
#endif

#define MAX_IMG (4 * 1024 * 1024)
static uint8_t a[MAX_IMG], b[MAX_IMG], c[MAX_IMG];

static int failures;

static size_t slurp(const char *path, uint8_t *dst)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(dst, 1, MAX_IMG, f);
    fclose(f);
    return n;
}

/* Wird fuer das DD-Pruefbild gebraucht, das es als Datei geben muss. */
static bool spill(const char *path, const uint8_t *data, size_t n)
{
    FILE *f = fopen(path, "wb");
    if (!f) return false;
    bool ok = fwrite(data, 1, n, f) == n;
    fclose(f);
    return ok;
}

/* Eine Wandlung ohne Zustimmung. Gibt die Ausgabegroesse zurueck, 0 bei
 * Ablehnung — und sagt dann auch, warum. */
static size_t convert(const char *src, const char *dst, uft_format_t fmt,
                      const char *label, uint8_t *out)
{
    uft_convert_options_t o;
    memset(&o, 0, sizeof(o));
    uft_convert_result_t r;
    memset(&r, 0, sizeof(r));

    remove(dst);
    uft_error_t e = uft_convert_file(src, dst, fmt, &o, &r);
    if (e != UFT_OK) {
        printf("  FAIL %s: abgelehnt (%d)%s%s\n", label, (int)e,
               r.warning_count > 0 ? " — " : "",
               r.warning_count > 0 ? r.warnings[0] : "");
        failures++;
        remove(dst);
        return 0;
    }
    size_t n = slurp(dst, out);
    remove(dst);
    if (!n) {
        printf("  FAIL %s: Ausgabe leer oder unlesbar\n", label);
        failures++;
    }
    return n;
}

/* Wandlung mit AUSDRUECKLICH benanntem Quellformat, ohne Zustimmung.
 * Der Weg fuer kopflose Formate — siehe Kopf dieser Datei. */
static size_t convert_named(const uint8_t *src, size_t n_src,
                            uft_format_t src_fmt, uft_format_t dst_fmt,
                            const char *label, uint8_t *out)
{
    uft_convert_options_ext_t o;
    memset(&o, 0, sizeof(o));
    uft_convert_result_t r;
    memset(&r, 0, sizeof(r));

    uint8_t *dst = NULL;
    size_t n_dst = 0;
    uft_error_t e = uft_convert_memory(src, n_src, src_fmt,
                                       &dst, &n_dst, dst_fmt, &o, &r);
    if (e != UFT_OK) {
        printf("  FAIL %s: abgelehnt (%d)%s%s\n", label, (int)e,
               r.warning_count > 0 ? " — " : "",
               r.warning_count > 0 ? r.warnings[0] : "");
        failures++;
        free(dst);
        return 0;
    }
    if (!dst || n_dst == 0) {
        printf("  FAIL %s: leere Ausgabe trotz UFT_OK\n", label);
        failures++;
        free(dst);
        return 0;
    }
    if (n_dst > MAX_IMG) {
        printf("  FAIL %s: Ausgabe %zu > Puffer\n", label, n_dst);
        failures++;
        free(dst);
        return 0;
    }
    memcpy(out, dst, n_dst);
    free(dst);
    return n_dst;
}

/* 1 + 2 + 3: die drei Richtungen am echten Korpus-Paar. */
static void test_korpus_paar(void)
{
    char p_atr[1024], p_xfd[1024];
    snprintf(p_atr, sizeof(p_atr), "%s/atrcopy_dos2sd.atr", UFT_CORPUS_DIR);
    snprintf(p_xfd, sizeof(p_xfd), "%s/atrcopy_dos2sd.xfd", UFT_CORPUS_DIR);

    size_t n_atr = slurp(p_atr, a);
    size_t n_xfd = slurp(p_xfd, b);
    if (!n_atr || !n_xfd) {
        printf("  FAIL Korpus-Paar fehlt (%s / %s)\n", p_atr, p_xfd);
        failures++;
        return;
    }

    /* Die Voraussetzung selbst pruefen, statt sie zu glauben. */
    if (n_atr != n_xfd + 16 || memcmp(a + 16, b, n_xfd) != 0) {
        printf("  FAIL Voraussetzung: atr[16:] != xfd (%zu vs %zu B)\n",
               n_atr, n_xfd);
        failures++;
        return;
    }
    printf("  ok   Voraussetzung: atr[16:] == xfd, %zu Byte\n", n_xfd);

    /* 1. ATR -> XFD */
    size_t n = convert(p_atr, "uft_axfd_out.xfd", UFT_FORMAT_XFD,
                       "ATR -> XFD", c);
    if (n) {
        if (n != n_xfd || memcmp(c, b, n_xfd) != 0) {
            printf("  FAIL ATR -> XFD: nicht bitgleich zum Korpus-XFD "
                   "(%zu vs %zu B)\n", n, n_xfd);
            failures++;
        } else {
            printf("  ok   ATR -> XFD: bitgleich zum Korpus-XFD, %zu Byte, "
                   "ohne Zustimmung\n", n);
        }
    }

    /* 2. XFD -> ATR, Quellformat ausdruecklich benannt */
    n = convert_named(b, n_xfd, UFT_FORMAT_XFD, UFT_FORMAT_ATR,
                      "XFD -> ATR", c);
    if (n) {
        if (n != n_atr || memcmp(c, a, n_atr) != 0) {
            size_t diff = 0, m = n < n_atr ? n : n_atr;
            for (size_t i = 0; i < m; i++) if (c[i] != a[i]) diff++;
            printf("  FAIL XFD -> ATR: nicht bitgleich zum Korpus-ATR "
                   "(%zu vs %zu B, %zu Bytes verschieden)\n",
                   n, n_atr, diff);
            printf("       erzeugter Kopf: ");
            for (int i = 0; i < 16 && (size_t)i < n; i++) printf("%02x ", c[i]);
            printf("\n       erwarteter Kopf: ");
            for (int i = 0; i < 16; i++) printf("%02x ", a[i]);
            printf("\n");
            failures++;
        } else {
            printf("  ok   XFD -> ATR: bitgleich zum Korpus-ATR, %zu Byte, "
                   "ohne Zustimmung\n", n);
        }
    }

    /* 3. voller Rundlauf ATR -> XFD -> ATR */
    n = convert(p_atr, "uft_axfd_rt.xfd", UFT_FORMAT_XFD,
                "Rundlauf Schritt 1", c);
    if (n) {
        static uint8_t zwischen[MAX_IMG];
        memcpy(zwischen, c, n);
        size_t m = convert_named(zwischen, n, UFT_FORMAT_XFD, UFT_FORMAT_ATR,
                                 "Rundlauf Schritt 2", c);
        if (m) {
            if (m != n_atr || memcmp(c, a, n_atr) != 0) {
                printf("  FAIL Rundlauf ATR->XFD->ATR nicht bitgleich "
                       "(%zu vs %zu B)\n", m, n_atr);
                failures++;
            } else {
                printf("  ok   Rundlauf ATR->XFD->ATR: bitgleich, %zu Byte\n", m);
            }
        }
    }
}

/* 4. Die Grenze: ein DD-ATR traegt Sektorgroesse 256 im Kopf, und die
 * kann XFD nicht speichern. Die Wandlung darf nicht stillschweigend
 * laufen — sie muss ohne Zustimmung ABGELEHNT werden. */
static void test_dd_wird_nicht_still_verworfen(void)
{
    /* 720 Sektoren: 3 Bootsektoren zu 128 + 717 zu 256 = 183 936 Nutzdaten.
     * Der Inhalt ist gleichgueltig, es geht um den Kopf. */
    const size_t payload = 3u * 128u + 717u * 256u;
    const size_t total = 16u + payload;
    if (total > MAX_IMG) { printf("  FAIL DD-Testbild zu gross\n"); failures++; return; }

    memset(a, 0xA5, total);
    uint32_t paragraphs = (uint32_t)(payload / 16u);
    a[0] = 0x96; a[1] = 0x02;                        /* Magic 0x0296 */
    a[2] = (uint8_t)(paragraphs & 0xFF);
    a[3] = (uint8_t)((paragraphs >> 8) & 0xFF);
    a[4] = 0x00; a[5] = 0x01;                        /* Sektorgroesse 256 */
    a[6] = (uint8_t)((paragraphs >> 16) & 0xFF);
    for (int i = 7; i < 16; i++) a[i] = 0;

    if (!spill("uft_axfd_dd.atr", a, total)) {
        printf("  FAIL DD-Testbild nicht schreibbar\n");
        failures++;
        return;
    }

    uft_convert_options_t o;
    memset(&o, 0, sizeof(o));
    uft_convert_result_t r;
    memset(&r, 0, sizeof(r));

    remove("uft_axfd_dd.xfd");
    uft_error_t e = uft_convert_file("uft_axfd_dd.atr", "uft_axfd_dd.xfd",
                                     UFT_FORMAT_XFD, &o, &r);
    if (e == UFT_OK) {
        printf("  FAIL DD-ATR (Sektorgroesse 256) lief OHNE Zustimmung "
               "durch — die Angabe ist danach unwiederbringlich\n");
        failures++;
    } else {
        printf("  ok   DD-ATR wird ohne Zustimmung abgelehnt (%d)%s%s\n",
               (int)e,
               r.warning_count > 0 ? " — " : "",
               r.warning_count > 0 ? r.warnings[0] : "");
    }
    remove("uft_axfd_dd.xfd");
    remove("uft_axfd_dd.atr");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);

    if (uft_register_all_formats() != UFT_OK) {
        printf("FEHLER: uft_register_all_formats() schlug fehl\n");
        return 1;
    }

    printf("ATR <-> XFD: Nutzdaten bitgleich, Sektorgroesse ist die Grenze "
           "(MF-655)\n");
    test_korpus_paar();
    test_dd_wird_nicht_still_verworfen();

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
