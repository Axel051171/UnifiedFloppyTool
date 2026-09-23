/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * test_d64_gap_fuellbyte_erreicht_die_luecken.c — die oeffentliche
 * Option `gap_fill` muss die SEKTORLUECKEN faerben, nicht nur die
 * Auffuellung am Spurende (MF-1333, Stufe 3).
 *
 * ── Der gemessene Defekt ─────────────────────────────────────────────
 * `convert_options_t.gap_fill` gibt es seit langem
 * (`include/uft/formats/c64/uft_d64_g64.h:159`, Vorgabe 0x55 in
 * `uft_d64_g64.c:1082`), und es wurde bis `build_gcr_track()`
 * durchgereicht. Dort faerbte es die AUFFUELLUNG auf Zonenkapazitaet —
 * und sonst nichts. Die beiden Luecken je Sektor standen zweimal fest
 * verdrahtet im Code:
 *
 *     sector_to_gcr()   memset(out, 0x55, 9) und memset(out, 0x55, gap_len)
 *     d64_write_gap()   memset(output, 0x55, count)
 *
 * Und der ZWEITE Weg ist der NORMALFALL: eine vollstaendige Spur laeuft
 * ueber `d64_write_track_gcr()` (MF-862), nicht ueber die Schleife mit
 * `sector_to_gcr()`. Wer nur die erste Stelle repariert, hat nichts
 * repariert — genau die Gestalt von MF-519/MF-529 ("eine Stelle geholt,
 * den Nachbarn uebersehen"), die dieser Baum dreimal gesehen hat.
 *
 * ── Warum das gebraucht wird ─────────────────────────────────────────
 * Der GEOS-Bootschutz IST der Lueckeninhalt auf Spur 21. Beschreibung
 * des Urhebers (Christian Meilinger, `neue-ideen/geocopy.zip ->
 * geocopy/READ.ME.cvt`; Kanal *Spec* nach MF-695, NC-Klausel, kein
 * Port): Standardformatierung `$55 …`, GeoCopy-Nachbau durchgehend
 * `$67`. Ohne ein durchgereichtes Fuellbyte kann UFT eine
 * GeoCopy-kompatible Spur gar nicht schreiben.
 *
 * ── Wie gemessen wird ────────────────────────────────────────────────
 * NICHT mit einem Bytevergleich an geratenen Versaetzen, sondern mit
 * dem Zerleger aus Stufe 2 (`uft_cbm_track_segmentieren`). Der findet
 * die Luecken STRUKTURELL — zwischen Blockende und naechstem Sync —
 * und ist gegen einen anderen Erzeuger abgenommen. Zwei Stufen, die
 * sich gegenseitig pruefen.
 */

#include <stdio.h>
#include <string.h>

#include "uft/formats/c64/uft_d64_g64.h"
#include "uft/formats/cbm/uft_cbm_track_segment.h"
#include "uft/uft_d64_writer.h"

static int fehler = 0;

static void zusage(int ok, const char *was)
{
    printf("  %-66s %s\n", was, ok ? "ok" : "[ROT]");
    if (!ok) fehler++;
}

#define SEKTOREN 21          /* Zone 0: Spuren 1..17 */
#define SPUR      17
#define KAPAZITAET 7692      /* src/formats/uft_d64_writer.c:44 */

static uint8_t g_sektoren[SEKTOREN][256];
static const uint8_t *g_zeiger[SEKTOREN];
static uint8_t g_spur[8192];

static void sektoren_fuellen(void)
{
    for (int s = 0; s < SEKTOREN; s++) {
        for (int k = 0; k < 256; k++)
            g_sektoren[s][k] = (uint8_t)(s * 13 + k);
        g_zeiger[s] = g_sektoren[s];
    }
}

/** Zaehlt die Luecken, die einheitlich aus `byte` bestehen. */
static unsigned luecken_mit(const uft_cbm_track_segmentierung_t *s,
                            uint8_t byte)
{
    unsigned n = 0;
    for (size_t i = 0; i < s->anzahl; i++) {
        const uft_cbm_segment_t *e = &s->segmente[i];
        if (e->art == UFT_CBM_SEG_GAP && e->einheitlich &&
            e->fuellbyte == byte)
            n++;
    }
    return n;
}

int main(void)
{
    printf("Das Lueckenbyte erreicht die Sektorluecken (MF-1333)\n");

    const uint8_t disk_id[2] = { 0x41, 0x42 };
    sektoren_fuellen();

    /* ── 1. Vorbedingung: eine volle Spur entsteht ueberhaupt ───────*/
    size_t len = build_gcr_track(g_zeiger, SEKTOREN, g_spur, SPUR,
                                 disk_id, 0x55);
    if (len != KAPAZITAET) {
        printf("  [ROT] Spur ist %zu Byte, Zonenkapazitaet %d — "
               "Vorbedingung traegt nicht\n", len, KAPAZITAET);
        return 1;
    }
    zusage(1, "volle Spur: 7692 Byte, genau Zonenkapazitaet");

    /* ── 2. Standardformatierung: alle Luecken 0x55 ─────────────────*/
    {
        uft_cbm_track_segmentierung_t s;
        uft_error_t rc = uft_cbm_track_segmentieren(g_spur, len, 0, &s);
        zusage(rc == UFT_OK, "0x55: Spur laesst sich zerlegen");
        zusage(s.koepfe == SEKTOREN && s.datenbloecke == SEKTOREN,
               "0x55: 21 Koepfe und 21 Datenbloecke");
        /* 42 Sektorluecken plus die Auffuellung am Spurende. */
        zusage(luecken_mit(&s, 0x55) >= SEKTOREN * 2,
               "0x55: mindestens 42 Luecken einheitlich 0x55");
        zusage(luecken_mit(&s, 0x67) == 0,
               "0x55: keine einzige Luecke traegt 0x67");
        uft_cbm_segmentierung_freigeben(&s);
    }

    /* ── 3. DIE ZUSAGE: 0x67 erreicht die Sektorluecken ─────────────
     * Vor MF-1333 kam hier 0 heraus, weil beide Erzeuger 0x55 fest
     * verdrahtet hatten — die Option faerbte nur das Spurende. */
    {
        memset(g_spur, 0, sizeof g_spur);
        size_t l2 = build_gcr_track(g_zeiger, SEKTOREN, g_spur, SPUR,
                                    disk_id, 0x67);
        zusage(l2 == KAPAZITAET, "0x67: Spur hat dieselbe Laenge");

        uft_cbm_track_segmentierung_t s;
        uft_cbm_track_segmentieren(g_spur, l2, 0, &s);
        zusage(s.koepfe == SEKTOREN && s.datenbloecke == SEKTOREN,
               "0x67: die Spur bleibt vollstaendig lesbar");
        zusage(luecken_mit(&s, 0x67) >= SEKTOREN * 2,
               "0x67: mindestens 42 Luecken einheitlich 0x67");
        zusage(luecken_mit(&s, 0x55) == 0,
               "0x67: KEINE Luecke traegt mehr 0x55");
        uft_cbm_segmentierung_freigeben(&s);
    }

    /* ── 4. Die Sektordaten bleiben unberuehrt ──────────────────────
     * Ein Fuellbyte darf die Nutzlast nicht anfassen. Geprueft ueber
     * den Rueckweg, an der Spur mit 0x67 aus Abschnitt 3. */
    {
        static uint8_t zurueck[256];
        int spur_aus = -1, sektor_aus = -1;
        uint8_t id_aus[2] = { 0, 0 };
        d64_error_t e = D64_ERR_OK;
        int rc = gcr_to_sector(g_spur, KAPAZITAET, zurueck,
                               &spur_aus, &sektor_aus, id_aus, &e);
        zusage(rc == 0 && e == D64_ERR_OK,
               "0x67: der erste Sektor laesst sich zurueckdekodieren");
        zusage(spur_aus == SPUR && sektor_aus == 0,
               "0x67: er nennt Spur 17 und Sektor 0");
        zusage(memcmp(zurueck, g_sektoren[0], 256) == 0,
               "0x67: er ist byteidentisch — Nutzlast unberuehrt");
    }

    /* ── 4b. Der RUECKFALLPFAD faerbt ebenso ────────────────────────
     * Fehlt ein Sektor, laeuft `build_gcr_track()` NICHT ueber
     * `d64_write_track_gcr()`, sondern ueber seine eigene Schleife mit
     * `sector_to_gcr_gap()`. Das sind zwei verschiedene Erzeuger, und
     * genau deshalb muss die Zusage an BEIDEN haengen — sonst ist sie
     * die Haelfte wert (MF-519/MF-529: eine Stelle geholt, den Nachbarn
     * uebersehen). Eine unlesbare Spur ist bei einer geschuetzten
     * Diskette der Normalfall, nicht die Ausnahme. */
    {
        const uint8_t *loechrig[SEKTOREN];
        for (int s = 0; s < SEKTOREN; s++) loechrig[s] = g_zeiger[s];
        loechrig[7] = NULL;                 /* Sektor 7 fehlt */

        memset(g_spur, 0, sizeof g_spur);
        size_t l3 = build_gcr_track(loechrig, SEKTOREN, g_spur, SPUR,
                                    disk_id, 0x67);
        zusage(l3 == KAPAZITAET,
               "Rueckfallpfad: Spur wird auf Zonenkapazitaet aufgefuellt");

        uft_cbm_track_segmentierung_t s;
        uft_cbm_track_segmentieren(g_spur, l3, 0, &s);
        zusage(s.koepfe == SEKTOREN - 1 && s.datenbloecke == SEKTOREN - 1,
               "Rueckfallpfad: 20 Koepfe — der fehlende wird nicht erfunden");
        zusage(luecken_mit(&s, 0x67) >= (SEKTOREN - 1) * 2,
               "Rueckfallpfad: auch hier traegt jede Luecke 0x67");
        zusage(luecken_mit(&s, 0x55) == 0,
               "Rueckfallpfad: keine Luecke traegt 0x55");
        uft_cbm_segmentierung_freigeben(&s);
    }

    /* ── 5. Eine zu lange Spur wird NICHT gepolstert ────────────────
     *
     * 21 Sektoren passen auf Spur 17 (Zone 0, 7692 Byte), auf Spur 35
     * (Zone 3, 6250 Byte) nicht. Vor MF-1333 lief das still durch:
     * `if (used < capacity)` hatte kein `else`, und die Rueckgabe sagte,
     * es sei gutgegangen.
     *
     * BERICHTIGT NOCH IN MF-1333: hier stand zuerst `zu_lang == 0`, also
     * eine harte Absage. Eine Messung hat das widerlegt —
     * `test_convert_via_plugin` wurde rot, weil `capacity_map` die Tafel
     * der 1541 ist und derselbe Erzeuger auch **D67** bedient, wo die
     * Spuren 18-24 ZWANZIG Sektoren tragen (20 x 363 = 7260 > 7142).
     * Die Absage haette sieben Spuren jeder D67 verworfen.
     *
     * Die Zusage lautet deshalb jetzt: die zurueckgegebene Laenge ist
     * die WIRKLICHE, es wird nicht auf eine falsche Kapazitaet gepolstert,
     * und die Beobachtung geht als Warnung hinaus. */
    {
        memset(g_spur, 0, sizeof g_spur);
        size_t zu_lang = build_gcr_track(g_zeiger, SEKTOREN, g_spur, 35,
                                         disk_id, 0x55);
        /* Keine feste Zahl: 21 Sektoren auf Spur 35 nehmen den
         * RUECKFALLPFAD (`d64_write_track_gcr()` weist die falsche
         * Sektorzahl ab), und der rechnet mit `gap_map[35]` statt mit
         * den festen 9. Die Zahl an dieser Stelle waere eine zweite
         * Kopie der Spurrechnung (MF-1177). Festgenagelt wird die
         * AUSSAGE. */
        zusage(zu_lang > 6250,
               "zu lange Spur: nicht auf die Zonenkapazitaet gepolstert");
        zusage(zu_lang <= 8192,
               "zu lange Spur: passt noch in den Puffer der Aufrufer");
    }

    /* ── 6. Die ALTEN Einstiege bleiben, wie sie waren ──────────────
     *
     * `sector_to_gcr()` und `d64_write_gap()` haben seit MF-1333 keinen
     * Produktionsaufrufer mehr — beide Wege gehen ueber die Fassung mit
     * Fuellbyte. Sie bleiben trotzdem stehen (MF-1077: Bestehendes wird
     * umgeschrieben, nicht entfernt), und ihr unveraendertes Verhalten
     * ist damit eine ZUSAGE.
     *
     * NACHGETRAGEN nach der Mutationsmatrix: ohne diese beiden Zusagen
     * konnte man beiden still 0x67 unterschieben, ohne dass ein Test
     * rot wurde. Eine bewahrte Schnittstelle, die niemand bewacht, ist
     * eine Einladung. */
    {
        static uint8_t einer[400];
        memset(einer, 0, sizeof einer);
        size_t l = sector_to_gcr(g_sektoren[3], einer, SPUR, 3,
                                 disk_id, D64_ERR_OK);
        zusage(l > 0, "sector_to_gcr(): liefert weiter eine Spur");

        uft_cbm_track_segmentierung_t s;
        uft_cbm_track_segmentieren(einer, l, 0, &s);
        zusage(luecken_mit(&s, 0x55) >= 1 && luecken_mit(&s, 0x67) == 0,
               "sector_to_gcr(): schreibt unveraendert 0x55");
        uft_cbm_segmentierung_freigeben(&s);
    }
    {
        static uint8_t luecke[32];
        memset(luecke, 0xAA, sizeof luecke);
        d64_write_gap(luecke, 16);
        int alle_55 = 1;
        for (int k = 0; k < 16; k++) if (luecke[k] != 0x55) alle_55 = 0;
        zusage(alle_55, "d64_write_gap(): schreibt unveraendert 0x55");
        zusage(luecke[16] == 0xAA,
               "d64_write_gap(): schreibt keine Byte ueber `count` hinaus");
    }

    /* ── Was die Matrix NICHT isolieren konnte, und warum ───────────
     *
     * ZWEI Mutationen sind durchgerutscht. Beide sind nachgemessen und
     * benannt statt verschwiegen (Gestalt MF-1028: "neun von zehn, und
     * die zehnte ist benannt").
     *
     * (1) `if (sectors[s])` auf `if (1)` — also fehlende Sektoren
     *     ERFINDEN statt ueberspringen. `sector_to_gcr_gap()` prueft
     *     selbst `if (!sector_data …) return 0`, schreibt also nichts
     *     und ruecke den Zeiger nicht vor. Beide Sicherungen halten die
     *     Zusage je ALLEIN; nur wer beide entfernt, bricht sie. Das ist
     *     MF-1031 (zwei gegenseitig redundante obere Schranken im
     *     2MG-Leser). Die aeussere Abfrage bleibt stehen: sie sagt die
     *     Absicht an der Stelle, an der man sie liest.
     *
     * (2) `if (used > capacity)` auf `if (0)` — die Kapazitaetswarnung
     *     faellt weg. Nicht isolierbar, weil der Zweig seit der
     *     Berichtigung NUR NOCH WARNT: die zurueckgegebene Laenge ist in
     *     beiden Faellen dieselbe. Und `include/uft/uft_log.h` hat
     *     gemessen keinen Aufnahmehaken — die einzige oeffentliche
     *     Stellschraube ist `uft_log_set_level()`. Eine Warnung, die nur
     *     nach stderr geht, laesst sich von hier aus nicht festnageln.
     *
     *     Das ist eine Luecke im WERKZEUG, nicht im Vorsatz, und sie
     *     steht als `P3-537` im Baum. Der zweite, groessere Teil davon:
     *     `build_gcr_track()` kann die Kapazitaet seines AUSGABEPUFFERS
     *     gar nicht pruefen — die Signatur fuehrt sie nicht, und beide
     *     Aufrufer reichen einen festen `uint8_t[8192]` herein
     *     (`P3-536`). */

    printf("%s\n", fehler ? "FEHLGESCHLAGEN" : "BESTANDEN (0 Fehler)");
    return fehler ? 1 : 0;
}
