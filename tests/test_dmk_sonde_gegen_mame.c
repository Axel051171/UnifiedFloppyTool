/**
 * @file test_dmk_sonde_gegen_mame.c
 * @brief `dmk_probe()` hat drei Pruefungen, die sein eigener Kommentar
 *        verlangt und der Code nur BELOHNT (MF-1151)
 *
 * ── Der Befund, und er ist gemessen ──────────────────────────────────
 *
 * A4 Runde 2 (MF-1150) hat gemessen: `uft_disk_open()` gibt
 * `tests/corpus_free/hxcfe_720k.st` — ein FLACHES Atari-ST-Abbild von
 * 737 280 Byte ohne jeden DMK-Kopf — an **DMK** mit Konfidenz **65**.
 *
 * Der Grund steht in den ersten sieben Byte der Datei:
 *
 *     00 01 55 46 54 2D 4B  =  0x00, 0x01, "UFT-K"
 *
 * Das ist UFTs EIGENE Selbstbeschreibungsmarke, und `dmk_probe()` liest
 * daraus einen gueltigen Kopf:
 *
 *     prot   = 0x00            -> +10 (Schutzbyte gueltig)
 *     tracks = 0x01            -> im erlaubten Bereich 1..96
 *     tlen   = 0x4655 = 18005  -> 'U','F' als LE16, im Bereich 1000..20000
 *     opts   = 0x54 ('T')      -> Bit 4 gesetzt: EINE Seite
 *
 * Seine einzige Groessenschranke ist
 *
 *     file_size >= 16 + tracks * sides * tlen  =  18 021
 *
 * und die ist gegen eine 737 280 Byte grosse Datei wirkungslos. **Bei
 * `tracks == 1` prueft eine UNTERE Schranke nichts.**
 *
 * ── Es ist derselbe Defekt, den MF-447 an dieser Sonde behoben hat ───
 *
 * Der Kopfkommentar von `src/formats/dmk/uft_dmk.c` erzaehlt die
 * Geschichte selbst: zwei Bereichspruefungen ergaben Konfidenz 85 und
 * beanspruchten G64-, G71- und ATR-Abbilder. Die Abhilfe war die
 * Groessenarithmetik, und der Kommentar sagt woertlich:
 *
 *     „A file whose length does not match that arithmetic is not a DMK
 *      image this reader could read to the end."
 *
 * **Der Kommentar sagt `match`, der Code sagt `>=`.** Die Korrektur war
 * richtig und um einen Vergleich zu schwach.
 *
 * ── Die Referenz: MAMEs `dmk_dsk.cpp` ───────────────────────────────
 *
 * `tools/uft-scout/work/mame-master/src/lib/formats/dmk_dsk.cpp`
 * (Wilbert Pol, **BSD-3-Clause**; nur GELESEN, Kanal *Spec* nach
 * MF-695). Sein `identify()` macht zu harten Toren, was UFT belohnt:
 *
 *     header[0] != 0x00 && header[0] != 0xff        -> return 0
 *     for (i = 5; i < 0x10; i++) if (header[i])     -> return 0
 *     track_size < 0x80 || track_size > 0x3fff      -> return 0
 *
 * Drei Unterschiede zu UFT, alle gemessen:
 *
 *   1. das Schutzbyte ist bei UFT +10 statt Bedingung
 *   2. UFT prueft nur Byte 5..11 und auch die nur mit +5; Byte 12..15
 *      liest es gar nicht (in der ST-Datei stehen dort `13 14 15 16`)
 *   3. UFTs Spurlaenge darf 1000..20000 sein, MAMEs 128..16383 —
 *      **UFT nimmt 16384..20000 an, die MAME abweist**
 *
 * **Eine Abweichung bleibt bewusst und ist hier festgenagelt:** MAME
 * nimmt Spurlaengen ab 128 Byte, UFT bleibt bei 1000. Eine untere
 * Schranke kann nur WEITER machen, und eine DMK-Spur unter 1000 Byte
 * traegt keine Diskette, die dieser Leser lesen koennte. Gestalt von
 * MF-1027, wo MAME an einer Stelle begruendet ueberstimmt wurde.
 *
 * ── Und die Konfidenz 100 ist die zweite Haelfte ─────────────────────
 *
 * Gemessen am echten Korpus-DMK (`hxcfe_pc160.dmk`): `dmk_probe()`
 * meldet **100** — 55 + 30 (Groesse exakt) + 10 (Schutzbyte) + 5
 * (Reserve null). Das ist die Spitze des Bandes „Merkmal getroffen"
 * (80..100 nach MF-729) fuer ein Format, das **gar keine Kennung hat**.
 * Was wirklich gelesen ist: elf Nullbyte, ein gueltiges Flagbyte und
 * eine aufgehende Spurarithmetik — das ist STRUKTUR, und Struktur
 * endet nach MF-729 bei 79.
 *
 * **Und ein gruener Test hat genau das bewacht:**
 * `tests/test_register_all_formats.c` verlangte `ASSERT(c >= 95)` fuer
 * einen synthetischen DMK-Kopf. Die Zusage folgte aus der
 * Ueberziehung, nicht aus dem Format — dieselbe Gestalt wie MF-1016
 * (`test_plugin_probe_real.c` bewachte JV1s erfundene zweite Seite) und
 * MF-1017 (`test_register_all_formats.c` schrieb JV3s falsche
 * Kopfgroesse fest). Seit MF-1151 verlangt sie das Band statt einer
 * Zahl.
 *
 * ── Was dieser Test NICHT prueft ─────────────────────────────────────
 *
 * MAMEs `identify()` geht noch einen Schritt weiter: es liest JEDE
 * Spur und prueft jeden der 64 IDAM-Eintraege — Versatz kleiner als die
 * Spurlaenge, und am Versatz muss `0xFE` stehen, die Adressmarke. Das
 * ist eine echte INHALTS-Pruefung und waere der Weg, das Band 50..79
 * wirklich zu verdienen. Sie ist hier NICHT umgesetzt: sie braucht die
 * ganze Spur im Sondenpuffer und ist damit eine eigene Arbeit. Als
 * Merkposten in P3-407.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

extern const uft_format_plugin_t uft_format_plugin_dmk;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *hinweis)
{
    if (ok) { printf("  [OK]   %s\n", was); gruen++; }
    else {
        printf("  [ROT]  %s%s%s\n", was, hinweis ? "  -- " : "",
               hinweis ? hinweis : "");
        rot++;
    }
}

/* Ein DMK-Kopf, wie `dmk_open()` ihn dereferenziert: 16 Byte, danach
 * nichts. Die Sonde bekommt ihn mit einer FREI gewaehlten Dateigroesse,
 * weil genau das der Vertrag ist (`size` und `file_size` getrennt). */
static void kopf(uint8_t h[16], uint8_t prot, uint8_t tracks,
                 uint16_t tlen, uint8_t opts)
{
    memset(h, 0, 16);
    h[0] = prot;
    h[1] = tracks;
    h[2] = (uint8_t)(tlen & 0xFF);
    h[3] = (uint8_t)(tlen >> 8);
    h[4] = opts;
}

static size_t ergibt(uint8_t tracks, uint16_t tlen, uint8_t opts)
{
    const size_t seiten = (opts & 0x10) ? 1u : 2u;
    return 16u + (size_t)tracks * seiten * (size_t)tlen;
}

static uint8_t *lies(const char *pfad, size_t *n, long *fs)
{
    FILE *f = fopen(pfad, "rb");
    uint8_t *b;
    long gr;
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    gr = ftell(f);
    if (gr <= 0 || fseek(f, 0, SEEK_SET) != 0) { fclose(f); return NULL; }
    b = (uint8_t *)malloc(65536);
    if (!b) { fclose(f); return NULL; }
    *n = fread(b, 1, 65536, f);
    *fs = gr;
    fclose(f);
    return b;
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_dmk;
    uint8_t h[16];
    char d1[300];

    printf("\nDMKs Sonde gegen MAMEs identify() (MF-1151)\n\n");

    /* ── 1. Der Befund aus MF-1150: das flache ST-Abbild ──────────── */
    {
        char pfad[600];
        size_t n = 0;
        long fs = 0;
        uint8_t *b;
        snprintf(pfad, sizeof pfad, "%s/hxcfe_720k.st", UFT_CORPUS_DIR);
        b = lies(pfad, &n, &fs);
        if (!b) {
            printf("  [SKIP] hxcfe_720k.st fehlt im Korpus\n");
        } else {
            int k = -1;
            bool ja = p->probe(b, n, (size_t)fs, &k);
            snprintf(d1, sizeof d1,
                     "probe = %d, Konfidenz %d; erste Byte %02X %02X "
                     "%02X %02X %02X %02X %02X",
                     (int)ja, k, b[0], b[1], b[2], b[3], b[4], b[5], b[6]);
            pruefe("ein FLACHES 737 280-Byte-ST-Abbild wird abgewiesen — "
                   "seine ersten Byte sind UFTs Marke \"UFT-K\", nicht ein "
                   "DMK-Kopf (Byte 5..15 sind nicht null, und tlen 18005 "
                   "liegt ueber MAMEs 0x3FFF)", !ja, d1);
            free(b);
        }
    }

    /* ── 2. Das echte Korpus-DMK bleibt lesbar ────────────────────── */
    {
        char pfad[600];
        size_t n = 0;
        long fs = 0;
        uint8_t *b;
        snprintf(pfad, sizeof pfad, "%s/hxcfe_pc160.dmk", UFT_CORPUS_DIR);
        b = lies(pfad, &n, &fs);
        if (!b) {
            printf("  [SKIP] hxcfe_pc160.dmk fehlt im Korpus\n");
        } else {
            int k = -1;
            bool ja = p->probe(b, n, (size_t)fs, &k);
            snprintf(d1, sizeof d1, "probe = %d, Konfidenz %d (Datei %ld "
                     "Byte, tracks %u, tlen %u)", (int)ja, k, fs,
                     (unsigned)b[1], (unsigned)(b[2] | (b[3] << 8)));
            pruefe("das echte DMK aus dem Korpus wird weiter angenommen — "
                   "eine Sonde, die ihren eigenen Leser aussperrt, waere "
                   "die andere Haelfte des Fehlers", ja, d1);
            /* BERICHTIGT MF-1153. Hier stand `k >= STRUCT_MIN && k <
             * MAGIC_MIN`, also 50..79 — und damit hat diese Zusage
             * MEINE eigene Handzahl festgeschrieben. MF-1151 hatte 100
             * auf 75 gesenkt, weil DMK keine Kennung hat; die 75 war
             * aber ebenso vergeben wie die 100 davor, nur kleiner.
             *
             * Seit der Sonden-Doktrin (`docs/SONDEN_DOKTRIN.md`) wird
             * die Zahl abgeleitet, und ohne Kennung ist die Obergrenze
             * **45** — Band „nur die Groesse". DMK legt STRUKTUR (elf
             * Nullbyte an fester Stelle) + GEOMETRIE + SELBSTKONSISTENZ
             * (Groesse trifft die Spurarithmetik) vor, das waere 50, und
             * die Klemme haelt es bei 45. */
            pruefe("und es beansprucht ohne Kennung hoechstens das Band "
                   "\"nur die Groesse\" (30..49): DMK hat keine Kennung, "
                   "gelesen sind elf Nullbyte, ein Flagbyte und eine "
                   "aufgehende Spurarithmetik — nach der Doktrin sind "
                   "das drei Belege ohne Kennung, also 45 statt 50",
                   ja && k >= UFT_PROBE_CONF_SIZE_MIN
                      && k < UFT_PROBE_CONF_STRUCT_MIN, d1);
            free(b);
        }
    }

    /* ── 3. Tor 1: das Schutzbyte ist eine BEDINGUNG ──────────────── */
    {
        int k = -1;
        bool ja;
        kopf(h, (uint8_t)'R', 40, 0x1900, 0x00);
        ja = p->probe(h, sizeof h, ergibt(40, 0x1900, 0x00), &k);
        snprintf(d1, sizeof d1, "probe = %d (%d)", (int)ja, k);
        pruefe("Schutzbyte 'R' (0x52) wird abgewiesen — MAME: "
               "`header[0] != 0x00 && header[0] != 0xff -> return 0`; "
               "UFT gab dafuer nur die +10 nicht", !ja, d1);
    }
    {
        int k = -1;
        bool ja;
        kopf(h, 0xFF, 40, 0x1900, 0x00);
        ja = p->probe(h, sizeof h, ergibt(40, 0x1900, 0x00), &k);
        snprintf(d1, sizeof d1, "probe = %d (%d)", (int)ja, k);
        pruefe("0xFF ist der ZWEITE erlaubte Wert und wird angenommen — "
               "ohne diese Gegenprobe waere \"nur 0x00\" ebenso gruen",
               ja, d1);
    }

    /* ── 4. Tor 2: Byte 5..15 muessen NULL sein ───────────────────── */
    {
        int k = -1;
        bool ja;
        kopf(h, 0x00, 40, 0x1900, 0x00);
        h[7] = 0x01;                     /* mitten in der Reserve */
        ja = p->probe(h, sizeof h, ergibt(40, 0x1900, 0x00), &k);
        snprintf(d1, sizeof d1, "probe = %d (%d)", (int)ja, k);
        pruefe("ein gesetztes Bit in Byte 5..11 wird abgewiesen — UFT "
               "liess es durch und gab nur die +5 nicht", !ja, d1);
    }
    {
        int k = -1;
        bool ja;
        kopf(h, 0x00, 40, 0x1900, 0x00);
        h[13] = 0x14;                    /* Byte 12..15 las UFT GAR NICHT */
        ja = p->probe(h, sizeof h, ergibt(40, 0x1900, 0x00), &k);
        snprintf(d1, sizeof d1, "probe = %d (%d)", (int)ja, k);
        pruefe("ein gesetztes Bit in Byte 12..15 wird abgewiesen — MAME "
               "prueft bis 0x0F, UFT hat diese vier Byte nie angesehen, "
               "und genau dort steht in der ST-Datei `13 14 15 16`",
               !ja, d1);
    }

    /* ── 5. Tor 3: die Spurlaenge, mit beiden Raendern ────────────── */
    {
        int k = -1;
        bool ja;
        kopf(h, 0x00, 1, 0x4000, 0x10);   /* 16384 = MAMEs Grenze + 1 */
        ja = p->probe(h, sizeof h, 737280u, &k);
        snprintf(d1, sizeof d1, "probe = %d (%d)", (int)ja, k);
        pruefe("Spurlaenge 16384 wird abgewiesen — MAMEs Obergrenze ist "
               "0x3FFF; UFTs 20000 nahm 3617 Werte an, die MAME verwirft",
               !ja, d1);
    }
    {
        int k = -1;
        bool ja;
        kopf(h, 0x00, 40, 0x3FFF, 0x00);
        ja = p->probe(h, sizeof h, ergibt(40, 0x3FFF, 0x00), &k);
        snprintf(d1, sizeof d1, "probe = %d (%d)", (int)ja, k);
        pruefe("Spurlaenge 16383 wird angenommen — der Rand selbst, ohne "
               "den waere die Obergrenze nicht gemessen", ja, d1);
    }
    {
        int k = -1;
        bool ja;
        kopf(h, 0x00, 40, 128, 0x00);
        ja = p->probe(h, sizeof h, ergibt(40, 128, 0x00), &k);
        snprintf(d1, sizeof d1, "probe = %d (%d)", (int)ja, k);
        pruefe("Spurlaenge 128 wird abgewiesen — hier weicht UFT von MAME "
               "BEWUSST ab: MAME nimmt ab 0x80, UFT bleibt bei 1000, weil "
               "eine untere Schranke nur WEITER machen kann und keine "
               "Diskette mit 128-Byte-Spuren dieser Leser liest",
               !ja, d1);
    }

    /* ── 6. Die Groessenarithmetik, in beide Richtungen ───────────── */
    {
        int k = -1, k2 = -1;
        bool ja, ja2;
        const size_t soll = ergibt(40, 0x1900, 0x00);
        kopf(h, 0x00, 40, 0x1900, 0x00);
        ja  = p->probe(h, sizeof h, soll, &k);
        ja2 = p->probe(h, sizeof h, soll - 1, &k2);
        snprintf(d1, sizeof d1, "exakt: %d (%d); ein Byte kuerzer: %d (%d)",
                 (int)ja, k, (int)ja2, k2);
        pruefe("40 x 2 x 0x1900 wird angenommen und ein Byte darunter "
               "abgewiesen — die Arithmetik aus MF-447 bleibt", ja && !ja2,
               d1);
    }
    {
        int k = -1, k2 = -1;
        const size_t soll = ergibt(40, 0x1900, 0x00);
        kopf(h, 0x00, 40, 0x1900, 0x00);
        (void)p->probe(h, sizeof h, soll, &k);
        (void)p->probe(h, sizeof h, soll + 4096, &k2);
        snprintf(d1, sizeof d1, "exakt %d, mit 4096 Byte Ueberhang %d",
                 k, k2);
        pruefe("die exakte Groesse traegt MEHR Konfidenz als eine mit "
               "Ueberhang — MAME unterscheidet genauso "
               "(`FIFID_HINT|FIFID_SIZE` gegen `FIFID_HINT`)",
               k > k2 && k2 > 0, d1);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
