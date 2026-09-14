/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_scp_ueberlauf_sagt_ab.c
 * @brief Rotbeweis zu MF-1133 — ein 16,8-ms-Zwischenraum kam als
 *        KURZES Intervall heraus
 *
 * ── Der Befund ────────────────────────────────────────────────────────────
 *
 * `src/formats/scp/uft_scp_plugin.c` rechnete Ticks in Nanosekunden so:
 *
 *     uint32_t overflow = 0;
 *     ...
 *     overflow += 65536;
 *     uint32_t ticks = overflow + val;
 *     uint32_t ns = ticks * SCP_TICK_NS * (scp->header.resolution + 1);
 *
 * `SCP_TICK_NS` ist 25, und `header.resolution` ist ein `uint8_t` AUS
 * DER DATEI, den das Plugin nirgends prueft — gemessen ueber die Datei
 * drei Fundstellen: die Deklaration und zwei Rechnungen. `(resolution +
 * 1)` liegt damit in [1, 256] und der Faktor in [25, 6400].
 *
 * `ticks * 6400` laeuft ueber `UINT32_MAX`, sobald `ticks > 670 433` —
 * das sind **16,8 ms** Flusszeit. Und das ist kein Randfall: der
 * Nullwort-Marker dieses Formats existiert genau dafuer, Intervalle
 * jenseits 65 536 Ticks (1,6 ms) zu kodieren. Eine No-Flux-Flaeche, eine
 * unformatierte Spur oder eine Killer-Spur liefert zweistellige
 * Millisekunden.
 *
 * Die Folge war eine STILL FALSCHE Flusszeit — ein riesiger
 * Zwischenraum, gemeldet als kurzes Intervall. Fuer ein Werkzeug mit dem
 * Grundsatz „Keine stille Veraenderung. Keine erfundenen Daten." ist das
 * die schwerste Klasse.
 *
 * Dazu ein zweiter Ueberlauf in derselben Schleife: `overflow += 65536`
 * in einem `uint32_t` laeuft nach 65 536 Nullworten um.
 *
 * Und eine dritte Stelle, `track->metrics.index_time_ns`: dort geht eine
 * GANZE UMDREHUNG durch dieselbe Rechnung (rund 8 Mio. Ticks bei
 * 200 ms), und die Zeile darunter TEILT durch das Ergebnis
 * (`rpm = 60.0e9 / index_time_ns`). Ein auf 0 umgelaufener Wert waere
 * dort eine Division durch Null gewesen.
 *
 * ── Warum die Absage und nicht das Klemmen ────────────────────────────────
 *
 * Ein auf `UINT32_MAX` gesaettigtes Intervall waere eine erfundene Zahl
 * mit dem Anschein einer Messung — die Gestalt von MF-1040, wo ein
 * 70 000-Byte-Block still auf 65 535 fiel und als `UFT_SECTOR_OK`
 * gemeldet wurde. Der Flussstrom ist ein reines `uint32_t*` ohne
 * Kennzeichnungskanal; es gibt keine Stelle, an der „nicht darstellbar"
 * vermerkt werden koennte. Also wird abgesagt.
 *
 * Eine Schranke fuer `resolution` wird NICHT erfunden. Die echte
 * Bedingung ist, dass das ERGEBNIS passt, und genau die wird geprueft —
 * was Abschnitt 2 dieses Tests belegt: dieselbe Datei mit
 * `resolution = 0` wird gelesen, und das Intervall stimmt auf die
 * Nanosekunde.
 *
 * ── Der Weg durch die oeffentliche API ────────────────────────────────────
 *
 * Der Test oeffnet ueber `uft_disk_open()` — also ueber Erkennung und
 * Plugin-Auswahl, nicht ueber eine interne Funktion. Das ist Absicht:
 * MF-1039 hat an `cpm` gemessen, dass ein Plugin UEBER DIE ERKENNUNG
 * UNERREICHBAR sein kann, waehrend sein interner Leser richtig ist.
 */

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_disk.h"
/* `uft_disk_open()` steht in uft_core.h, NICHT in uft_disk.h — das
 * deklariert nur create/close/free/get_geometry.
 *
 * Ohne diesen Include nimmt gcc eine implizite `int`-Rueckgabe an und
 * kuerzt den 64-Bit-Zeiger auf 32 Bit. Gemessen: der Test stuerzte beim
 * ersten Zugriff auf `d->plugin` ab, obwohl `d != NULL` galt — weil `d`
 * ein halber Zeiger war. Genau die Falle, die dieser Baum notiert hat
 * („lokal mit -Werror=implicit-function-declaration pruefen: gcc warnt,
 * macOS clang bricht"). */
#include "uft/uft_core.h"
/* NICHT `uft/uft_track.h` einbinden: gemessen widersprechen sich
 * `uft_track.h` und `uft_format_plugin.h` in der Deklaration von
 * `uft_track_get_sector` ("conflicting types"), die beiden Header sind
 * also nicht gemeinsam einbindbar. Aufgeraeumt wird deshalb mit dem
 * kanonischen `uft_track_cleanup()` aus `uft_format_plugin.h`, der
 * bedingungslos freigibt. Eigener offener Punkt. */

static int gruen = 0;
static int rot = 0;

#define ZUSAGE(bed, text)                                                   \
    do {                                                                    \
        if (bed) { gruen++; printf("   [ok ] %s\n", (text)); }               \
        else     { rot++;   printf("   [ROT] %s\n", (text)); }               \
        assert(bed);                                                        \
    } while (0)

/* ══════════════════════════════════════════════════════════════════════
 * Die Pruefdatei
 * ══════════════════════════════════════════════════════════════════════
 *
 * Aufbau, gelesen aus `uft_scp_plugin.c`:
 *
 *   0x00  Kopf, 16 Byte: "SCP", version, disk_type, revolutions,
 *         start_track, end_track, flags, bitcell_width, heads,
 *         resolution, checksum(4)
 *   0x10  Versatztabelle, `end_track + 1` Eintraege a 4 Byte, ABSOLUT
 *         indiziert (MF-481)
 *   0x14  Spurkopf: "TRK" + Spurnummer
 *   0x18  Umdrehungskopf: duration(4), length(4), offset(4)
 *         — `offset` ist relativ zum SPURKOPF
 *   0x24  Flussworte, big-endian uint16
 */
#define NULLWORTE 11u                  /* je 65 536 Ticks */
#define REST      256u
#define TICKS     (NULLWORTE * 65536u + REST)     /* 721 152 */

static bool datei_bauen(const char *pfad, uint8_t resolution) {
    const uint32_t flusszahl = NULLWORTE + 1u;
    const uint32_t flussbytes = flusszahl * 2u;

    uint8_t buf[0x24 + 64];
    memset(buf, 0, sizeof(buf));

    /* Kopf */
    buf[0] = 'S'; buf[1] = 'C'; buf[2] = 'P';
    buf[3] = 0x19;            /* version 2.5 */
    buf[4] = 0x00;            /* disk_type: C64 */
    buf[5] = 1;               /* revolutions */
    buf[6] = 0;               /* start_track */
    buf[7] = 0;               /* end_track  -> Tabelle hat 1 Eintrag */
    buf[8] = 0;               /* flags */
    buf[9] = 0;               /* bitcell_width: 16 bit */
    buf[10] = 1;              /* heads: nur Seite 0 */
    buf[11] = resolution;     /* HIER sitzt der Faktor */
    /* buf[12..15] checksum bleibt 0 */

    /* Versatztabelle: Eintrag 0 -> Spurkopf bei 0x14 */
    buf[0x10] = 0x14; buf[0x11] = 0; buf[0x12] = 0; buf[0x13] = 0;

    /* Spurkopf */
    buf[0x14] = 'T'; buf[0x15] = 'R'; buf[0x16] = 'K'; buf[0x17] = 0;

    /* Umdrehungskopf: duration = TICKS, length = WORTZAHL,
     * offset = 0x24 - 0x14 = 0x10 (relativ zum Spurkopf).
     *
     * `length` ist eine WORTZAHL, keine Bytezahl — das steht im
     * Plugin: `size_t num_words = length; raw_bytes = num_words *
     * sizeof(uint16_t)`. Die erste Fassung dieses Tests trug hier die
     * Bytezahl; das Plugin las daraus doppelt so viele Byte, lief ueber
     * das Dateiende und gab `UFT_ERR_FILE_READ` — fuer BEIDE
     * Aufloesungen gleich. Der Test waere damit in Abschnitt 1 gruen
     * gewesen, ohne den Ueberlauf zu treffen: gruen aus dem falschen
     * Grund (Klasse MF-1014/1026/1028). */
    const uint32_t dauer = TICKS;
    memcpy(buf + 0x18, &dauer, 4);
    memcpy(buf + 0x1C, &flusszahl, 4);
    (void)flussbytes;
    const uint32_t rel = 0x10;
    memcpy(buf + 0x20, &rel, 4);

    /* Flussworte: NULLWORTE Nullen, dann REST — big-endian */
    size_t p = 0x24;
    for (uint32_t i = 0; i < NULLWORTE; i++) { buf[p++] = 0; buf[p++] = 0; }
    buf[p++] = (uint8_t)(REST >> 8);
    buf[p++] = (uint8_t)(REST & 0xFF);

    FILE *f = fopen(pfad, "wb");
    if (!f) return false;
    const bool ok = fwrite(buf, 1, p, f) == p;
    fclose(f);
    return ok;
}

/* ══════════════════════════════════════════════════════════════════════ */
extern const uft_format_plugin_t uft_format_plugin_scp;

/* `uft_disk_open()` braucht eine gefuellte Registry — ohne das liefert
 * jeder Pfad „kein Plugin" (MF-447: vor der Registrierung war die
 * Registry zur Laufzeit leer und `uft_disk_open()` gab fuer JEDE Datei
 * NULL).
 *
 * Registriert wird NUR dieses eine Plugin, nicht
 * `uft_register_all_formats()`: damit bleibt der Link klein, und die
 * Erkennung laeuft trotzdem ueber den echten Weg — Sonde, Auswahl,
 * `open`. Das ist ausdruecklich KEINE Spiegelung des Produktionspfads
 * (die waere nach der EINFRIER-REGEL nicht ausreichend:
 * „ein Messaufbau, der den Produktionspfad nachbaut statt ihn zu
 * benutzen"). `tests/test_cfi_schreibt_in_die_datei.c:145` spiegelt ihn
 * und sagt das auch. */
static void registrieren_einmal(void) {
    static bool getan = false;
    if (getan) return;
    getan = true;
    (void)uft_register_format_plugin(&uft_format_plugin_scp);
}

static uft_error_t spur_lesen(const char *pfad, uft_track_t *track,
                              const char **wer) {
    *wer = NULL;
    registrieren_einmal();
    uft_disk_t *d = uft_disk_open(pfad, true);
    if (!d) return UFT_ERROR_FILE_OPEN;


    if (!d->plugin || !d->plugin->read_track) {
        uft_disk_close(d);
        return UFT_ERROR_NOT_SUPPORTED;
    }
    *wer = d->plugin->name;

    memset(track, 0, sizeof(*track));
    const uft_error_t rc = d->plugin->read_track(d, 0, 0, track);
    uft_disk_close(d);
    return rc;
}

int main(void) {
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("MF-1133 — ein 16,8-ms-Zwischenraum darf nicht kurz werden\n");
    printf("   Ticks im Pruefstueck: %u (= %.1f ms bei 25 ns)\n",
           (unsigned)TICKS, (double)TICKS * 25.0 / 1e6);

    /* ── 1) resolution = 255: Faktor 6400, 4,6e9 ns, NICHT darstellbar */
    printf("\n1) resolution=255 (Faktor 6400) -> Absage\n");
    {
        const char *pfad = "mf1133_res255.scp";
        ZUSAGE(datei_bauen(pfad, 255), "Pruefdatei geschrieben");

        uft_track_t t;
        const char *wer = NULL;
        const uft_error_t rc = spur_lesen(pfad, &t, &wer);
        printf("   Plugin=%s rc=%d flux_count=%zu\n",
               wer ? wer : "(keins)", (int)rc, (size_t)t.flux_count);

        ZUSAGE(wer != NULL,
               "die Datei wird ueber uft_disk_open() einem Plugin "
               "zugeordnet (oeffentliche API, nicht intern)");
        /* 721152 * 6400 = 4 615 372 800 > UINT32_MAX */
        ZUSAGE(rc != UFT_OK,
               "die Spur wird ABGESAGT statt mit umgelaufener Zeit "
               "geliefert (vorher: rc=OK und ein kurzes Intervall)");
        ZUSAGE(t.flux_count == 0,
               "und es steht kein Flussstrom in der Spur");

        uft_track_cleanup(&t);
        remove(pfad);
    }

    /* ── 2) Gegenrichtung: resolution = 0 MUSS gelesen werden ──────────
     *
     * Ohne diesen Abschnitt waere die Korrektur gruen, wenn sie ALLES
     * absagt — aus einem falschen Wert waere eine unbrauchbare Funktion
     * geworden (Klasse MF-444). Und die Zahl wird NACHGERECHNET, nicht
     * geglaubt. */
    printf("\n2) resolution=0 (Faktor 25) -> gelesen, Zeit stimmt\n");
    {
        const char *pfad = "mf1133_res0.scp";
        ZUSAGE(datei_bauen(pfad, 0), "Pruefdatei geschrieben");

        uft_track_t t;
        const char *wer = NULL;
        const uft_error_t rc = spur_lesen(pfad, &t, &wer);
        printf("   Plugin=%s rc=%d flux_count=%zu\n",
               wer ? wer : "(keins)", (int)rc, (size_t)t.flux_count);

        ZUSAGE(rc == UFT_OK, "die Spur wird gelesen");
        ZUSAGE(t.flux_count == 1,
               "genau EIN Intervall — die 11 Nullworte sind ein Marker, "
               "kein Ereignis");

        if (t.flux_count == 1 && t.flux) {
            const uint32_t erwartet = TICKS * 25u;   /* 18 028 800 */
            printf("   gemessen=%u erwartet=%u\n",
                   (unsigned)t.flux[0], (unsigned)erwartet);
            ZUSAGE(t.flux[0] == erwartet,
                   "und das Intervall stimmt auf die Nanosekunde — "
                   "nachgerechnet, nicht geglaubt");
        } else {
            rot++;
            printf("   [ROT] kein Flussstrom zum Nachrechnen\n");
        }

        /* Die dritte Stelle: index_time_ns und die Division darunter. */
        printf("   index_time_ns=%u rpm=%.2f\n",
               (unsigned)t.metrics.index_time_ns, t.metrics.rpm);
        ZUSAGE(t.metrics.index_time_ns == TICKS * 25u,
               "index_time_ns kommt aus derselben 64-Bit-Rechnung");
        ZUSAGE(t.metrics.rpm > 0.0,
               "und die Drehzahl ist keine Division durch Null");

        uft_track_cleanup(&t);
        remove(pfad);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
