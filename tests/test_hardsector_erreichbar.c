/**
 * @file test_hardsector_erreichbar.c
 * @brief Die Sonde von `hardsector` konnte NIE zustimmen (MF-1059)
 *
 * ── Der Befund, gemessen am Quelltext vor dem Test ──────────────────────
 *
 * `hardsector_probe_plugin()` verwirft die Dateigroesse und reicht die
 * PUFFERgroesse weiter:
 *
 *     static bool hardsector_probe_plugin(const uint8_t *data, size_t size,
 *                                         size_t file_size, int *confidence) {
 *         (void)file_size;
 *         return uft_hardsector_probe(data, size, confidence);
 *     }
 *
 * Und `uft_hardsector_probe()` vergleicht sein Groessenargument mit den
 * fuenf Eintraegen der `HS_*`-Tafel. Die sind:
 *
 *     35 x 1 x 10 x 256 =    89 600
 *     40 x 1 x 16 x 256 =   163 840
 *     77 x 1 x 26 x 128 =   256 256
 *     77 x 2 x 26 x 128 =   512 512
 *     77 x 2 x 26 x 256 = 1 025 024
 *
 * **Jede einzelne ist groesser als `UFT_PROBE_BUFFER_SIZE`** (65 536,
 * `include/uft/uft_format_plugin.h:995`) — und der Produktionspfad
 * deckelt den Puffer genau dort (`src/core/uft_format_plugin.c:365-366`):
 *
 *     size_t probe_size = ((size_t)fs < UFT_PROBE_BUFFER_SIZE)
 *                             ? (size_t)fs : UFT_PROBE_BUFFER_SIZE;
 *
 * Die Bedingung konnte also nie zutreffen. Das Plugin war ueber die
 * Erkennung **unerreichbar** — dieselbe Falle wie MF-1029 (`myz80`, wo
 * ein Groessenrueckfall toter Code war), MF-1030, MF-1031, MF-1036,
 * MF-1039 (`cpm`, wo es der ganze Erkenner war) und MF-1054. Zum
 * **siebten** Mal, und hier zum zweiten Mal in der reinsten Gestalt:
 * nicht ein Teil des Erkenners ist tot, sondern **alle fuenf** Eintraege.
 *
 * ── Der zweite Befund, den der erste verdeckt hat ───────────────────────
 *
 * `uft_hardsector_probe()` beginnt mit
 *
 *     (void)data;  // Content doesn't matter for hard-sector detection
 *
 * und meldet dann Konfidenz **50**. Nach MF-729 ist 50..79 das Band
 * „Struktur gelesen", und wer es beansprucht, muss **>= 95 % zufaelliger
 * Puffer abweisen** (`tests/test_probe_confidence_on_random.c`). Gelesen
 * wird hier kein einziges Byte — erkannt ist ausschliesslich die
 * Dateigroesse, also Band **30..49** („nur die Groesse").
 *
 * **Und die Eichung konnte das nicht bemerken**, weil sie mit Puffern von
 * `UFT_PROBE_BUFFER_SIZE` arbeitet: die Sonde sagte dort ohnehin immer
 * `false`, die Abweisungsquote war 100 %, der Test gruen. Der eine Fehler
 * hat den anderen verborgen — dieselbe Gestalt wie MF-1031, wo ein
 * `uint8_t`-Ueberlauf eine falsche Laengenpruefung verdeckte.
 *
 * Deshalb liegen beide Berichtigungen in EINEM Schritt: die Sonde
 * erreichbar zu machen, ohne die Konfidenz zu senken, wuerde die
 * Eichung aus MF-729 verletzen. MF-706 verlangt diese Reihenfolge
 * ausdruecklich — erst die Geometrie, dann die Groessenquelle.
 *
 * ── Die Gegenprobe, die den Fix ISOLIERT ────────────────────────────────
 *
 * Die Lehre aus MF-1029 war, dass eine Gegenprobe, die die Dateigroesse
 * als Puffergroesse uebergibt, den Fehler NICHT sieht. Zusage 5 haelt
 * deshalb die Umkehrung fest: ein wirklich 163 840 Byte grosser Puffer
 * mit Dateigroesse 4096 muss **abgewiesen** werden. Ohne sie waere
 * „nimm einfach beide Groessen" eine bestehende Loesung, und die
 * Groessenquelle bliebe unbestimmt.
 *
 * ── Was dieser Test AUSDRUECKLICH NICHT behauptet ───────────────────────
 *
 * **Die Stufe aendert sich nicht.** `hardsector` bleibt auf T3. Die drei
 * 8-Zoll-Eintraege der Tafel sind die **weichsektorierten** IBM-3740- und
 * System-34-Geometrien (26 x 128 bzw. 26 x 256) und tragen den Namen
 * „Hartsektor" zu Unrecht; MF-1058 hat das an `hardsector_tool`
 * (Apache-2.0, im Baum) gemessen und als Unterscheidung in P3-340
 * festgehalten: 32 physische Sektorloecher sind eine Eigenschaft des
 * MEDIUMS, die logische Aufteilung (Wang 2200: 16 x 256 auf 77 Spuren)
 * eine des RECHNERS. Solange diese Eigentuemer-Entscheidung offensteht,
 * waere eine Hebung das Verbreiten der falschen Beschriftung.
 *
 * Die gesenkte Konfidenz ist genau das, was das verhindert: das Plugin
 * sagt jetzt, was es weiss — die Dateigroesse — und nicht mehr.
 *
 * Ebenfalls nicht geprueft: der Schreibpfad (`uft_hardsector_write()` hat
 * seit MF-930 keinen Aufrufer, P3-204) und die Sektornummern, die aus
 * `geometry.first_sector = 1` folgen — ohne belegte Referenz je Rechner
 * ist das keine pruefbare Zusage, sondern Teil von P3-340.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"   /* uft_track_release — ohne diesen Header eine
                              * implizite Deklaration, und die ist auf
                              * macOS-Clang ein FEHLER, nicht nur eine
                              * Warnung (gcc laesst sie durch). */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern const uft_format_plugin_t uft_format_plugin_hardsector;

/* Die Geometrie, an der gelesen wird: 5.25" 16-Sektor aus der HS_*-Tafel.
 * Gewaehlt, weil sie die kleinste der fuenf ist, die der Leser
 * vollstaendig belegt — 640 Sektoren, 163 840 Byte. */
#define ZYL      40u
#define KOEPFE    1u
#define SEKT     16u
#define SGR     256u
#define GESAMT  (ZYL * KOEPFE * SEKT * SGR)   /* 163 840 */

/* Alle fuenf Groessen der Tafel, aus ihren Faktoren gerechnet statt
 * abgeschrieben. */
static const size_t ALLE_FUENF[5] = {
    35u * 1u * 10u * 256u,   /*    89 600 */
    40u * 1u * 16u * 256u,   /*   163 840 */
    77u * 1u * 26u * 128u,   /*   256 256 */
    77u * 2u * 26u * 128u,   /*   512 512 */
    77u * 2u * 26u * 256u,   /* 1 025 024 */
};

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

static void pfad_bauen(char *p, size_t n, const char *name)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(p, n, "%s/uft_hs_%s.img", d, name);
}

/* Jeder Sektor benennt sich selbst — so sagt ein Leseergebnis nicht nur,
 * DASS etwas kam, sondern WELCHE Stelle geliefert wurde (MF-1020). */
static void sektor_inhalt(unsigned c, unsigned h, unsigned s, uint8_t *b)
{
    char k[24];
    unsigned i;
    snprintf(k, sizeof k, "UFT-K T%02u H%u S%02u ", c, h, s);
    memcpy(b, k, 16);
    for (i = 16; i < SGR; i++)
        b[i] = (uint8_t)((c * 37u + h * 101u + s * 7u + (i - 16u) * 3u) & 0xFFu);
}

static int schreibe_abbild(const char *pfad)
{
    uint8_t b[SGR];
    unsigned c, h, s;
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    for (c = 0; c < ZYL; c++)
        for (h = 0; h < KOEPFE; h++)
            for (s = 0; s < SEKT; s++) {
                sektor_inhalt(c, h, s, b);
                if (fwrite(b, 1, SGR, f) != SGR) { fclose(f); return 0; }
            }
    fclose(f);
    return 1;
}

/* Ruft die Sonde GENAU so, wie der Produktionspfad es tut:
 * `src/core/uft_format_plugin.c:365-366` liest hoechstens
 * UFT_PROBE_BUFFER_SIZE Byte in den Puffer und uebergibt die WIRKLICHE
 * Dateigroesse als drittes Argument. */
static int sonde_wie_produktion(const uint8_t *datei, size_t dateigroesse,
                                int *konf)
{
    const uft_format_plugin_t *p = &uft_format_plugin_hardsector;
    size_t puffer = (dateigroesse < (size_t)UFT_PROBE_BUFFER_SIZE)
                        ? dateigroesse : (size_t)UFT_PROBE_BUFFER_SIZE;
    *konf = -1;
    return p->probe(datei, puffer, dateigroesse, konf) ? 1 : 0;
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_hardsector;
    uint8_t *abbild;
    char pfad[600], det[260], erster[200];
    unsigned c, s, i, gesehen = 0, gleich = 0, falsch = 0;
    int h, konf, ok, angenommen = 0;
    uft_disk_t disk;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("HardSector: die Sonde muss auf dem Produktionspfad "
           "erreichbar sein - MF-1059\n");
    printf("=============================================="
           "=============================\n");

    abbild = (uint8_t *)malloc(GESAMT);
    if (!abbild) { printf("kein Speicher\n"); return 1; }
    for (c = 0; c < ZYL; c++)
        for (h = 0; h < (int)KOEPFE; h++)
            for (s = 0; s < SEKT; s++)
                sektor_inhalt(c, (unsigned)h, s,
                              abbild + ((c * KOEPFE + (unsigned)h) * SEKT + s) * SGR);

    /* 1 — der Kern des Befunds. Alle fuenf Groessen liegen ueber dem
     *     Sondenpuffer, also entscheidet allein, ob `file_size` benutzt
     *     wird. Vor MF-1059 antwortete die Sonde hier 0. */
    ok = sonde_wie_produktion(abbild, GESAMT, &konf);
    snprintf(det, sizeof det,
             "probe=%d konf=%d (Puffer %u von %u Byte)", ok, konf,
             (unsigned)UFT_PROBE_BUFFER_SIZE, (unsigned)GESAMT);
    pruefe("163 840 Byte werden auf dem PRODUKTIONSPFAD angenommen - "
           "der Puffer ist nur 65 536 Byte gross", ok, det);

    /* 2 — die Konfidenz gehoert in das Band, das die Sonde belegen kann.
     *     `(void)data` liest nichts; erkannt ist die Dateigroesse. */
    snprintf(det, sizeof det, "konf=%d, erwartet 30..49 (MF-729)", konf);
    pruefe("Konfidenz im Band 30..49 - erkannt ist NUR die Groesse",
           konf >= 30 && konf <= 49, det);

    /* 3 — alle fuenf Eintraege der Tafel sind erreichbar, nicht nur der
     *     eine, an dem gelesen wird. */
    erster[0] = 0;
    for (i = 0; i < 5; i++) {
        size_t g = ALLE_FUENF[i];
        uint8_t kopf[UFT_PROBE_BUFFER_SIZE];
        memset(kopf, 0, sizeof kopf);
        konf = -1;
        if (p->probe(kopf, sizeof kopf, g, &konf)
            && konf >= 30 && konf <= 49) angenommen++;
        else if (!erster[0])
            snprintf(erster, sizeof erster,
                     "%u Byte: probe=0 oder konf=%d", (unsigned)g, konf);
    }
    snprintf(det, sizeof det, "%d von 5 angenommen%s%s", angenommen,
             erster[0] ? " - " : "", erster);
    pruefe("alle fuenf Groessen der HS_*-Tafel sind erreichbar",
           angenommen == 5, det);

    /* 4 — eine Groesse ausserhalb der Tafel wird abgewiesen. Ohne diese
     *     Zusage waere „gib immer true zurueck" eine bestehende Loesung. */
    {
        uint8_t kopf[1024];
        memset(kopf, 0, sizeof kopf);
        konf = -1;
        ok = p->probe(kopf, sizeof kopf, 163841u, &konf) ? 1 : 0;
        snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
        pruefe("163 841 Byte (eine Groesse, die die Tafel nicht kennt) "
               "werden abgewiesen", !ok, det);
    }

    /* 5 — die Gegenprobe, die die GROESSENQUELLE festnagelt, und zwar
     *     genau die, an der MF-1029 einmal vorbeigemessen hat: ein
     *     wirklich 163 840 Byte grosser Puffer, aber eine Dateigroesse
     *     von 4096. Entscheiden muss die DATEIgroesse. */
    konf = -1;
    ok = p->probe(abbild, GESAMT, 4096u, &konf) ? 1 : 0;
    snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
    pruefe("ein 163 840-Byte-Puffer mit Dateigroesse 4096 wird "
           "abgewiesen - entschieden wird an der DATEIgroesse", !ok, det);

    /* 6 — und zugleich der Beleg fuer das Band: reiner Zufall derselben
     *     Groesse wird genauso angenommen. Die Sonde liest kein Merkmal,
     *     also darf sie keines beanspruchen. */
    {
        uint8_t *zufall = (uint8_t *)malloc(GESAMT);
        if (!zufall) { printf("kein Speicher\n"); free(abbild); return 1; }
        for (i = 0; i < (unsigned)GESAMT; i++)
            zufall[i] = (uint8_t)((i * 1103515245u + 12345u) >> 16);
        konf = -1;
        ok = sonde_wie_produktion(zufall, GESAMT, &konf);
        snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
        pruefe("Zufallsinhalt derselben Groesse wird EBENSO angenommen - "
               "das begruendet Band 30..49 statt 50..79",
               ok && konf >= 30 && konf <= 49, det);
        free(zufall);
    }

    /* 7 — der Leser selbst. Er war nie das Problem; das wird hier
     *     gemessen statt behauptet, damit die Berichtigung der Sonde
     *     nicht unbemerkt etwas anderes verschiebt. */
    pfad_bauen(pfad, sizeof pfad, "16sec");
    if (!schreibe_abbild(pfad)) {
        pruefe("Pruefdatei liess sich schreiben", 0, pfad);
        free(abbild);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    memset(&disk, 0, sizeof disk);
    if (p->open(&disk, pfad, true) != UFT_OK) {
        pruefe("open liest das Abbild", 0, pfad);
        remove(pfad);
        free(abbild);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    erster[0] = 0;
    for (c = 0; c < ZYL; c++) {
        for (h = 0; h < (int)KOEPFE; h++) {
            uft_track_t t;
            uint8_t soll[SGR];
            memset(&t, 0, sizeof t);
            if (p->read_track(&disk, (int)c, h, &t) != UFT_OK) continue;
            for (s = 0; s < t.sector_count; s++) {
                const uft_sector_t *sec = &t.sectors[s];
                /* `hardsector` fuellt `data_size` (legacy) und laesst
                 * `data_len` auf 0 — dieselbe Fallunterscheidung, die
                 * `uft_generic_verify_track()` seit jeher trifft. */
                size_t len = sec->data_len ? sec->data_len : sec->data_size;
                gesehen++;
                sektor_inhalt(c, (unsigned)h, s, soll);
                if (sec->data && len == SGR
                    && memcmp(sec->data, soll, SGR) == 0) gleich++;
                else {
                    falsch++;
                    if (!erster[0])
                        snprintf(erster, sizeof erster,
                                 "Spur %u/%d Sektor %u weicht ab (len=%u)",
                                 c, h, s, (unsigned)len);
                }
            }
            uft_track_release(&t);
        }
    }
    p->close(&disk);
    remove(pfad);

    snprintf(det, sizeof det, "%u gesehen, %u gleich, %u falsch%s%s",
             gesehen, gleich, falsch, erster[0] ? " - " : "", erster);
    pruefe("der Leser liefert 640 von 640 Sektoren byteidentisch - "
           "er war nie das Problem",
           gesehen == ZYL * KOEPFE * SEKT && gleich == gesehen
           && falsch == 0, det);

    free(abbild);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
