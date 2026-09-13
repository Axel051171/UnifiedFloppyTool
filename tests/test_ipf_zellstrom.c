/**
 * @file test_ipf_zellstrom.c
 * @brief IPF: `raw_data` versprach Zellen und lieferte Bytes (MF-1079)
 *
 * Behebt P3-360 (2). Gemessen an den beiden SPS-Abbildern im
 * eingeschraenkten Korpus.
 *
 * ── Der Befund ──────────────────────────────────────────────────────────
 *
 * `ipf_plugin_read_track()` fuellte `raw_data` aus
 * `ipf_air_get_track_raw()`. Das haengt die Werte der Datenelemente
 * aneinander — also die **dekodierten Bytes**. Das Feld verspricht aber
 * einen Zellstrom, und `raw_bits` dessen Zahl. Gemessen an Spur 0/0 von
 * `sps_lethalxcess_a.ipf`:
 *
 *     geliefert :  6 034 Byte = 48 272 Bit
 *     Datei     : trackbits   = 101 304   (= 12 663 Byte)
 *
 * Zwei verschiedene Aussagen in einem Feld — dieselbe Gestalt wie P3-357
 * bei `pri`. Ein Aufrufer, der `raw_bits` als Zellzahl nimmt, rechnet mit
 * einer Spur, die es nicht gibt.
 *
 * ── Die Regel, und woher sie kommt ──────────────────────────────────────
 *
 * Benannte Referenz: MAMEs `src/lib/formats/ipf_dsk.cpp` (BSD-3-Clause,
 * Olivier Galibert; im Baum unter `neue-ideen/formats1.zip`) — gelesen,
 * nicht uebernommen. Seine Zeilen 555-570 benennen die Elementarten:
 * SYNC und RAW sind **bereits Zellen**, DATA und GAP sind dekodierte
 * Bytes und ergeben je Datenbit **zwei** Zellen.
 *
 * Das ist hier keine Annahme, weil die Datei ihr eigener Pruefstein ist:
 * ueber die ganze Diskette gerechnet trifft die Zellsumme je Block
 * `datasize` in **1618 von 1618** Faellen, je Spur `databits` in 160 von
 * 160, und `databits + gapbits == trackbits` in 160 von 160. Deshalb
 * prueft die Umsetzung dieselbe Gleichung zur Laufzeit und gibt lieber
 * nichts aus als etwas Unbelegtes.
 *
 * ── Der zweite Befund: eine Absage, die ihren Grund kennt ───────────────
 *
 * Spur 79/0 von Diskette A sagt **35 Bloecke** an; `IPF_MAX_BLOCKS` ist
 * **16**. MF-830 hatte diesen Verlust bereits als `blocks_truncated`
 * festgehalten — dies ist der erste an einem echten Abbild **beobachtete**
 * Fall. Ein Strom aus 16 von 35 Bloecken waere kuerzer als `track_bits`,
 * also gibt es keinen: eigener Code `-3`, und der Aufrufer NENNT den
 * Grund, statt ihn zu raten.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Ausdrueckliche Gap-Beschreibungen.** Der Zwischenraum wird als
 *   MFM fuer lauter 0x00 gefuellt — mit dem laufenden Kontext, siehe
 *   Abschnitt 3b; die Referenz kennt vier Gap-Arten, die diese Fassung
 *   nicht auswertet. Steht so im Kopf der Umsetzung.
 * * **Der CAPS-Pfad** (`encoder_type=1`) — dort gibt es keine
 *   Blockelemente, und `uft_ipf_zellstrom()` sagt mit -1 ab.
 * * **Ob der Strom eine echte Diskette trifft.** Belegt ist, dass er die
 *   Zahlen der DATEI trifft. Ein Vergleich gegen eine Aufnahme derselben
 *   Diskette braucht Hardware (MF-310).
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"
#include "uft/formats/ipf/uft_ipf_air.h"
#include "uft/formats/ipf/uft_ipf_zellstrom.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef UFT_CORPUS_RESTRICTED_DIR
#define UFT_CORPUS_RESTRICTED_DIR ""
#endif

extern const uft_format_plugin_t uft_format_plugin_ipf;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

/** Ein Bit aus dem Zellstrom, MSB zuerst. */
static int bit_von(const uint8_t *p, uint32_t i)
{
    return (p[i >> 3] >> (7 - (i & 7u))) & 1;
}

/** 16 Zellen ab `i` als Wort — fuer den Sync-Vergleich. */
static uint32_t wort16(const uint8_t *p, uint32_t i)
{
    uint32_t w = 0, k;
    for (k = 0; k < 16u; k++) w = (w << 1) | (uint32_t)bit_von(p, i + k);
    return w;
}

/**
 * Misst die MFM-Zellregel ueber ALLE Spuren einer Diskette: keine zwei
 * benachbarten 1-Zellen, und hoechstens drei Nullzellen am Stueck.
 *
 * Das ist die Regel, die den Zellabstand einer MFM-Diskette ausmacht
 * (2, 3 oder 4 Mikrosekunden bei DD) — und sie ist von der
 * Laengenrechnung UNABHAENGIG: eine Spur kann exakt `trackbits` lang
 * sein und trotzdem eine Folge tragen, die kein Laufwerk schreiben
 * kann. Genau so lag der Fehler, den diese Messung gefunden hat.
 */
static void zellregel(ipf_air_disk_t *disk, unsigned long long *paare,
                      unsigned long long *max_null, unsigned *spuren)
{
    int zy = 0, ko = 0, cc, hh;
    uint32_t plattform = 0;
    *paare = 0; *max_null = 0; *spuren = 0;
    ipf_air_get_geometry(disk, &zy, &ko, &plattform);
    for (cc = 0; cc < zy; cc++)
        for (hh = 0; hh < ko; hh++) {
            uint8_t *b = NULL;
            uint32_t bits = 0, i;
            int vor = 0;
            unsigned long long lauf = 0;
            if (uft_ipf_zellstrom(disk, cc, hh, &b, &bits) != 0) continue;
            (*spuren)++;
            for (i = 0; i < bits; i++) {
                const int v = bit_von(b, i);
                if (v && vor) (*paare)++;
                if (v) { if (lauf > *max_null) *max_null = lauf; lauf = 0; }
                else lauf++;
                vor = v;
            }
            if (lauf > *max_null) *max_null = lauf;
            free(b);
        }
}

static uint8_t *lies_datei(const char *pfad, long *out_gr)
{
    FILE *f = fopen(pfad, "rb");
    long gr;
    uint8_t *ganz;
    if (!f) return NULL;
    fseek(f, 0, SEEK_END); gr = ftell(f); fseek(f, 0, SEEK_SET);
    if (gr <= 0) { fclose(f); return NULL; }
    ganz = (uint8_t *)malloc((size_t)gr);
    if (!ganz || fread(ganz, 1, (size_t)gr, f) != (size_t)gr) {
        fclose(f); free(ganz); return NULL;
    }
    fclose(f);
    *out_gr = gr;
    return ganz;
}

int main(void)
{
    char pfad_a[600], pfad_b[600], det[400];
    uint8_t *roh_a = NULL, *roh_b = NULL;
    long gr_a = 0, gr_b = 0;
    ipf_air_disk_t *a = NULL, *b = NULL;
    int zyl = 0, koepfe = 0, c, h;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("IPF: raw_data traegt Zellen, nicht Bytes - MF-1079\n");
    printf("==================================================\n");

    if (UFT_CORPUS_RESTRICTED_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_RESTRICTED_DIR nicht gesetzt.\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 */
    }
    snprintf(pfad_a, sizeof pfad_a, "%s/sps_lethalxcess_a.ipf",
             UFT_CORPUS_RESTRICTED_DIR);
    snprintf(pfad_b, sizeof pfad_b, "%s/sps_lethalxcess_b.ipf",
             UFT_CORPUS_RESTRICTED_DIR);
    roh_a = lies_datei(pfad_a, &gr_a);
    roh_b = lies_datei(pfad_b, &gr_b);
    if (!roh_a || !roh_b) {
        free(roh_a); free(roh_b);
        printf("SKIP: Korpus-Abbilder fehlen.\n");
        return 77;
    }

    a = ipf_air_alloc();
    b = ipf_air_alloc();
    if (!a || !b || ipf_air_parse(roh_a, (size_t)gr_a, a) != IPF_AIR_OK
        || ipf_air_parse(roh_b, (size_t)gr_b, b) != IPF_AIR_OK) {
        printf("SKIP: Abbilder nicht zerlegbar.\n");
        if (a) { ipf_air_free(a); free(a); }
        if (b) { ipf_air_free(b); free(b); }
        free(roh_a); free(roh_b);
        return 77;
    }
    {
        uint32_t plattform = 0;
        ipf_air_get_geometry(a, &zyl, &koepfe, &plattform);
    }

    /* ── 1. Jede Spur mit Bloecken trifft die Zahl DER DATEI ───────── */
    {
        unsigned gebaut = 0, ohne_bloecke = 0, gekappt = 0, falsch = 0;
        unsigned long long summe = 0;
        for (c = 0; c < zyl; c++)
            for (h = 0; h < koepfe; h++) {
                uint8_t *buf = NULL;
                uint32_t bits = 0, soll = 0, dichte = 0, flaggen = 0;
                bool fuzzy = false;
                int rc;
                if (!ipf_air_track_present(a, c, h)) continue;
                if (ipf_air_get_track_meta(a, c, h, &soll, &dichte,
                                           &flaggen, &fuzzy) != 0) continue;
                rc = uft_ipf_zellstrom(a, c, h, &buf, &bits);
                if (rc == -1)      { ohne_bloecke++; continue; }
                else if (rc == -3) { gekappt++;      continue; }
                if (rc != 0 || !buf || bits != soll) falsch++;
                else { gebaut++; summe += bits; }
                free(buf);
            }
        snprintf(det, sizeof det,
                 "%u gebaut, %u ohne Bloecke, %u gekappt, %u falsch, "
                 "%llu Bit", gebaut, ohne_bloecke, gekappt, falsch, summe);
        pruefe("Diskette A: 159 Spuren gebaut, jede genau so lang wie ihr "
               "eigenes trackbits - 4 ohne Bloecke, 1 gekappt, 0 falsch",
               gebaut == 159 && ohne_bloecke == 4 && gekappt == 1
               && falsch == 0 && summe == 16106832ULL, det);
    }
    {
        unsigned gebaut = 0, ohne = 0, falsch = 0;
        int zb = 0, kb = 0;
        uint32_t pb = 0;
        ipf_air_get_geometry(b, &zb, &kb, &pb);
        for (c = 0; c < zb; c++)
            for (h = 0; h < kb; h++) {
                uint8_t *buf = NULL;
                uint32_t bits = 0, soll = 0, d2 = 0, f2 = 0;
                bool fz = false;
                int rc;
                if (!ipf_air_track_present(b, c, h)) continue;
                if (ipf_air_get_track_meta(b, c, h, &soll, &d2, &f2,
                                           &fz) != 0) continue;
                rc = uft_ipf_zellstrom(b, c, h, &buf, &bits);
                if (rc == -1) ohne++;
                else if (rc != 0 || bits != soll) falsch++;
                else gebaut++;
                free(buf);
            }
        snprintf(det, sizeof det, "%u gebaut, %u ohne Bloecke, %u falsch",
                 gebaut, ohne, falsch);
        pruefe("Diskette B: 160 von 160 Spuren, keine einzige Absage - "
               "die Rechnung haengt nicht an einem Einzelfall",
               gebaut == 160 && ohne == 4 && falsch == 0, det);
    }

    /* ── 2. Der Strom ist NICHT die alte Ausgabe ───────────────────── */
    {
        uint8_t *zellen = NULL, *bytes = NULL;
        uint32_t zbits = 0, bbits = 0;
        int rz = uft_ipf_zellstrom(a, 0, 0, &zellen, &zbits);
        int rb = ipf_air_get_track_raw(a, 0, 0, &bytes, &bbits);
        snprintf(det, sizeof det, "Zellen rc=%d %u Bit, alt rc=%d %u Bit",
                 rz, zbits, rb, bbits);
        pruefe("Spur 0/0: der Zellstrom hat 101 304 Bit, die frueher "
               "gelieferten dekodierten Bytes haben 48 272 - genau diese "
               "Verwechslung war P3-360",
               rz == 0 && zbits == 101304u && rb == 0 && bbits == 48272u,
               det);

        /* ── 3. Die Zellen tragen den Amiga-Sync UNKODIERT ─────────
         *
         * Die Spur beginnt mit zwei Gap-Bytes 0x00 (MFM: 1010...) und
         * dann dem SYNC-Element 44 89 44 89. Genau hier entscheidet
         * sich die Regel: SYNC ist bereits ZELLEN. Waere es als Daten
         * MFM-kodiert worden, stuenden dort 0x4489 NICHT — und 0x4489
         * ist das Amiga-Synchronwort, das ein Laufwerk sucht. */
        if (rz == 0 && zellen && zbits > 64u) {
            snprintf(det, sizeof det,
                     "%02X %02X %02X %02X %02X %02X %02X %02X",
                     zellen[0], zellen[1], zellen[2], zellen[3],
                     zellen[4], zellen[5], zellen[6], zellen[7]);
            pruefe("Spur 0/0 beginnt mit AA AA AA AA (MFM-Zellen zweier "
                   "0x00-Gapbytes) und dann 44 89 44 89 - das "
                   "Amiga-Synchronwort steht UNKODIERT im Strom",
                   zellen[0] == 0xAA && zellen[1] == 0xAA
                   && zellen[2] == 0xAA && zellen[3] == 0xAA
                   && wort16(zellen, 32) == 0x4489u
                   && wort16(zellen, 48) == 0x4489u, det);

            /* Gegenprobe zur selben Regel: waere SYNC MFM-kodiert
             * worden, laege 0x4489 nicht an Zelle 32 — und an Zelle 64
             * steht es auch dann nicht, wenn man die Stelle nur
             * verschoben sucht. */
            snprintf(det, sizeof det, "Wort bei 32 = %04X, bei 64 = %04X",
                     wort16(zellen, 32), wort16(zellen, 64));
            pruefe("Gegenprobe: 0x4489 steht an Zelle 32, nicht an 64 - "
                   "die Sync-Bytes wurden mit 1 Zelle je Bit uebernommen, "
                   "nicht mit 2",
                   wort16(zellen, 32) == 0x4489u
                   && wort16(zellen, 64) != 0x4489u, det);
        } else {
            pruefe("Spur 0/0 liefert Zellen", 0, det);
            pruefe("Sync-Gegenprobe", 0, det);
        }
        free(zellen);
        free(bytes);
    }

    /* ── 3b. Die MFM-ZELLREGEL, ueber beide Disketten ──────────────
     *
     * Diese Zusage hat einen Fehler in meiner eigenen Umsetzung
     * gefunden, den die Laengenrechnung nicht sehen konnte. Der
     * Zwischenraum begann unbedingt mit einer 1-Zelle; war das letzte
     * Datenbit davor eine 1, standen dort zwei benachbarte Einsen —
     * eine Folge, die auf einer MFM-Diskette nicht vorkommt. Gemessen
     * am Vorzustand: **587** Paare auf A, **760** auf B, und alle in
     * MFM-Bereichen, keines in den Rohzellen — also genau dort, wo
     * sie unmoeglich sind. Die Spurlaengen stimmten dabei auf das Bit.
     *
     * Das ist die Lehre von MF-1026 in anderer Gestalt: eine Summe,
     * die aufgeht, sagt nichts ueber die Verteilung darin. */
    {
        unsigned long long pa = 0, pb = 0, na = 0, nb2 = 0;
        unsigned sa = 0, sb = 0;
        zellregel(a, &pa, &na, &sa);
        zellregel(b, &pb, &nb2, &sb);
        snprintf(det, sizeof det,
                 "A: %u Spuren, %llu Paare, Nulllauf %llu | "
                 "B: %u Spuren, %llu Paare, Nulllauf %llu",
                 sa, pa, na, sb, pb, nb2);
        pruefe("beide Disketten halten die MFM-Zellregel: keine zwei "
               "benachbarten 1-Zellen und hoechstens drei Nullen am "
               "Stueck - vor dem Zwischenraum-Fix waren es 587 bzw. "
               "760 unmoegliche Paare",
               sa == 159 && sb == 160 && pa == 0 && pb == 0
               && na == 3 && nb2 == 3, det);
    }

    /* ── 4. Die Absage nennt ihren Grund ───────────────────────────── */
    {
        uint8_t *buf = (uint8_t *)0x1;   /* muss auf NULL gesetzt werden */
        uint32_t bits = 7u;
        uint32_t angesagt = 0, gehalten = 0;
        bool gekappt = false;
        int rc = uft_ipf_zellstrom(a, 79, 0, &buf, &bits);
        int lrc = ipf_air_get_track_loss(a, 79, 0, &angesagt, &gehalten,
                                         &gekappt);
        snprintf(det, sizeof det,
                 "rc=%d buf=%s bits=%u | loss rc=%d %u angesagt, %u "
                 "gehalten, gekappt=%d", rc, buf ? "gesetzt" : "NULL",
                 bits, lrc, angesagt, gehalten, (int)gekappt);
        pruefe("Spur 79/0 sagt mit -3 ab UND gibt nichts aus - die Datei "
               "sagt 35 Bloecke an, IPF_MAX_BLOCKS ist 16 (MF-830, erster "
               "beobachteter Fall)",
               rc == -3 && buf == NULL && bits == 0 && lrc == 0
               && angesagt == 35u && gehalten == 16u && gekappt, det);
    }

    /* ── 5. Der Weg durch das PLUGIN, nicht nur durch die Funktion ── */
    {
        uft_disk_t disk;
        uft_track_t t;
        memset(&disk, 0, sizeof disk);
        if (uft_format_plugin_ipf.open(&disk, pfad_a, true) != UFT_OK) {
            pruefe("open liest sps_lethalxcess_a.ipf", 0, "open scheitert");
        } else {
            memset(&t, 0, sizeof t);
            if (uft_format_plugin_ipf.read_track(&disk, 0, 0, &t) != UFT_OK) {
                pruefe("read_track 0/0", 0, "read_track scheitert");
            } else {
                snprintf(det, sizeof det,
                         "raw_bits=%u raw_size=%u raw_data=%s",
                         (unsigned)t.raw_bits, (unsigned)t.raw_size,
                         t.raw_data ? "da" : "NULL");
                pruefe("read_track(0,0) reicht den ZELLSTROM heraus - "
                       "101 304 Bit in 12 663 Byte; vor MF-1079 standen "
                       "dort 6034 Byte und 48 272 Bit",
                       t.raw_data != NULL && t.raw_bits == 101304u
                       && t.raw_size == 12663u, det);
                uft_track_release(&t);
            }

            memset(&t, 0, sizeof t);
            if (uft_format_plugin_ipf.read_track(&disk, 79, 0, &t) != UFT_OK) {
                pruefe("read_track 79/0", 0, "read_track scheitert");
            } else {
                snprintf(det, sizeof det, "raw_data=%s raw_size=%u",
                         t.raw_data ? "da" : "NULL", (unsigned)t.raw_size);
                pruefe("read_track(79,0) liefert die Metadaten und LAESST "
                       "raw_data leer - ein gekuerzter Strom waere eine "
                       "erfundene Spur",
                       t.raw_data == NULL && t.raw_size == 0, det);
                uft_track_release(&t);
            }
            uft_format_plugin_ipf.close(&disk);
        }
    }

    ipf_air_free(a); free(a);
    ipf_air_free(b); free(b);
    free(roh_a); free(roh_b);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
