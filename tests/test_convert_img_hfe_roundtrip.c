/**
 * @file test_convert_img_hfe_roundtrip.c
 * @brief Ist die geschriebene HFE lesbar? IMG -> HFE -> IMG (MF-539)
 *
 * ── Warum es diesen Test gibt ────────────────────────────────────────────
 *
 * `uftc_convert_sectors_to_hfe()` hat bis MF-539 eine Datei erzeugt, die
 * kein Leser dekodieren kann, und dabei jede Spur und jeden Sektor als
 * gewandelt gezaehlt. Drei voneinander unabhaengige Fehler:
 *
 *   1. die Bytes wurden nicht MFM-kodiert (roh statt Zellenstrom)
 *   2. beide CRC-Felder jedes Sektors blieben 0x00 0x00
 *   3. die Bit-Spiegelung fuer HFEs LSB-first-Ablage fehlte
 *
 * Aufgefallen ist das nicht am IMG-Pfad, sondern am Amiga-Pfad: der
 * Rundlauf ADF -> HFE -> ADF lieferte eine ADF aus lauter Nullen
 * (MF-538). Der Vergleich der erzeugten Datei mit einer echten
 * Greaseweazle-Aufnahme (tests/corpus_free/gw_amigados.hfe) zeigte null
 * Synchronmarken 0x4489 gegen 22 und 5969 rohe Nullbytes gegen null —
 * mehr als drei Nullbits am Stueck kann ein MFM-Strom gar nicht enthalten.
 *
 * Der Amiga-Pfad lehnt seither ab (kein AmigaDOS-Encoder im Baum). Der
 * IBM-Pfad wurde repariert, indem der bereits vorhandene, seit MF-539
 * belegte `uft_mfm_encode_track()` gerufen wird statt einer
 * handgeschriebenen Zweitfassung daneben.
 *
 * ── Was dieser Test misst ────────────────────────────────────────────────
 *
 *      IMG (synthetisch, bekannter Inhalt)
 *        -> uftc_convert_sectors_to_hfe()
 *        -> uftc_convert_hfe_to_sectors()
 *        -> Byte-Vergleich gegen die Quelle
 *
 * Die Quelle ist synthetisch, und das ist hier zulaessig: eine IMG-Datei
 * IST der rohe Sektorinhalt, ohne Kopf, ohne Kennung, ohne Struktur, die
 * man falsch erfinden koennte. Der Aufbau steht nicht zur Debatte —
 * Zylinder x Kopf x Sektor x 512, in dieser Reihenfolge. Was geprueft
 * wird, ist nicht das Format, sondern ob die Kodierung ihre eigene
 * Dekodierung uebersteht.
 *
 * Jeder Sektor bekommt ein eigenes Muster, das seine Position enthaelt.
 * Kaemen zwei Sektoren vertauscht zurueck, faellt das auf; bei
 * gleichfoermigem Fuellmuster wuerde eine Vertauschung als Erfolg
 * durchgehen.
 *
 * ── Was rot bedeutet ─────────────────────────────────────────────────────
 *
 * Abweichende Bytes heissen: die Kodierung, die Dekodierung oder die
 * CRC-Rechnung stimmt nicht. Das ist derselbe Zustand wie vor MF-539 —
 * nur wuerde er diesmal auffallen, statt als "1440 Sektoren gewandelt"
 * gemeldet zu werden.
 */

#include "uft/uft_core.h"
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
extern uft_error_t uftc_convert_hfe_to_sectors(const uint8_t *src_data,
                                               size_t src_size,
                                               const char *src_path,
                                               const char *dst_path,
                                               uft_format_t dst_format,
                                               const uft_convert_options_ext_t *opts,
                                               uft_convert_result_t *result);

#define CYLS    80
#define HEADS    2
#define SPT     18
#define SECSZ  512
#define IMG_SZ ((size_t)CYLS * HEADS * SPT * SECSZ)   /* 1474560 */

static uint8_t src[IMG_SZ];
static uint8_t mid[8u * 1024u * 1024u];
static uint8_t out[8u * 1024u * 1024u];

static int failures;

static size_t slurp(const char *path, uint8_t *dst, size_t cap)
{
    FILE *f = fopen(path, "rb");
    if (!f) return 0;
    size_t n = fread(dst, 1, cap, f);
    fclose(f);
    return n;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== IMG -> HFE -> IMG: ist die HFE lesbar? (MF-539) ===\n");

    /* Quelle: je Sektor ein Muster, das Zylinder, Kopf und Sektor
     * enthaelt. Eine Vertauschung faellt damit auf. */
    for (int cyl = 0; cyl < CYLS; cyl++)
        for (int hd = 0; hd < HEADS; hd++)
            for (int sec = 0; sec < SPT; sec++) {
                size_t off = ((size_t)cyl * HEADS * SPT
                              + (size_t)hd * SPT + sec) * SECSZ;
                for (int i = 0; i < SECSZ; i++)
                    src[off + i] = (uint8_t)(cyl * 31 + hd * 17 + sec * 7
                                             + i * 3 + (i >> 6));
            }

    uft_convert_options_ext_t o;
    uft_convert_result_t r1, r2;
    memset(&o, 0, sizeof(o));
    o.accept_data_loss = true;
    memset(&r1, 0, sizeof(r1));
    memset(&r2, 0, sizeof(r2));

    remove("uft_ih_mid.hfe");
    remove("uft_ih_out.img");

    uft_error_t e1 = uftc_convert_sectors_to_hfe(src, IMG_SZ, "uft_ih_mid.hfe",
                                                 UFT_FORMAT_IMG, &o, &r1);
    size_t n1 = slurp("uft_ih_mid.hfe", mid, sizeof(mid));
    printf("  IMG->HFE: %d, %zu Byte, %d Spuren gewandelt / %d gescheitert, "
           "%d Sektoren\n",
           (int)e1, n1, r1.tracks_converted, r1.tracks_failed,
           r1.sectors_converted);
    if (e1 != UFT_OK || !n1) {
        printf("  FAIL: die Hinrichtung lieferte nichts\n");
        failures++;
        goto done;
    }

    /* Vor MF-539 war genau das der Zustand: null Synchronmarken im
     * gesamten Abbild. Sie sind das Erkennungszeichen eines kodierten
     * Stroms — der Test schaut nicht in eine Spur hinein, sondern zaehlt
     * ueber die ganze Datei, damit er nicht von der Verschachtelung
     * abhaengt. */
    {
        long sync = 0;
        uint16_t v = 0;
        for (size_t i = 0; i < n1; i++)
            for (int b = 7; b >= 0; b--) {
                v = (uint16_t)((v << 1) | ((mid[i] >> b) & 1));
                /* HFE legt LSB-first ab, gesucht wird deshalb das
                 * gespiegelte 0x4489 = 0x9122. */
                if (v == 0x9122) sync++;
            }
        printf("  Synchronmarken im Abbild: %ld\n", sync);
        if (sync == 0) {
            printf("  FAIL: keine einzige — der Strom ist nicht kodiert\n");
            failures++;
        }
    }

    uft_error_t e2 = uftc_convert_hfe_to_sectors(mid, n1, "uft_ih_mid.hfe",
                                                 "uft_ih_out.img",
                                                 UFT_FORMAT_IMG, &o, &r2);
    size_t n2 = slurp("uft_ih_out.img", out, sizeof(out));
    printf("  HFE->IMG: %d, %zu Byte, %d Spuren gewandelt / %d gescheitert, "
           "%d Sektoren\n",
           (int)e2, n2, r2.tracks_converted, r2.tracks_failed,
           r2.sectors_converted);
    if (e2 != UFT_OK || !n2) {
        printf("  FAIL: die Rueckrichtung lieferte nichts\n");
        failures++;
        goto done;
    }

    {
        size_t m = n2 < IMG_SZ ? n2 : IMG_SZ;
        size_t diff = 0;
        for (size_t i = 0; i < m; i++) if (src[i] != out[i]) diff++;

        /* Die Nulllinie aus MF-538: ohne sie sagt eine Prozentzahl ueber
         * einem duennen Abbild nichts. Hier ist die Quelle dicht besetzt,
         * die Linie liegt also hoch — trotzdem steht sie da, weil eine
         * Zahl ohne ihre Bezugsgroesse schon einmal einen falschen
         * Tabelleneintrag erzeugt hat. */
        size_t nonzero = 0;
        for (size_t i = 0; i < IMG_SZ; i++) if (src[i]) nonzero++;

        printf("  Vergleich: %zu -> %zu Byte, %zu von %zu verschieden "
               "(%.2f %%)\n", IMG_SZ, n2, diff, m,
               m ? 100.0 * (double)diff / (double)m : 0.0);
        printf("  Nulllinie: %zu Byte (eine Datei aus lauter Nullen)\n",
               nonzero);

        if (n2 != IMG_SZ) {
            printf("  FAIL: Groesse weicht ab\n");
            failures++;
        }
        if (diff == 0) {
            printf("  ok   BITGLEICH\n");
        } else {
            printf("  FAIL: %zu Byte verschieden — Kodierung, Dekodierung\n"
                   "        oder CRC-Rechnung stimmt nicht\n", diff);
            failures++;
        }
    }

done:
    remove("uft_ih_mid.hfe");
    remove("uft_ih_out.img");
    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
