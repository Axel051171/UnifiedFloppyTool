/**
 * @file test_ssd_katalog_zehn_bit.c
 * @brief Das DFS-Sektorzahlfeld ist ZEHN Bit breit, und die Sonde liest
 *        ACHT (MF-1152)
 *
 * ── Der Befund, und er ist gemessen ──────────────────────────────────
 *
 * A4 Runde 2 (MF-1150) hat gemessen: `uft_disk_open()` gibt
 * `tests/corpus_free/floptool_jv1_80spuren.jv1` — eine TRS-80-Diskette
 * von MAMEs floptool — an **SSD** mit Konfidenz **85**, also im Band
 * „Merkmal getroffen" (MF-729). Der `jv1`-Leser bekommt sein eigenes
 * Abbild nicht, und `tan` (dieselbe Datei, P3-365) auch nicht.
 *
 * MF-1146 hatte diese Sonde eigens verschaerft, weil sie auf reinem
 * TEXT 82 meldete: das Tor zum Merkmalsband verlangt seither ZWEI
 * Katalogfelder statt eines. Dieses Abbild trifft beide.
 *
 * ── Warum beide Felder es nicht aufhalten ────────────────────────────
 *
 * Gemessen am Byteinhalt:
 *
 *     floptool_jv1_80spuren.jv1   0x105 = 0x56   0x106 = 0x31   0x107 = 0x20
 *
 * Die alte Pruefung war `data[0x107] == 0x90 || == 0x20 || == 0xA0`
 * plus `(data[0x106] >> 4) <= 3 && data[0x107] > 0`. Byte 0x107 ist
 * hier **0x20, das Leerzeichen** — genau das Byte, das MF-1146 als
 * Ursache benannt hat —, und 0x31 ('1') hat das obere Nibble 3, also
 * gilt auch die zweite Bedingung.
 *
 * **Und die zwei Felder sind nicht unabhaengig:** die zweite Bedingung
 * verlangt zusaetzlich `data[0x107] > 0`, was die erste schon
 * garantiert (0x90, 0x20 und 0xA0 sind alle groesser als null). Neu
 * geprueft wird also allein `data[0x106] <= 0x3F`. Die Formulierung
 * „zwei unabhaengige Katalogfelder" aus MF-1146 traegt damit nicht —
 * das ist meine eigene Angabe, und sie wird hier berichtigt.
 *
 * ── Die Ursache: das Feld ist zehn Bit breit ─────────────────────────
 *
 * Die Sektorzahl eines DFS-Katalogs steht NICHT in einem Byte. Sie
 * steht in den unteren **zwei Bit von 0x106** und den acht Bit von
 * 0x107, und das obere Nibble von 0x106 ist die Bootoption:
 *
 *     Sektoren   = ((data[0x106] & 0x03) << 8) | data[0x107]
 *     Bootoption = (data[0x106] >> 4) & 0x03
 *
 * **Und dieses Wissen stand schon im Baum — in einem TESTKOMMENTAR
 * neben einer Sonde, die es ignoriert.**
 * `tests/test_ssd_hadfs_nicht_dfs.c` (MF-836) schreibt woertlich:
 *
 *     400 Sektoren = 0x190 -> High-Bits 0x01 bei 0x106, Low 0x90 bei 0x107
 *
 * und setzt seine Pruefdatei genau so. Das ist woertlich die Gestalt
 * von MF-1020 bei `mfi`: „das Wissen stand im Kommentar und nicht im
 * Code".
 *
 * Mit den zehn Bit gelesen trennt es sofort:
 *
 *     echtes DFS      0x106=0x03 0x107=0x20  ->  800 Sektoren
 *     floptool-JV1    0x106=0x31 0x107=0x20  ->  288 Sektoren
 *
 * 288 ist **kein Vielfaches von 10**, und eine DFS-Diskette hat zehn
 * Sektoren je Spur — `ssd_detect()` rechnet selbst mit `fs / 2560`.
 * Dazu sind 288 x 256 = 73 728 Byte **kleiner als die Datei** (204 800):
 * ein Katalog kann nicht weniger Sektoren ansagen, als die Diskette
 * hat.
 *
 * ── Die Referenz: ein ECHTER DFS-Katalog ─────────────────────────────
 *
 * `tools/uft-scout/work/DiscImageManager/Blank Images/Acorn DFS/`
 * (Gerald Holdsworth, **GPL-3**; seit MF-693 als Oracle registriert,
 * ausdruecklich als DATENQUELLE — im Baum nicht gebaut, benutzt werden
 * seine formatierten Leer-Abbilder). Gemessen:
 *
 *     BlankSingle.ssd   512 Byte  sha256 6b1a6d51…
 *     BlankDouble.dsd  3072 Byte  sha256 74e5ed92…
 *
 *     Sektor 1, 0x100..0x107:  00 00 00 00 00 00 03 20
 *
 * also 0 Dateien, 800 Sektoren, Bootoption 0. **Beide Dateien sind zu
 * kurz fuer `ssd_detect()`** (512 bzw. 3072 Byte, verlangt sind 40, 80
 * oder 160 Spursaetze) — sie kommen deshalb NICHT als Pruefdatei in
 * Frage, und genau deshalb steht hier kein Korpuseintrag. Was sie
 * liefern, sind die FELDWERTE, gegen die das Raster geprueft ist.
 *
 * ── Eichung 1 hat meinen eigenen Fehler gefangen ─────────────────────
 *
 * Die erste Fassung der Korrektur gab die zweite Stufe (60) auch fuer
 * `datei_ok && boot_ok` — zwei Bereichspruefungen. Auf einem
 * **NULLPUFFER** sind beide wahr (`0 % 8 == 0` und `0 <= 3`), und damit
 * meldete `ssd` dort 60. `tests/test_probe_confidence_on_zeros.c`
 * (Eichung 1, MF-729) ist beim ersten vollen Lauf danach rot geworden
 * und hat es gemeldet: auf Nullen darf nichts ab 50 melden.
 *
 * Das gehoert hierher, weil es der Beleg dafuer ist, dass die Eichungen
 * tragen — und weil es dieselbe Lehre in eigener Sache ist: zwei
 * Pruefungen, die die LEERE Datei erfuellt, sind keine Struktur. Die
 * zweite Stufe haengt deshalb allein an der Sektorzahl, die von null
 * verschieden, ein Vielfaches von zehn und gross genug sein muss.
 *
 * ── Was dieser Test NICHT behauptet ──────────────────────────────────
 *
 * * Nichts darueber, ob 85 fuer DFS die richtige Zahl IST. DFS hat
 *   keine Kennung, und nach MF-729 gehoert das Band 80..100 einem
 *   getroffenen Merkmal. Nach dieser Korrektur besteht der Anspruch aus
 *   einem Quervergleich zwischen Katalog und Dateigroesse — das ist
 *   staerker als eine Bereichspruefung und bleibt keine Signatur. Die
 *   Bandfrage ist bei `dmk` (MF-1151) zugunsten von 75 entschieden
 *   worden und steht fuer `ssd` in P3-405.
 * * Nichts ueber eine echte DFS-Diskette MIT Dateien. Im Korpus liegt
 *   keine; `gw_ssd.img` ist ein roher gw-Abzug ohne Verzeichnis und
 *   meldet darum zu Recht 30.
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

extern const uft_format_plugin_t uft_format_plugin_ssd;

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

#define SEK  256u
#define SPT   10u

/* Ein Abbild in einer Groesse, die `ssd_detect()` annimmt, mit frei
 * gesetzten Katalogfeldern. `spuren` mal 10 mal 256 Byte. */
static uint8_t *baue(unsigned spuren, uint8_t dateien, uint8_t hoch,
                     uint8_t niedrig, size_t *groesse)
{
    const size_t n = (size_t)spuren * SPT * SEK;
    uint8_t *b = (uint8_t *)calloc(1, n);
    if (!b) return NULL;
    b[0x105] = dateien;
    b[0x106] = hoch;
    b[0x107] = niedrig;
    *groesse = n;
    return b;
}

static int konfidenz(const uint8_t *b, size_t n)
{
    int k = -1;
    if (!uft_format_plugin_ssd.probe(b, n, n, &k)) return 0;
    return k;
}

int main(void)
{
    char d1[260];

    printf("\nDFS-Katalog: zehn Bit, nicht acht (MF-1152)\n\n");

    /* ── 1. Der Befund: das floptool-JV1 ──────────────────────────── */
    {
        char pfad[600];
        FILE *f;
        snprintf(pfad, sizeof pfad, "%s/floptool_jv1_80spuren.jv1",
                 UFT_CORPUS_DIR);
        f = fopen(pfad, "rb");
        if (!f) {
            printf("  [SKIP] floptool_jv1_80spuren.jv1 fehlt im Korpus\n");
        } else {
            uint8_t *b = (uint8_t *)malloc(204800);
            size_t n = b ? fread(b, 1, 204800, f) : 0;
            fclose(f);
            if (!b || n < 0x108) {
                printf("  [SKIP] Pruefdatei nicht lesbar\n");
            } else {
                const unsigned zehn =
                    ((unsigned)(b[0x106] & 0x03) << 8) | b[0x107];
                int k = konfidenz(b, n);
                snprintf(d1, sizeof d1,
                         "0x105=0x%02X 0x106=0x%02X 0x107=0x%02X -> "
                         "%u Sektoren; Konfidenz %d",
                         b[0x105], b[0x106], b[0x107], zehn, k);
                pruefe("eine TRS-80-Diskette erreicht das Merkmalsband "
                       "NICHT mehr: ihr Katalogfeld sagt 288 Sektoren, "
                       "und das ist weder ein Vielfaches von 10 noch "
                       "gross genug fuer 204 800 Byte",
                       k < UFT_PROBE_CONF_MAGIC_MIN, d1);
            }
            free(b);
        }
    }

    /* ── 2. Die Gegenprobe: die ECHTEN DFS-Feldwerte ──────────────── */
    {
        size_t n = 0;
        /* 0x03/0x20 = 800 Sektoren, 0 Dateien — gemessen an
         * DiscImageManagers BlankSingle.ssd und BlankDouble.dsd. 800
         * Sektoren sind 80 Spuren, also die passende Bildgroesse. */
        uint8_t *b = baue(80, 0x00, 0x03, 0x20, &n);
        if (!b) { pruefe("Speicher", 0, NULL); }
        else {
            int k = konfidenz(b, n);
            snprintf(d1, sizeof d1, "%zu Byte, 800 Sektoren, Konfidenz %d",
                     n, k);
            pruefe("die Feldwerte eines ECHTEN DFS-Katalogs (0x00/0x03/"
                   "0x20 = 0 Dateien, 800 Sektoren, Bootoption 0) werden "
                   "weiter voll anerkannt — ohne diese Zusage waere die "
                   "Verschaerfung die andere Haelfte des Fehlers",
                   k >= UFT_PROBE_CONF_MAGIC_MIN, d1);
            free(b);
        }
    }

    /* ── 3. Die zehn Bit einzeln: dieselbe UNTERE Haelfte, anderes
     *      Ergebnis. Genau das konnte die alte Sonde nicht sehen. ─── */
    {
        size_t n1 = 0, n2 = 0;
        uint8_t *a = baue(80, 0x00, 0x03, 0x20, &n1);   /* 800 */
        uint8_t *b = baue(80, 0x00, 0x31, 0x20, &n2);   /* 288 */
        if (!a || !b) { pruefe("Speicher", 0, NULL); }
        else {
            int ka = konfidenz(a, n1), kb = konfidenz(b, n2);
            snprintf(d1, sizeof d1, "0x03|0x20 -> %d, 0x31|0x20 -> %d",
                     ka, kb);
            pruefe("zwei Abbilder mit DEMSELBEN Byte 0x107 (0x20) und "
                   "verschiedenem 0x106 werden verschieden beurteilt — "
                   "die Sonde liest jetzt die oberen zwei Bit mit",
                   ka >= UFT_PROBE_CONF_MAGIC_MIN
                   && kb < UFT_PROBE_CONF_MAGIC_MIN, d1);
        }
        free(a); free(b);
    }

    /* ── 4. Das Vielfache von 10 ──────────────────────────────────── */
    {
        size_t n = 0;
        /* 0x03/0x25 = 805 Sektoren: gross genug, aber kein Vielfaches
         * von 10 — eine DFS-Diskette hat zehn Sektoren je Spur. */
        uint8_t *b = baue(80, 0x00, 0x03, 0x25, &n);
        if (!b) { pruefe("Speicher", 0, NULL); }
        else {
            int k = konfidenz(b, n);
            snprintf(d1, sizeof d1, "805 Sektoren, Konfidenz %d", k);
            pruefe("805 Sektoren werden nicht als Katalog anerkannt — "
                   "DFS hat zehn Sektoren je Spur, und `ssd_detect()` "
                   "rechnet selbst mit `fs / 2560`",
                   k < UFT_PROBE_CONF_MAGIC_MIN, d1);
            free(b);
        }
    }

    /* ── 5. Die Ansage darf nicht KLEINER sein als die Diskette ───── */
    {
        size_t n = 0;
        /* 0x01/0x90 = 400 Sektoren in einem 80-Spur-Abbild (800). */
        uint8_t *b = baue(80, 0x00, 0x01, 0x90, &n);
        if (!b) { pruefe("Speicher", 0, NULL); }
        else {
            int k = konfidenz(b, n);
            snprintf(d1, sizeof d1, "400 angesagt, %zu Byte Datei "
                     "(= 800 Sektoren), Konfidenz %d", n, k);
            pruefe("400 angesagte Sektoren in einem 800-Sektor-Abbild "
                   "werden nicht anerkannt — ein Katalog kann nicht "
                   "weniger Sektoren nennen, als die Datei enthaelt",
                   k < UFT_PROBE_CONF_MAGIC_MIN, d1);
            free(b);
        }
    }
    {
        size_t n = 0;
        /* Dieselben 400 in einem 40-Spur-Abbild: jetzt stimmt es. */
        uint8_t *b = baue(40, 0x00, 0x01, 0x90, &n);
        if (!b) { pruefe("Speicher", 0, NULL); }
        else {
            int k = konfidenz(b, n);
            snprintf(d1, sizeof d1, "400 angesagt, %zu Byte, Konfidenz %d",
                     n, k);
            pruefe("dieselben 400 Sektoren in einem 40-Spur-Abbild werden "
                   "anerkannt — der Quervergleich prueft die Richtung, "
                   "nicht die Zahl", k >= UFT_PROBE_CONF_MAGIC_MIN, d1);
            free(b);
        }
    }

    /* ── 6. Die Dateizahl ist ein Vielfaches von ACHT ─────────────── */
    {
        size_t n = 0;
        /* 0x105 = 0x56 = 86: das Byte des floptool-JV1. Ein
         * DFS-Katalogeintrag ist 8 Byte lang, also kann dort nur ein
         * Vielfaches von 8 stehen, hoechstens 31 * 8 = 248. */
        uint8_t *b = baue(80, 0x56, 0x03, 0x20, &n);
        if (!b) { pruefe("Speicher", 0, NULL); }
        else {
            int k = konfidenz(b, n);
            snprintf(d1, sizeof d1, "0x105 = 0x56 (86), Konfidenz %d", k);
            pruefe("eine Dateizahl von 86 Byte wird nicht anerkannt — ein "
                   "DFS-Eintrag ist ACHT Byte lang, dort steht "
                   "Dateizahl x 8 und hoechstens 248",
                   k < UFT_PROBE_CONF_MAGIC_MIN, d1);
            free(b);
        }
    }
    {
        size_t n = 0;
        uint8_t *b = baue(80, 248, 0x03, 0x20, &n);   /* 31 Dateien */
        if (!b) { pruefe("Speicher", 0, NULL); }
        else {
            int k = konfidenz(b, n);
            snprintf(d1, sizeof d1, "0x105 = 248 (31 Dateien), Konfidenz %d",
                     k);
            pruefe("248 — die Obergrenze von 31 Dateien — wird anerkannt; "
                   "ohne diesen Rand waere \"nur 0\" ebenso gruen",
                   k >= UFT_PROBE_CONF_MAGIC_MIN, d1);
            free(b);
        }
    }

    /* ── 7. Die Bootoption bleibt, was sie war ────────────────────── */
    {
        size_t n = 0;
        uint8_t *b = baue(80, 0x00, 0xC3, 0x20, &n);   /* Nibble 0xC */
        if (!b) { pruefe("Speicher", 0, NULL); }
        else {
            int k = konfidenz(b, n);
            snprintf(d1, sizeof d1, "0x106 = 0xC3 (Bootoption 12), "
                     "Konfidenz %d", k);
            pruefe("eine Bootoption von 12 wird nicht anerkannt — es gibt "
                   "vier (0..3), und diese Pruefung hatte die Sonde schon",
                   k < UFT_PROBE_CONF_MAGIC_MIN, d1);
            free(b);
        }
    }

    /* ── 8. Und die Groesse allein traegt weiter ihr eigenes Band ─── */
    {
        size_t n = 0;
        uint8_t *b = baue(80, 0x56, 0x5A, 0x5A, &n);   /* alles daneben */
        if (!b) { pruefe("Speicher", 0, NULL); }
        else {
            int k = konfidenz(b, n);
            snprintf(d1, sizeof d1, "Konfidenz %d", k);
            pruefe("ein Abbild mit passender GROESSE und unbrauchbarem "
                   "Katalog wird weiter angenommen, aber nur im Band "
                   "\"nur die Groesse\" (30..49) — ein Befund darf den "
                   "Zugang nicht versperren (MF-830)",
                   k >= 30 && k < UFT_PROBE_CONF_STRUCT_MIN, d1);
            free(b);
        }
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
