/**
 * @file test_edk_gegen_ensoniq.c
 * @brief EDK gegen die benannte Ensoniq-Beschreibung (MF-1056)
 *
 * ── Warum es diese Referenz vorher nicht gab ────────────────────────────
 *
 * `edk` stand seit MF-1041 als `P3-340` auf **T3**, mit dem Satz im
 * eigenen Kopf: *„Dieses Plugin hat keine nachpruefbare Referenz — der
 * Kopf nannte keine, und im Baum liegt keine."* Das stimmte. Gesucht
 * wurde dann an der falschen Stelle: nicht im Baum, sondern draussen.
 *
 * ── Zwei Haende ─────────────────────────────────────────────────────────
 *
 * **1. Die Beschreibung** — `youngmonkey.ca/nose/audio_tech/synth/
 * Ensoniq-DiskFormats.html`, am 2026-09-12 gegen die lebende Seite
 * geprueft (nicht nur gegen eine Abschrift). Woertlich:
 *
 *     „80 tracks numbered 0 - 79 on each side"
 *     „Each track has ten 512 byte sectors numbered consecutively
 *      from zero to nine"
 *     „Block = ((Track x 2) + Head) x 10) + Sector"
 *     „The blocks … are numbered from 0 - 1599"
 *
 * Sie gilt fuer EPS, EPS-16, SD-1 und VFX-SD; **die ASR nennt sie
 * nicht**.
 *
 * **2. Die Umsetzung** — EpsLin v1.58, dessen Konstanten im Klon
 * `tools/uft-scout/work/epstool/references/epslin_source_constants.txt`
 * liegen (nur gelesen):
 *
 *     #define EPS_IMAGE_SIZE      819200    // 800K DD  (1600 blocks)
 *     #define E16_SD_IMAGE_SIZE  2611200    // 2550K SuperDisk
 *     #define ASR_IMAGE_SIZE     1638400    // 1600K HD  (3200 blocks)
 *     #define ASR_SD_IMAGE_SIZE  5222400    // 5100K SuperDisk
 *
 * **Und die HD-Geometrie rechnet sich daraus selbst auf:** 3200 Bloecke
 * auf 80 Spuren x 2 Seiten sind **20 Sektoren je Spur**. Das ist dieselbe
 * Art Beleg wie MF-1050 (`a2nibblize` leitet 6656 aus der Feldanordnung
 * ab) — eine Zahl, die sich selbst begruendet, statt behauptet zu werden.
 *
 * ── Der Befund ──────────────────────────────────────────────────────────
 *
 * **Die Sektornummern waren um eins zu hoch.** `edk_read_track()` rief
 * `uft_format_add_sector(t, s, …)`, und dieser Helfer addiert laut
 * seinem eigenen Kopf 1 — er ist „fuer Formate mit Sektoren 1..N (IBM PC
 * und Verwandte)", und derselbe Kopf sagt: *„Formate, deren Sektoren bei
 * 0 beginnen, benutzen `uft_format_add_sector_with_id()`."*
 *
 * Ensoniq beginnt bei 0. Gemessen lieferte jede Spur die IDs **1..10**
 * statt 0..9, bei HD 1..20 statt 0..19. Das ist die Gestalt von MF-1016
 * (`jv1`) und MF-1026 (`tan`) — **zum dritten Mal**.
 *
 * ── Was richtig war und richtig bleibt ──────────────────────────────────
 *
 * Die Versatzformel `((cyl * 2 + head) * spt * 512)` ist **genau** die
 * Blockrechnung der Beschreibung, und die beiden angenommenen Groessen
 * sind zwei der vier, die EpsLin nennt. Beides gemessen und festgenagelt,
 * nicht angefasst.
 *
 * ── Die zwei SuperDisk-Groessen: benannt, nicht angenommen ──────────────
 *
 * `2 611 200` und `5 222 400` Byte sind echte Ensoniq-Bildgroessen, und
 * `edk` weist sie ab. **Das bleibt so, und der Grund ist gerechnet:**
 * 2 611 200 / 512 = 5100 Bloecke und 5 222 400 / 512 = 10 200 Bloecke —
 * keine der beiden Zahlen ist durch 160 (80 Spuren x 2 Seiten) teilbar,
 * also ist es keine Diskettengeometrie. Eine Geometrie dafuer zu erfinden
 * waere die Wette aus FMT-2/3. Der Test haelt die Abweisung fest, damit
 * sie eine Entscheidung bleibt und kein Versehen wird.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Die Endung `.ede`**, die die Merkmalstafel mitfuehrt. EDE ist
 *   Gieblers **komprimiertes** Format — die EpsLin-Konstanten fuehren
 *   dafuer eine Uebersprungtabelle (`EDE_SKIP_SIZE 200`), es ist also
 *   kein rohes Abbild und hat keine der vier Groessen. Siehe die
 *   Berichtigung in der Merkmalstafel.
 * * **Der Geraetekopf in Block 0** (Sektoren je Spur bei Byte 5-6,
 *   Zylinder bei 9-10, Bytes je Block bei 11-14). `edk` liest ihn nicht;
 *   er waere die naechste Verschaerfung der Sonde.
 */
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

extern const uft_format_plugin_t uft_format_plugin_edk;

#define BLOCKGR   512u
#define SPUREN     80u
#define KOEPFE      2u
#define SPT_DD     10u
#define SPT_HD     20u
#define DD_GROESSE (SPUREN * KOEPFE * SPT_DD * BLOCKGR)   /*   819 200 */
#define HD_GROESSE (SPUREN * KOEPFE * SPT_HD * BLOCKGR)   /* 1 638 400 */

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
    snprintf(p, n, "%s/uft_edk_%s.eds", d, name);
}

/* Jeder Block benennt sich selbst — so sagt ein Leseergebnis nicht nur,
 * DASS etwas kam, sondern WELCHER Block geliefert wurde (MF-1020). */
static void block_inhalt(unsigned nr, uint8_t *b)
{
    char k[16];
    unsigned i;
    snprintf(k, sizeof k, "UFT-K B%04u ", nr);
    memcpy(b, k, 12);
    for (i = 12; i < BLOCKGR; i++)
        b[i] = (uint8_t)((nr * 7u + (i - 12u) * 3u) & 0xFFu);
}

static int schreibe_abbild(const char *pfad, unsigned spt)
{
    unsigned bloecke = SPUREN * KOEPFE * spt;
    uint8_t b[BLOCKGR];
    FILE *f = fopen(pfad, "wb");
    unsigned i;
    if (!f) return 0;
    for (i = 0; i < bloecke; i++) {
        block_inhalt(i, b);
        if (fwrite(b, 1, BLOCKGR, f) != BLOCKGR) { fclose(f); return 0; }
    }
    fclose(f);
    return 1;
}

/* Liest das ganze Abbild und prueft zweierlei: dass jeder Sektor den
 * Block traegt, den die Formel der Beschreibung nennt, und dass seine
 * Nummer 0-basiert ist. */
static void durchlauf(const char *pfad, unsigned spt, const char *was)
{
    const uft_format_plugin_t *p = &uft_format_plugin_edk;
    uft_disk_t disk;
    unsigned c, s, gesehen = 0, gleich = 0, falsch = 0;
    unsigned id_falsch = 0;
    char det[220];
    char erster[160];
    uint8_t soll[BLOCKGR];
    int h;

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
    pruefe("Geometrie 80 x 2 x spt x 512 nach der Beschreibung",
           disk.geometry.cylinders == (int)SPUREN
           && disk.geometry.heads == (int)KOEPFE
           && disk.geometry.sectors == (int)spt
           && disk.geometry.sector_size == (int)BLOCKGR, det);

    for (c = 0; c < SPUREN; c++) {
        for (h = 0; h < (int)KOEPFE; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof t);
            if (p->read_track(&disk, (int)c, h, &t) != UFT_OK) continue;
            for (s = 0; s < t.sector_count; s++) {
                const uft_sector_t *sec = &t.sectors[s];
                /* Block = ((Track x 2) + Head) x spt + Sector */
                unsigned blk = ((c * KOEPFE) + (unsigned)h) * spt + s;
                gesehen++;
                block_inhalt(blk, soll);
                if (sec->data && sec->data_len == BLOCKGR
                    && memcmp(sec->data, soll, BLOCKGR) == 0) gleich++;
                else {
                    falsch++;
                    if (!erster[0])
                        snprintf(erster, sizeof erster,
                                 "Block %u weicht ab", blk);
                }
                /* Die Beschreibung: Sektoren 0 bis spt-1. */
                if ((unsigned)sec->id.sector != s) {
                    id_falsch++;
                    if (!erster[0])
                        snprintf(erster, sizeof erster,
                                 "Spur %u/%d: Sektor-ID %u, erwartet %u",
                                 c, h, (unsigned)sec->id.sector, s);
                }
            }
        }
    }

    snprintf(det, sizeof det, "%s: %u gesehen, %u gleich, %u falsch%s%s",
             was, gesehen, gleich, falsch, erster[0] ? " — " : "", erster);
    pruefe("jeder Sektor traegt den Block aus "
           "((Spur x 2) + Kopf) x spt + Sektor",
           gesehen == SPUREN * KOEPFE * spt && gleich == gesehen
           && falsch == 0, det);

    snprintf(det, sizeof det, "%s: %u Sektoren mit falscher Nummer%s%s",
             was, id_falsch, erster[0] ? " — " : "", erster);
    pruefe("die Sektornummern sind 0-basiert, wie die Beschreibung sagt",
           id_falsch == 0, det);

    p->close(&disk);
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_edk;
    char pdd[600], phd[600], det[200];
    int konf, ok;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("EDK gegen youngmonkey.ca + EpsLin v1.58 — MF-1056\n");
    printf("=================================================\n");

    /* 1 — die Sonde nimmt beide Diskettengroessen an, im Band
     *     „nur die Groesse" (30..49 nach MF-729): erkannt ist genau
     *     die Dateigroesse, kein Merkmal. */
    konf = -1;
    ok = p->probe(NULL, 0, DD_GROESSE, &konf) ? 1 : 0;
    snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
    pruefe("819 200 Byte (EPS, 1600 Bloecke) werden angenommen, "
           "Konfidenz 30..49", ok && konf >= 30 && konf <= 49, det);

    konf = -1;
    ok = p->probe(NULL, 0, HD_GROESSE, &konf) ? 1 : 0;
    snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
    pruefe("1 638 400 Byte (ASR HD, 3200 Bloecke) werden angenommen",
           ok && konf >= 30 && konf <= 49, det);

    /* 2 — die beiden SuperDisk-Groessen sind ECHT und werden trotzdem
     *     abgewiesen: ihre Blockzahl ist nicht durch 160 teilbar, also
     *     ist es keine Diskettengeometrie. Festgehalten, damit die
     *     Abweisung eine Entscheidung bleibt. */
    konf = -1;
    ok = p->probe(NULL, 0, 2611200u, &konf) ? 1 : 0;
    snprintf(det, sizeof det, "probe=%d (5100 Bloecke, 5100 %% 160 = %u)",
             ok, 5100u % 160u);
    pruefe("2 611 200 Byte (EPS16 SuperDisk) werden abgewiesen — "
           "5100 Bloecke sind keine 80 x 2 x N", !ok, det);

    konf = -1;
    ok = p->probe(NULL, 0, 5222400u, &konf) ? 1 : 0;
    snprintf(det, sizeof det, "probe=%d (10200 Bloecke, 10200 %% 160 = %u)",
             ok, 10200u % 160u);
    pruefe("5 222 400 Byte (ASR SuperDisk) werden abgewiesen — "
           "10 200 Bloecke sind keine 80 x 2 x N", !ok, det);

    /* 3 — eine Datei, die keine der vier Groessen hat, wird abgewiesen. */
    konf = -1;
    ok = p->probe(NULL, 0, 100u, &konf) ? 1 : 0;
    snprintf(det, sizeof det, "probe=%d", ok);
    pruefe("100 Byte werden abgewiesen", !ok, det);

    /* 4 und 5 — die beiden Diskettengeometrien ganz durchlesen. */
    pfad_bauen(pdd, sizeof pdd, "dd");
    pfad_bauen(phd, sizeof phd, "hd");

    if (schreibe_abbild(pdd, SPT_DD)) {
        durchlauf(pdd, SPT_DD, "DD");
        remove(pdd);
    } else {
        pruefe("DD-Pruefdatei liess sich schreiben", 0, pdd);
    }

    if (schreibe_abbild(phd, SPT_HD)) {
        durchlauf(phd, SPT_HD, "HD");
        remove(phd);
    } else {
        pruefe("HD-Pruefdatei liess sich schreiben", 0, phd);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
