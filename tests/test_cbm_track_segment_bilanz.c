/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * test_cbm_track_segment_bilanz.c — jedes Bit der Spur gehoert zu genau
 * einem Segment (MF-1333, Stufe 2).
 *
 * ── Warum der Test die Spur SELBST baut ──────────────────────────────
 * Er koennte `d64_write_track_gcr()` rufen. Dann befragte er aber
 * dieselbe Quelle wie der Pruefling, und eine Uebereinstimmung sagte
 * nichts (Klasse MF-1000/MF-1026 — die Victor-Tafel steht im Test
 * deshalb ein zweites Mal). Die Anordnung wird hier aus der
 * Beschreibung nachgebaut:
 *
 *     Sync (sync_len Byte 0xFF) · Kopf 10 GCR · Gap1
 *     Sync (sync_len Byte 0xFF) · Daten 325 GCR · Gap2
 *
 * Referenz `src/formats/uft_d64_writer.c:358-409`; Blockkennungen
 * `D64_HEADER_MARK 0x08` / `D64_DATA_MARK 0x07`
 * (`include/uft/uft_d64_writer.h:46-47`).
 *
 * ── Die drei Lueckenmuster ───────────────────────────────────────────
 * Aus der Beschreibung des Urhebers von GeoCopy (Christian Meilinger,
 * `neue-ideen/geocopy.zip -> geocopy/READ.ME.cvt`; Kanal *Spec* nach
 * MF-695, NC-Klausel, kein Port):
 *
 *     Standardformat    $55 $55 $55 …   einheitlich
 *     Original          $55 $55 $67 …   gemischt, jedes dritte Byte
 *     GeoCopy-Nachbau   $67 $67 $67 …   einheitlich
 *
 * ── Die Bilanz ist die eigentliche Zusage ────────────────────────────
 * MF-1026 hat gemessen, dass eine aufgehende Summe nichts ueber die
 * Verteilung darin sagt: zwei Zonengrenzen lagen um +1 und -1 daneben,
 * die Summe blieb 1224, und 22 Spuren wurden falsch gelesen. Dieser
 * Test prueft deshalb BEIDES — die Summe UND dass jedes Segment genau
 * dort beginnt, wo das vorige endet.
 *
 * ── Und die Bitprobe ─────────────────────────────────────────────────
 * Zusage 10 schiebt die ganze Spur um DREI Bit nach rechts. Ein
 * byteweiser Zerleger faellt daran sofort um; der Baum hat diese Klasse
 * bereits benannt (`gcr_count_syncs_bytealigned()` sagt im Namen, was
 * es nicht kann).
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/formats/cbm/uft_cbm_track_segment.h"
#include "uft/formats/c64/uft_gcr_ops.h"

static int fehler = 0;

static void zusage(int ok, const char *was)
{
    printf("  %-66s %s\n", was, ok ? "ok" : "[ROT]");
    if (!ok) fehler++;
}

/* ── Spurbau nach der Beschreibung ───────────────────────────────────*/

#define SYNC_LEN   5      /* Byte 0xFF vor jedem Block */
#define GAP_LEN    9      /* D64_GAP1_LENGTH / D64_GAP2_LENGTH */
#define SEKTOREN  21      /* Zone 3: Spuren 1..17 */
#define JE_SEKTOR (SYNC_LEN + 10 + GAP_LEN + SYNC_LEN + 325 + GAP_LEN)
#define SPUR_LEN  (SEKTOREN * JE_SEKTOR)     /* 21 * 363 = 7623 */

/** Welches Byte an Stelle `k` der Luecke steht. */
typedef uint8_t (*gap_muster_t)(int k);

static uint8_t muster_standard(int k) { (void)k; return 0x55; }
static uint8_t muster_geocopy(int k)  { (void)k; return 0x67; }
/* Original: "... $55 $55 $67 $55 $55 $67 ..." — jedes dritte Byte. */
static uint8_t muster_original(int k) { return (k % 3 == 2) ? 0x67 : 0x55; }

/** Baut eine vollstaendige GCR-Spur. Gibt die Laenge in Bytes zurueck. */
static size_t spur_bauen(uint8_t *aus, int spur, gap_muster_t gap)
{
    uint8_t *p = aus;

    for (int s = 0; s < SEKTOREN; s++) {
        /* Sync vor dem Kopf */
        memset(p, 0xFF, SYNC_LEN); p += SYNC_LEN;

        /* Kopfblock: 8 Klarbyte -> 10 GCR-Byte */
        uint8_t kopf[8];
        kopf[0] = 0x08;                                  /* Kennung */
        kopf[2] = (uint8_t)s;                            /* Sektor  */
        kopf[3] = (uint8_t)spur;                         /* Spur    */
        kopf[4] = 0x41;                                  /* id2     */
        kopf[5] = 0x42;                                  /* id1     */
        kopf[6] = 0x0F;
        kopf[7] = 0x0F;
        kopf[1] = (uint8_t)(kopf[3] ^ kopf[2] ^ kopf[5] ^ kopf[4]);
        p += gcr_encode(kopf, sizeof kopf, p);

        /* Gap 1 */
        for (int k = 0; k < GAP_LEN; k++) *p++ = gap(k);

        /* Sync vor den Daten */
        memset(p, 0xFF, SYNC_LEN); p += SYNC_LEN;

        /* Datenblock: 260 Klarbyte -> 325 GCR-Byte */
        uint8_t daten[260];
        memset(daten, 0, sizeof daten);
        daten[0] = 0x07;                                 /* Kennung */
        for (int k = 0; k < 256; k++)
            daten[1 + k] = (uint8_t)(s * 7 + k);
        uint8_t ps = 0;
        for (int k = 0; k < 256; k++) ps ^= daten[1 + k];
        daten[257] = ps;
        p += gcr_encode(daten, sizeof daten, p);

        /* Gap 2 */
        for (int k = 0; k < GAP_LEN; k++) *p++ = gap(k);
    }

    return (size_t)(p - aus);
}

/** Schiebt `n` Byte um `versatz` Bits nach rechts; `aus` braucht
 *  `n + 1` Byte. Fuehrende Bits werden 0. */
static void bits_schieben(const uint8_t *ein, size_t n, int versatz,
                          uint8_t *aus)
{
    memset(aus, 0, n + 1);
    for (size_t bit = 0; bit < n * 8; bit++) {
        int b = (ein[bit >> 3] >> (7 - (bit & 7))) & 1;
        size_t z = bit + (size_t)versatz;
        if (b) aus[z >> 3] |= (uint8_t)(1u << (7 - (z & 7)));
    }
}

/* ── Pruefungen ──────────────────────────────────────────────────────*/

/** Die Bilanz: lueckenlos, ueberlappungsfrei, Summe = Spurlaenge. */
static int bilanz_geht_auf(const uft_cbm_track_segmentierung_t *s)
{
    if (s->bits_erfasst != s->bits_gesamt) return 0;
    size_t erwartet = 0;
    for (size_t i = 0; i < s->anzahl; i++) {
        if (s->segmente[i].bit_start != erwartet) return 0;
        if (s->segmente[i].bit_len == 0) return 0;
        erwartet += s->segmente[i].bit_len;
    }
    return erwartet == s->bits_gesamt;
}

/** Erste Luecke der Segmentierung, oder NULL. */
static const uft_cbm_segment_t *
erste_luecke(const uft_cbm_track_segmentierung_t *s)
{
    for (size_t i = 0; i < s->anzahl; i++)
        if (s->segmente[i].art == UFT_CBM_SEG_GAP) return &s->segmente[i];
    return NULL;
}

/** Wie viele Luecken einheitlich aus `byte` bestehen.
 *
 *  NACHGETRAGEN nach der Mutationsmatrix: `erste_luecke()` liefert immer
 *  Gap 1, also die Luecke hinter dem KOPF. Eine Mutation, die den
 *  Datenblock um ein GCR-Byte verkuerzt, verschiebt nur Gap 2 — und
 *  rutschte deshalb durch. Das ist die Klasse MF-1014: eine Gegenprobe,
 *  die aus dem falschen Grund gruen ist. Geprueft werden jetzt ALLE. */
static unsigned luecken_einheitlich_mit(
    const uft_cbm_track_segmentierung_t *s, uint8_t byte)
{
    unsigned n = 0;
    for (size_t i = 0; i < s->anzahl; i++) {
        const uft_cbm_segment_t *e = &s->segmente[i];
        if (e->art != UFT_CBM_SEG_GAP) continue;
        if (e->einheitlich && e->fuellbyte == byte) n++;
    }
    return n;
}

int main(void)
{
    printf("Jedes Bit gehoert zu genau einem Segment (MF-1333)\n");

    static uint8_t spur[SPUR_LEN + 8];
    uft_cbm_track_segmentierung_t s;

    /* ── 1. Vorbedingung: der Spurbau trifft seine eigene Rechnung ──*/
    size_t len = spur_bauen(spur, 18, muster_standard);
    if (len != SPUR_LEN) {
        printf("  [ROT] Spurbau lieferte %zu Byte, gerechnet %d — "
               "Vorbedingung traegt nicht\n", len, SPUR_LEN);
        return 1;
    }
    zusage(1, "Spurbau: 21 x 363 = 7623 Byte, wie gerechnet");

    /* ── 2. Die Bilanz ──────────────────────────────────────────────*/
    {
        uft_error_t rc = uft_cbm_track_segmentieren(spur, len, 0, &s);
        zusage(rc == UFT_OK, "Zerlegung gelingt");
        zusage(bilanz_geht_auf(&s),
               "Bilanz: lueckenlos, ueberlappungsfrei, Summe = Spurlaenge");

        /* Je Sektor: Sync, Kopf, Gap, Sync, Daten, Gap = 6 Segmente. */
        zusage(s.anzahl == (size_t)SEKTOREN * 6,
               "126 Segmente — sechs je Sektor");
        zusage(s.sync_laeufe == SEKTOREN * 2,
               "42 Sync-Laeufe — zwei je Sektor");
        zusage(s.koepfe == SEKTOREN, "21 Kopfbloecke");
        zusage(s.datenbloecke == SEKTOREN, "21 Datenbloecke");
        zusage(s.luecken == SEKTOREN * 2, "42 Luecken");
        zusage(s.unbekannt == 0, "nichts blieb unbekannt");

        /* ── 3. Die Koepfe nennen ihre eigene Stelle ───────────────*/
        int koepfe_richtig = 0, erwartet_sektor = 0;
        for (size_t i = 0; i < s.anzahl; i++) {
            if (s.segmente[i].art != UFT_CBM_SEG_HEADER) continue;
            if (s.segmente[i].kopf_gelesen &&
                s.segmente[i].kopf_spur == 18 &&
                s.segmente[i].kopf_sektor == erwartet_sektor)
                koepfe_richtig++;
            erwartet_sektor++;
        }
        zusage(koepfe_richtig == SEKTOREN,
               "alle 21 Koepfe nennen Spur 18 und ihren eigenen Sektor");

        /* ── 4. Die Luecken werden vermessen ───────────────────────
         *
         * ACHTUNG, hier stand zuerst `== GAP_LEN` (9), und die Messung
         * hat die Erwartung widerlegt — nicht den Code. 0x55 endet auf
         * einem 1-Bit; das gehoert zum Sync-Lauf dahinter, also ist die
         * Luecke 71 Bit lang und traegt 8 GANZE Bytes. Der angrenzende
         * Sync misst entsprechend 41 Bit statt 40.
         *
         * Diese Zusage nagelt beide Haelften fest, damit niemand die
         * Grenze spaeter stillschweigend verschiebt. */
        const uft_cbm_segment_t *g = erste_luecke(&s);
        zusage(g != NULL && g->bit_len == GAP_LEN * 8 - 1,
               "Luecke aus 0x55: 71 Bit — ein 1-Bit ging an den Sync");
        zusage(g != NULL && g->byte_zahl == GAP_LEN - 1,
               "Luecke aus 0x55: 8 ganze Bytes");
        zusage(s.segmente[0].art == UFT_CBM_SEG_SYNC &&
               s.segmente[0].bit_len == SYNC_LEN * 8,
               "erster Sync der Spur: volle 40 Bit (keine Luecke davor)");
        zusage(s.segmente[3].art == UFT_CBM_SEG_SYNC &&
               s.segmente[3].bit_len == SYNC_LEN * 8 + 1,
               "Sync nach einer 0x55-Luecke: 41 Bit — das Gegenstueck");
        zusage(g != NULL && g->fuellbyte == 0x55 && g->einheitlich,
               "Standardformat: einheitlich 0x55");
        zusage(g != NULL && g->zahl_55 == GAP_LEN - 1 && g->zahl_67 == 0,
               "Standardformat: 8 mal 0x55, 0 mal 0x67");
        /* ALLE 42, nicht nur die erste — sonst bleibt Gap 2 ungeprueft
         * und eine falsche Datenblocklaenge faellt nicht auf. */
        zusage(luecken_einheitlich_mit(&s, 0x55) == SEKTOREN * 2,
               "alle 42 Luecken sind einheitlich 0x55 (auch die hinter Daten)");

        uft_cbm_segmentierung_freigeben(&s);
    }

    /* ── 5. GeoCopy-Nachbau: durchgehend 0x67 ──────────────────────*/
    {
        spur_bauen(spur, 21, muster_geocopy);
        uft_cbm_track_segmentieren(spur, SPUR_LEN, 0, &s);
        const uft_cbm_segment_t *g = erste_luecke(&s);
        zusage(bilanz_geht_auf(&s), "GeoCopy-Muster: Bilanz geht auf");
        zusage(g != NULL && g->fuellbyte == 0x67 && g->einheitlich,
               "GeoCopy-Nachbau: einheitlich 0x67");
        /* 0x67 endet auf DREI 1-Bits — die Luecke misst 69 Bit, nicht
         * 71 wie bei 0x55. Dass die Zahl vom FUELLBYTE abhaengt, ist
         * genau der Punkt, den ein byteweiser Zerleger verdeckt. */
        zusage(g != NULL && g->bit_len == GAP_LEN * 8 - 3,
               "Luecke aus 0x67: 69 Bit — drei 1-Bits gingen an den Sync");
        zusage(g != NULL && g->zahl_67 == GAP_LEN - 1 && g->zahl_55 == 0,
               "GeoCopy-Nachbau: 8 mal 0x67, 0 mal 0x55");
        uft_cbm_segmentierung_freigeben(&s);
    }

    /* ── 6. Original: gemischt, jedes dritte Byte 0x67 ─────────────*/
    {
        spur_bauen(spur, 21, muster_original);
        uft_cbm_track_segmentieren(spur, SPUR_LEN, 0, &s);
        const uft_cbm_segment_t *g = erste_luecke(&s);
        zusage(bilanz_geht_auf(&s), "Originalmuster: Bilanz geht auf");
        zusage(g != NULL && !g->einheitlich,
               "Original: NICHT einheitlich — das unterscheidet es");
        /* Geschrieben: 55 55 67 55 55 67 55 55 67. Das letzte 0x67
         * verliert drei Bits an den Sync, gelesen werden also die
         * ersten acht: 55 55 67 55 55 67 55 55. */
        zusage(g != NULL && g->zahl_55 == 6 && g->zahl_67 == 2,
               "Original: 6 mal 0x55 und 2 mal 0x67 in 8 lesbaren Byte");
        zusage(g != NULL && g->fuellbyte == 0x55,
               "Original: haeufigstes Byte ist 0x55, nicht 0x67");
        uft_cbm_segmentierung_freigeben(&s);
    }

    /* ── 7. Absagen statt raten ────────────────────────────────────*/
    {
        uft_cbm_track_segmentierung_t leer;
        zusage(uft_cbm_track_segmentieren(NULL, 10, 0, &leer)
               == UFT_ERR_INVALID_ARG,
               "Nullzeiger wird abgesagt");
        zusage(uft_cbm_track_segmentieren(spur, 10, 0, NULL)
               == UFT_ERR_INVALID_ARG,
               "fehlendes Ziel wird abgesagt");
        zusage(uft_cbm_track_segmentieren(spur, 10, 1000, &leer)
               == UFT_ERR_INVALID_ARG,
               "mehr Bits als Bytes: abgesagt statt gekappt");
    }

    /* ── 8. Eine Spur ohne jeden Sync ist EINE Luecke ──────────────*/
    {
        static uint8_t flach[256];
        memset(flach, 0x55, sizeof flach);
        uft_cbm_track_segmentieren(flach, sizeof flach, 0, &s);
        zusage(bilanz_geht_auf(&s), "Spur ohne Sync: Bilanz geht auf");
        zusage(s.anzahl == 1 && s.segmente[0].art == UFT_CBM_SEG_GAP,
               "Spur ohne Sync ist genau EIN Lueckensegment");
        zusage(s.segmente[0].byte_zahl == 256 &&
               s.segmente[0].fuellbyte == 0x55,
               "und sie wird vollstaendig vermessen (256 Byte 0x55)");
        uft_cbm_segmentierung_freigeben(&s);
    }

    /* ── 9. Ein Sync ohne lesbaren Block heisst `unbekannt` ────────*/
    {
        static uint8_t wirr[64];
        memset(wirr, 0x00, sizeof wirr);
        memset(wirr + 8, 0xFF, 5);       /* Sync, dann lauter Nullen */
        uft_cbm_track_segmentieren(wirr, sizeof wirr, 0, &s);
        zusage(bilanz_geht_auf(&s), "wirre Spur: Bilanz geht auf");
        zusage(s.unbekannt >= 1,
               "Sync ohne Kennung: als `unbekannt` gemeldet, nicht geraten");
        zusage(s.koepfe == 0 && s.datenbloecke == 0,
               "und es wird kein Block erfunden");
        uft_cbm_segmentierung_freigeben(&s);
    }

    /* ── 10. Die Bitprobe: um drei Bit verschoben ──────────────────
     * Der schaerfste Fall. Ein byteweiser Zerleger faellt hier um. */
    {
        spur_bauen(spur, 18, muster_geocopy);
        static uint8_t versetzt[SPUR_LEN + 8];
        bits_schieben(spur, SPUR_LEN, 3, versetzt);

        uft_error_t rc = uft_cbm_track_segmentieren(
            versetzt, SPUR_LEN + 1, (size_t)SPUR_LEN * 8 + 3, &s);
        zusage(rc == UFT_OK, "um 3 Bit versetzte Spur: Zerlegung gelingt");
        zusage(bilanz_geht_auf(&s), "um 3 Bit versetzt: Bilanz geht auf");
        zusage(s.koepfe == SEKTOREN && s.datenbloecke == SEKTOREN,
               "um 3 Bit versetzt: alle 21 Koepfe und 21 Datenbloecke");

        int treffer = 0, sektor = 0;
        for (size_t i = 0; i < s.anzahl; i++) {
            if (s.segmente[i].art != UFT_CBM_SEG_HEADER) continue;
            if (s.segmente[i].kopf_spur == 18 &&
                s.segmente[i].kopf_sektor == sektor) treffer++;
            sektor++;
        }
        zusage(treffer == SEKTOREN,
               "um 3 Bit versetzt: jeder Kopf nennt weiter seine Stelle");

        /* NACHGETRAGEN nach der Matrix: ohne diese Zusage rutscht eine
         * Mutation durch, die die Luecke erst ab der naechsten
         * BYTEGRENZE liest. In der unverschobenen Spur faellt das nicht
         * auf, weil dort jede Luecke ohnehin auf einer Bytegrenze
         * beginnt (Gap 1 bei Bit 120). Erst der Versatz trennt beides. */
        zusage(luecken_einheitlich_mit(&s, 0x67) == SEKTOREN * 2,
               "um 3 Bit versetzt: alle 42 Luecken weiter einheitlich 0x67");
        uft_cbm_segmentierung_freigeben(&s);
    }

    printf("%s\n", fehler ? "FEHLGESCHLAGEN" : "BESTANDEN (0 Fehler)");
    return fehler ? 1 : 0;
}
