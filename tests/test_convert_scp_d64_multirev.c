/**
 * @file test_convert_scp_d64_multirev.c
 * @brief SCP -> D64 lieferte eine leere Diskette und meldete Erfolg (MF-565)
 *
 * -- Was gemessen wurde --------------------------------------------------
 *
 * Kein Test im Baum fasste `uftc_convert_scp_to_d64()` je an. Dieser hier
 * baut eine fehlerfreie synthetische C64-Aufnahme (D64 -> GCR -> Fluss ->
 * SCP) und wandelt sie zurueck. Vorher:
 *
 *     0 von 683 Sektoren byteweise richtig, Rueckgabe UFT_OK,
 *     success = true, 174848 Byte Ausgabe -- vollstaendig Fuellbyte.
 *
 * Drei Ursachen, alle gemessen:
 *
 *   1. **Die Zonen-Zellzeiten standen verkehrt herum.**
 *      `D64_ZONE_0` heisst laut Enum "21 sectors, fastest" und trug 4,0 us
 *      -- die Zeit der LANGSAMSTEN Zone. Der Schreiber legte den Fluss mit
 *      4,0 us, der Leser stellte den PLL nach `uft_c64_track_bitrate()` auf
 *      3,25 us. 23 % auseinander. Gegenprobe: 7692 Byte * 8 * 3,25 us =
 *      200,0 ms = eine Umdrehung bei 300 U/min; mit 4,0 us waeren es
 *      246,1 ms.
 *
 *   2. **Der Sektor-Parser war ein Stub.** `uft_c64_parser_add_bit()` schob
 *      Bits in ein Schieberegister; "Full sync-pattern detection + sector
 *      extraction deferred" stand als Kommentar daneben. Der Baum hat seit
 *      MF-536 einen gegen VICE geprueften GCR-Dekoder -- der Fluss-Pfad war
 *      der einzige, der ihn nicht benutzte.
 *
 *   3. **Nur eine Umdrehung wurde dekodiert**, ausgewaehlt nach `max_flux`.
 *      Eine Umdrehung mit ZUSAETZLICHEN Uebergaengen -- Rauschen, schwaches
 *      Bit, Schreibnaht -- hat mehr Fluss als eine saubere. Das Kriterium
 *      waehlte also unter Umstaenden gerade die schlechteste, und die
 *      uebrigen blieben ungenutzt in der Datei liegen. Der Amiga-Zwilling
 *      in derselben Datei laeuft seit MF-473 ueber alle Umdrehungen.
 *
 * Und `decode_retries` -- dokumentiert als "Sector extraction (Flux ->
 * Sector)", Vorgabe 5 -- wurde ausgerechnet in dieser Funktion in eine
 * lokale Variable gelesen und nie benutzt. Ein Vorkommen im ganzen Baum.
 *
 * -- Was der Test prueft -------------------------------------------------
 *
 * Jede Messung mit ihrer Bezugsgroesse davor, sonst ist sie keine:
 *
 *   BEZUG        eine saubere Umdrehung          -> muss 683/683 ergeben
 *   MEHRFACH     drei, die letzte verrauscht     -> darf nicht schlechter sein
 *   NUR-RAUSCH   die verrauschte allein          -> Bezug fuer den naechsten
 *   DANN-SAUBER  verrauschte zuerst, saubere dahinter
 *                                                -> muss deutlich besser sein
 *
 * Die letzte Zeile ist der eigentliche Beweis: sie faellt nur gruen aus,
 * wenn spaetere Umdrehungen wirklich angefasst werden.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_convert.h"
#include "uft/formats/c64/uft_d64_g64.h"
#include "uft/uft_d64_writer.h"
#include "uft/formats/uft_scp_writer.h"
#include "uft/uft_format_parsers.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern uft_error_t uftc_convert_scp_to_d64(const uint8_t *src_data,
                                           size_t src_size,
                                           const char *dst_path,
                                           const uft_convert_options_ext_t *opts,
                                           uft_convert_result_t *result);

#define D64_SIZE   174848
#define SECSZ      256
#define NREVS      3

static int failures;
static uint8_t g_d64[D64_SIZE];
static uint8_t g_out[D64_SIZE * 2];

/** Sektoren je Spur, 1-basiert (Commodore-Zonen). */
static int spt(int track)
{
    if (track <= 17) return 21;
    if (track <= 24) return 19;
    if (track <= 30) return 18;
    return 17;
}

static d64_speed_zone_t zone_of(int track)
{
    if (track <= 17) return D64_ZONE_0;
    if (track <= 24) return D64_ZONE_1;
    if (track <= 30) return D64_ZONE_2;
    return D64_ZONE_3;
}

/** Versatz eines Sektors in der D64. */
static size_t d64_off(int track, int sec)
{
    size_t off = 0;
    for (int t = 1; t < track; t++) off += (size_t)spt(t) * SECSZ;
    return off + (size_t)sec * SECSZ;
}

static size_t slurp(const char *p, uint8_t *dst, size_t cap)
{
    FILE *f = fopen(p, "rb");
    if (!f) return 0;
    size_t n = fread(dst, 1, cap, f);
    fclose(f);
    return n;
}

/**
 * Wandelt eine SCP und zaehlt die byteweise richtigen Sektoren.
 *
 * @param nrevs      wie viele Umdrehungen in die SCP geschrieben werden
 * @param noisy_rev  welche davon Rauschen traegt, oder -1 fuer keine
 * @param out_total  Anzahl geprueften Sektoren (Bezugsgroesse)
 * @param out_r      Bericht des Wandlers, zum Abgleich gegen die Datei
 * @return byteweise richtige Sektoren, oder -1 wenn der Aufbau scheiterte
 */
static int convert_and_count(int nrevs, int noisy_rev, int *out_total,
                             uft_convert_result_t *out_r)
{
    const uint8_t disk_id[2] = { 'M', 'F' };
    static uint8_t gcr[16384];
    static uint32_t flux[131072];
    static uint32_t noisy[131072];

    scp_writer_t *w = scp_writer_create(SCP_TYPE_C64, (uint8_t)nrevs);
    if (!w) return -1;

    for (int track = 1; track <= 35; track++) {
        const uint8_t *ptr[21];
        int n = spt(track);
        for (int s = 0; s < n; s++) ptr[s] = &g_d64[d64_off(track, s)];

        size_t gcr_len = build_gcr_track(ptr, n, gcr, track, disk_id, 0x55);
        if (!gcr_len) continue;

        size_t fc = 0;
        if (d64_gcr_to_flux(gcr, gcr_len, zone_of(track), flux, &fc) != 0)
            continue;

        /* Ticks -> Nanosekunden, wie es der Schreiber erwartet (MF-537). */
        for (size_t i = 0; i < fc; i++) flux[i] *= 25u;

        uint32_t dur = 0;
        for (size_t i = 0; i < fc; i++) dur += flux[i];

        /* Die verrauschte Fassung: jeder zwanzigste Uebergang wird
         * halbiert und ein zusaetzlicher eingefuegt. Mehr Uebergaenge,
         * schlechter lesbar — genau die Falle fuer `max_flux`. */
        size_t nc = 0;
        for (size_t i = 0; i < fc && nc + 2 < 131072; i++) {
            if ((i % 20) == 19 && flux[i] > 400) {
                noisy[nc++] = flux[i] / 2;
                noisy[nc++] = flux[i] - flux[i] / 2;
            } else {
                noisy[nc++] = flux[i];
            }
        }
        uint32_t ndur = 0;
        for (size_t i = 0; i < nc; i++) ndur += noisy[i];

        /* scp_writer_add_track() nimmt den ZYLINDER und rechnet
         * `track_num * 2 + side` selbst — der Leser
         * (uft_scp_get_track_flux) nimmt dagegen den fertigen SCP-Index.
         * Zwei Zaehlweisen fuer dieselbe Datei; wer hier den Halbspur-
         * Index uebergibt, legt die Spuren doppelt so weit auseinander
         * und findet nur Spur 1 wieder (gemessen: 21 von 683). */
        int scp_track = track - 1;
        for (int r = 0; r < nrevs; r++) {
            if (r == noisy_rev)
                scp_writer_add_track(w, scp_track, 0, noisy, nc, ndur, r);
            else
                scp_writer_add_track(w, scp_track, 0, flux, fc, dur, r);
        }
    }

    const char *scp_path = "uft_mrev_d64.scp";
    remove(scp_path);
    if (scp_writer_save(w, scp_path) != 0) { scp_writer_free(w); return -1; }
    scp_writer_free(w);

    static uint8_t scp_blob[16u * 1024u * 1024u];
    size_t scp_len = slurp(scp_path, scp_blob, sizeof(scp_blob));
    if (!scp_len) return -1;

    uft_convert_options_ext_t o;
    memset(&o, 0, sizeof(o));
    memset(out_r, 0, sizeof(*out_r));
    o.accept_data_loss = true;

    const char *out = "uft_mrev_out.d64";
    remove(out);
    uftc_convert_scp_to_d64(scp_blob, scp_len, out, &o, out_r);
    size_t n_out = slurp(out, g_out, sizeof(g_out));

    int total = 0, good = 0;
    for (int track = 1; track <= 35; track++) {
        int n = spt(track);
        for (int s = 0; s < n; s++) {
            size_t off = d64_off(track, s);
            if (off + SECSZ > n_out) continue;
            total++;
            if (memcmp(&g_out[off], &g_d64[off], SECSZ) == 0) good++;
        }
    }
    remove(scp_path);
    remove(out);
    *out_total = total;
    return good;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== Der D64-Pfad wirft die uebrigen Umdrehungen weg (MF-565) ===\n");

    /* ── Quelle: eine D64 mit erkennbarem Inhalt ────────────────────── */
    for (size_t i = 0; i < D64_SIZE; i++)
        g_d64[i] = (uint8_t)(i * 7 + (i >> 8));


    /* ── Bezugsgroesse zuerst ───────────────────────────────────────────
     *
     * Eine Umdrehung, sauber. Was hier herauskommt, ist das Beste, was der
     * Pfad ueberhaupt kann. Erst danach hat die Mehrfach-Messung eine
     * Bedeutung: sie kann nur schlechter sein als diese Zahl, nie besser.
     *
     * Ohne diese Zeile waere „0 von 683" eine Aussage ueber gar nichts. */
    {
        int total = 0;
        uft_convert_result_t r;
        int good = convert_and_count(1, -1, &total, &r);
        if (good < 0) { printf("  Aufbau schlug fehl\n"); return 2; }
        printf("  BEZUG   eine saubere Umdrehung: %d von %d Sektoren "
               "byteweise richtig (%.1f %%)\n",
               good, total, total ? 100.0 * good / total : 0.0);
        printf("          (Wandler meldet %d gewandelt / %d gescheitert)\n",
               r.sectors_converted, r.sectors_failed);
        if (good == 0) {
            printf("  FAIL: der SCP->D64-Pfad findet auf einer SAUBEREN\n"
                   "        Aufnahme keinen einzigen Sektor. Die Umdrehungs-\n"
                   "        wahl ist dann nicht das Problem, sondern der\n"
                   "        Dekoder selbst.\n");
            failures++;
        }
    }

    /* ── Und jetzt drei Umdrehungen, davon eine verrauscht ─────────────
     *
     * Umdrehung 0 und 1 sauber, Umdrehung 2 mit Rauschen — und damit mit
     * dem HOECHSTEN Flusszaehler. Der alte Wandler suchte `max_flux` und
     * dekodierte genau diese eine, also die schlechteste. Das Ergebnis
     * darf nicht schlechter sein als die Bezugsgroesse: mehr Aufnahmen
     * duerfen nie weniger Sektoren ergeben. */
    {
        int total = 0;
        uft_convert_result_t r;
        int good = convert_and_count(NREVS, NREVS - 1, &total, &r);
        if (good < 0) { printf("  Aufbau schlug fehl\n"); return 2; }
        printf("  MEHRFACH %d Umdrehungen (Nr. %d verrauscht): %d von %d "
               "byteweise richtig (%.1f %%)\n",
               NREVS, NREVS - 1, good, total,
               total ? 100.0 * good / total : 0.0);
        printf("           (Wandler meldet %d gewandelt / %d gescheitert)\n",
               r.sectors_converted, r.sectors_failed);

        if (good < total * 95 / 100) {
            printf("  FAIL: %d von %d Sektoren fehlen, obwohl ZWEI saubere\n"
                   "        Umdrehungen in der Datei liegen.\n",
                   total - good, total);
            failures++;
        } else {
            printf("  ok   die sauberen Umdrehungen werden genutzt\n");
        }

        /* Die Buchhaltung muss zur Datei passen. Eine Zahl, die mehr
         * meldet als byteweise stimmt, ist genau die Sorte Aussage, die
         * diese Pruefsitzung sucht. */
        if (r.sectors_converted > good) {
            printf("  FAIL: gemeldet %d gewandelt, byteweise richtig sind "
                   "aber nur %d\n", r.sectors_converted, good);
            failures++;
        }
        if (r.sectors_converted + r.sectors_failed != total) {
            printf("  FAIL: %d + %d Sektoren gemeldet, die Diskette hat %d\n",
                   r.sectors_converted, r.sectors_failed, total);
            failures++;
        }
    }

    /* ── Der eigentliche Beweis: die verrauschte Umdrehung ZUERST ────────
     *
     * Oben war Umdrehung 0 sauber — der Wandler war nach der ersten fertig
     * und die uebrigen liefen gar nicht. Das beweist noch nicht, dass er
     * sie benutzen KANN.
     *
     * Hier ist Umdrehung 0 die verrauschte. Erst wird gemessen, was sie
     * allein hergibt (Bezugsgroesse fuer diesen Teil), dann dieselbe
     * Umdrehung mit zwei sauberen dahinter. Die zweite Zahl muss groesser
     * sein — sonst werden die spaeteren Umdrehungen nicht angefasst. */
    {
        int t_alone = 0, t_all = 0;
        uft_convert_result_t r_alone, r_all;
        int alone = convert_and_count(1, 0, &t_alone, &r_alone);
        int all   = convert_and_count(NREVS, 0, &t_all, &r_all);
        if (alone < 0 || all < 0) { printf("  Aufbau schlug fehl\n"); return 2; }

        printf("  NUR-RAUSCH  die verrauschte Umdrehung allein: %d von %d "
               "(%.1f %%), gemeldet %d gewandelt\n", alone, t_alone,
               t_alone ? 100.0 * alone / t_alone : 0.0,
               r_alone.sectors_converted);
        printf("  DANN-SAUBER dieselbe, mit zwei sauberen dahinter: %d von "
               "%d (%.1f %%)\n", all, t_all,
               t_all ? 100.0 * all / t_all : 0.0);

        if (alone >= t_alone) {
            /* Kein Rotbeweis ohne Schaden: wenn das Rauschen nichts
             * kaputt macht, misst dieser Teil gar nichts. Dann ist der
             * Test schuld, nicht der Wandler. */
            printf("  FAIL: das eingebaute Rauschen kostet keinen einzigen "
                   "Sektor —\n        dieser Teil des Tests misst nichts.\n");
            failures++;
        } else if (all <= alone) {
            printf("  FAIL: zwei saubere Umdrehungen dahinter bringen "
                   "nichts (%d -> %d).\n        Die spaeteren Umdrehungen "
                   "werden nicht angefasst.\n", alone, all);
            failures++;
        } else {
            printf("  ok   die spaeteren Umdrehungen holen %d Sektoren "
                   "nach\n", all - alone);
        }

        /* Und die Nicht-Verschlimmerungs-Garantie: mehr Aufnahmen duerfen
         * nie weniger ergeben. */
        if (all < t_all * 95 / 100) {
            printf("  FAIL: mit zwei sauberen Umdrehungen fehlen immer noch "
                   "%d von %d\n", t_all - all, t_all);
            failures++;
        }

        /* ── Ein eigener, gemessener Befund (MF-565) ────────────────────
         *
         * Die verrauschte Umdrehung allein liefert 0 byteweise richtige
         * Sektoren — der Wandler meldet aber welche als gewandelt. Das
         * ist kein Rechenfehler, sondern die Grenze des Formats: die
         * CBM-Datenpruefsumme ist EIN XOR-Byte. Ein zerstoerter Sektor
         * besteht sie mit Wahrscheinlichkeit 1/256.
         *
         *     683 Sektoren / 256 = 2,7 erwartete Zufallstreffer
         *     gemessen:             3
         *
         * Folge: diese drei gelten als „geprueft", und die sauberen
         * Umdrehungen dahinter duerfen sie nicht mehr ersetzen — daher
         * 680 statt 683. Ein Wandler kann das aus EINER Lesung nicht
         * besser wissen; die Abhilfe ist Uebereinstimmung ueber mehrere
         * Umdrehungen statt Vertrauen in ein Pruefbyte. Das ist eigene
         * Arbeit mit eigenem Rotbeweis (BACKLOG C-9).
         *
         * Hier wird nur festgenagelt, dass es nicht SCHLECHTER wird. */
        if (r_alone.sectors_converted > 6) {
            printf("  FAIL: %d Sektoren als gewandelt gemeldet, byteweise "
                   "richtig sind %d.\n        Erwartet waren rund 683/256 = "
                   "2,7 Zufallstreffer der XOR-Pruefsumme;\n        so viele "
                   "sind eine andere Ursache.\n",
                   r_alone.sectors_converted, alone);
            failures++;
        }
    }

    printf("\n%s (%d Abweichungen)\n",
           failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
