/**
 * @file test_xdm86_ist_ti99.c
 * @brief `xdm86` trug WOERTLICH die zwei Befunde, die MF-1027 an `v9t9`
 *        behoben hat (MF-1057)
 *
 * ── Wie der Befund gefunden wurde ───────────────────────────────────────
 *
 * `xdm86` stand als `P3-340` auf T3, mit dem Satz: „Es gibt keine
 * nachpruefbare Referenz." Die Suche nach einer draussen blieb leer —
 * **„XDM86" kommt als DATEIFORMAT nirgends vor**; im Feld heissen die
 * TI-99-Abbildformate `v9t9`/DOAD, PC99 und DSK, und „Disk Manager" ist
 * ein PROGRAMM auf dem TI-99, kein Behaelter.
 *
 * Aber MF-1041 hatte die entscheidende Beobachtung schon notiert und
 * nicht zu Ende verfolgt: **`xdm86`s drei Groessen sind dieselben, die
 * `v9t9` fuehrt** — 92 160, 184 320 und 368 640 Byte —, bei derselben
 * Sektorgroesse 256 und denselben Geometrien. Und `v9t9` ist seit
 * MF-1027 auf **T2**, gegen MAMEs `ti99_dsk.cpp`.
 *
 * Die Frage war also nie „wo ist eine Referenz fuer XDM86", sondern:
 * **liest dieses zweite TI-99-Plugin dieselben Dateien gleich?**
 *
 * ── Es liest sie nicht gleich, und zwar zweimal falsch ──────────────────
 *
 * **1. Die Anordnung.** `xdm86_read_track()` rechnete
 *
 *     off = (cyl * heads + head) * spt * 256
 *
 * — **genau die Zeile, die MF-1027 aus `uft_v9t9.c` entfernt hat.** MAMEs
 * `ti99_dsk.cpp` (LGPL-2.1+, Michael Zapf) dokumentiert die Wahrheit
 * woertlich, und `uft_v9t9.c` zitiert sie im Kopf:
 *
 *     „all tracks on side 0 as going inwards, and then all tracks on
 *      side 1 going outwards —  00 01 ... 38 39 / 79 78 ... 41 40"
 *
 * Die Datei ist **kopf-dur**, und auf Seite 1 laeuft die Spurzahl
 * **rueckwaerts**. Die richtige Rechnung steht seit MF-1027 in
 * `v9t9_track_offset()`:
 *
 *     logical = (head == 0) ? cyl : (2 * cyl_count - cyl - 1)
 *
 * **2. Die Sektornummern.** `xdm86` rief `uft_format_add_sector(t, s, …)`,
 * und dieser Helfer addiert laut seinem eigenen Kopf 1. MF-1027 hat an
 * MAMEs `load_track()` gemessen, dass die TI-Sektornummern **0-basiert**
 * sind (`sector[i] = secno` mit `secno` aus `0..sectorcount-1`).
 *
 * ── Warum das bemerkenswert ist ─────────────────────────────────────────
 *
 * Das ist die Gestalt von MF-1026: dort trug `tan` **woertlich die zwei
 * Befunde**, die MF-1016 an `jv1` behoben hatte — die erfundene zweite
 * Seite und die um eins verschobenen Sektornummern. Hier ist es dasselbe
 * Paar, zwischen zwei Plugins **derselben Maschine mit denselben drei
 * Dateigroessen**. Eine Korrektur an einer Datei sagt nichts ueber ihre
 * Nachbarn (MF-519/MF-529), und diesmal lag der Nachbar so nah, dass die
 * Groessenliste ihn schon genannt hatte.
 *
 * Die 1-basierte Sektornummer ist zugleich der **vierte** Fall dieser
 * Falle: MF-1016 (`jv1`), MF-1026 (`tan`), MF-1056 (`edk`) und hier.
 *
 * ── Die offene Frage, die dieser Test NICHT entscheidet ─────────────────
 *
 * Nach dieser Korrektur lesen `xdm86` und `v9t9` dieselben Dateien
 * **gleich**. Ob es zwei Plugins fuer ein Format braucht, ist eine
 * Eigentuemer-Entscheidung wie bei `rcpmfs` (P3-338) und steht als
 * Nachtrag in `P3-340`. Gefaehrlich ist sie nicht: die Sonden ordnen
 * sich von selbst, weil `v9t9` mit gueltiger VIB im Band „Merkmal
 * getroffen" liegt und `xdm86` bei „nur die Groesse" bleibt.
 */
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern const uft_format_plugin_t uft_format_plugin_xdm86;

#define SS        256u
#define SSSD_LEN   92160u   /* 40 x 1 x  9 x 256 */
#define DSSD_LEN  184320u   /* 40 x 2 x  9 x 256 */
#define DSDD_LEN  368640u   /* 40 x 2 x 18 x 256 */

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s — %s\n", was, detail ? detail : ""); }
}

static void pfad_bauen(char *p, size_t n, const char *name)
{
    const char *d = getenv("TMPDIR");
    if (!d || !d[0]) d = getenv("TMP");
    if (!d || !d[0]) d = getenv("TEMP");
    if (!d || !d[0]) d = ".";
    snprintf(p, n, "%s/uft_xdm86_%s.dsk", d, name);
}

/* Jeder Sektor benennt seine Stelle IN DER DATEI — logische Spur und
 * Sektor. So sagt ein Leseergebnis, WELCHE Stelle geliefert wurde
 * (Methode MF-1020). */
static void sektor_inhalt(unsigned log_spur, unsigned sek, uint8_t *b)
{
    char k[20];
    unsigned i;
    snprintf(k, sizeof k, "UFT-K L%02u S%02u ", log_spur, sek);
    memcpy(b, k, 14);
    for (i = 14; i < SS; i++)
        b[i] = (uint8_t)((log_spur * 31u + sek * 7u + i) & 0xFFu);
}

static int schreibe_abbild(const char *pfad, unsigned log_spuren, unsigned spt)
{
    uint8_t b[SS];
    FILE *f = fopen(pfad, "wb");
    unsigned t, s;
    if (!f) return 0;
    for (t = 0; t < log_spuren; t++)
        for (s = 0; s < spt; s++) {
            sektor_inhalt(t, s, b);
            if (fwrite(b, 1, SS, f) != SS) { fclose(f); return 0; }
        }
    fclose(f);
    return 1;
}

/* Die Regel aus MF-1027 / MAMEs ti99_dsk.cpp. */
static unsigned logische_spur(unsigned cyl, unsigned head, unsigned cyl_count)
{
    return (head == 0u) ? cyl : (2u * cyl_count - cyl - 1u);
}

static void durchlauf(const char *pfad, unsigned cyl_count, unsigned heads,
                      unsigned spt, const char *was)
{
    const uft_format_plugin_t *p = &uft_format_plugin_xdm86;
    uft_disk_t disk;
    unsigned c, h, s, gesehen = 0, gleich = 0, falsch = 0, id_falsch = 0;
    char det[240], erster[180];
    uint8_t soll[SS];

    erster[0] = 0;
    memset(&disk, 0, sizeof disk);
    if (p->open(&disk, pfad, true) != UFT_OK) {
        snprintf(det, sizeof det, "%s: open scheitert", was);
        pruefe("open liest das Abbild", 0, det);
        return;
    }

    snprintf(det, sizeof det, "%s: %d x %d x %d x %d", was,
             disk.geometry.cylinders, disk.geometry.heads,
             disk.geometry.sectors, disk.geometry.sector_size);
    pruefe("Geometrie wie die Dateigroesse sie bestimmt",
           (unsigned)disk.geometry.cylinders == cyl_count
           && (unsigned)disk.geometry.heads == heads
           && (unsigned)disk.geometry.sectors == spt
           && (unsigned)disk.geometry.sector_size == SS, det);

    for (c = 0; c < cyl_count; c++) {
        for (h = 0; h < heads; h++) {
            uft_track_t t;
            unsigned lg = logische_spur(c, h, cyl_count);
            memset(&t, 0, sizeof t);
            if (p->read_track(&disk, (int)c, (int)h, &t) != UFT_OK) continue;
            for (s = 0; s < t.sector_count; s++) {
                const uft_sector_t *sec = &t.sectors[s];
                gesehen++;
                sektor_inhalt(lg, s, soll);
                if (sec->data && sec->data_len == SS
                    && memcmp(sec->data, soll, SS) == 0) gleich++;
                else {
                    falsch++;
                    if (!erster[0]) {
                        char kam[16];
                        memset(kam, 0, sizeof kam);
                        if (sec->data && sec->data_len >= 14)
                            memcpy(kam, sec->data, 14);
                        snprintf(erster, sizeof erster,
                                 "Zyl %u Kopf %u Sektor %u: kam \"%s\", "
                                 "soll logische Spur %u", c, h, s, kam, lg);
                    }
                }
                if ((unsigned)sec->id.sector != s) {
                    id_falsch++;
                    if (!erster[0])
                        snprintf(erster, sizeof erster,
                                 "Zyl %u Kopf %u: Sektor-ID %u, erwartet %u",
                                 c, h, (unsigned)sec->id.sector, s);
                }
            }
        }
    }

    snprintf(det, sizeof det, "%s: %u gesehen, %u gleich, %u falsch%s%s",
             was, gesehen, gleich, falsch, erster[0] ? " — " : "", erster);
    pruefe("jede Spur liegt dort, wo MAMEs Regel sie hinlegt "
           "(Seite 1 rueckwaerts)",
           gesehen == cyl_count * heads * spt && gleich == gesehen
           && falsch == 0, det);

    snprintf(det, sizeof det, "%s: %u Sektoren mit falscher Nummer%s%s",
             was, id_falsch, erster[0] ? " — " : "", erster);
    pruefe("die Sektornummern sind 0-basiert, wie MAMEs load_track()",
           id_falsch == 0, det);

    p->close(&disk);
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_xdm86;
    char p1[600], p2[600], p3[600], det[200];
    int konf, ok;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("XDM86 gegen die TI-99-Regel aus MF-1027 — MF-1057\n");
    printf("=================================================\n");

    /* 1 — die Sonde: drei Groessen, Band „nur die Groesse". */
    konf = -1;
    ok = p->probe(NULL, 0, SSSD_LEN, &konf) ? 1 : 0;
    snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
    pruefe("92 160 Byte werden angenommen, Konfidenz 30..49",
           ok && konf >= 30 && konf <= 49, det);

    konf = -1;
    ok = p->probe(NULL, 0, 100u, &konf) ? 1 : 0;
    snprintf(det, sizeof det, "probe=%d", ok);
    pruefe("100 Byte werden abgewiesen", !ok, det);

    /* 2 — einseitig: hier gehen alte und neue Rechnung ineinander ueber
     *     (MF-1027: `heads == 1` macht aus `cyl*1+0` genau `cyl`).
     *     Der Fall ist deshalb KEIN Rotbeweis, sondern die Gegenprobe,
     *     dass die Korrektur die einseitigen Abbilder nicht bricht. */
    pfad_bauen(p1, sizeof p1, "sssd");
    if (schreibe_abbild(p1, 40, 9)) {
        durchlauf(p1, 40, 1, 9, "SSSD");
        remove(p1);
    } else {
        pruefe("SSSD-Pruefdatei liess sich schreiben", 0, p1);
    }

    /* 3 — zweiseitig, einfache Dichte: 80 logische Spuren. */
    pfad_bauen(p2, sizeof p2, "dssd");
    if (schreibe_abbild(p2, 80, 9)) {
        durchlauf(p2, 40, 2, 9, "DSSD");
        remove(p2);
    } else {
        pruefe("DSSD-Pruefdatei liess sich schreiben", 0, p2);
    }

    /* 4 — zweiseitig, doppelte Dichte. */
    pfad_bauen(p3, sizeof p3, "dsdd");
    if (schreibe_abbild(p3, 80, 18)) {
        durchlauf(p3, 40, 2, 18, "DSDD");
        remove(p3);
    } else {
        pruefe("DSDD-Pruefdatei liess sich schreiben", 0, p3);
    }

    /* 5 — die eine Stelle, an der die Umkehrung am deutlichsten ist:
     *     Kopf 1, Zylinder 0 ist die AEUSSERSTE Spur der zweiten Seite
     *     und liegt damit bei der logischen Spur 79. */
    pfad_bauen(p2, sizeof p2, "dssd2");
    if (schreibe_abbild(p2, 80, 9)) {
        uft_disk_t disk;
        uft_track_t t;
        uint8_t soll[SS];
        memset(&disk, 0, sizeof disk);
        if (p->open(&disk, p2, true) == UFT_OK
            && (memset(&t, 0, sizeof t),
                p->read_track(&disk, 0, 1, &t) == UFT_OK)
            && t.sector_count > 0) {
            char kam[16];
            memset(kam, 0, sizeof kam);
            if (t.sectors[0].data && t.sectors[0].data_len >= 14)
                memcpy(kam, t.sectors[0].data, 14);
            sektor_inhalt(79, 0, soll);
            snprintf(det, sizeof det, "kam \"%s\"", kam);
            pruefe("Kopf 1 / Zylinder 0 ist die logische Spur 79 "
                   "(die aeusserste der zweiten Seite)",
                   t.sectors[0].data && t.sectors[0].data_len == SS
                   && memcmp(t.sectors[0].data, soll, SS) == 0, det);
            p->close(&disk);
        } else {
            pruefe("Kopf 1 / Zylinder 0 ist die logische Spur 79", 0,
                   "nicht lesbar");
        }
        remove(p2);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
