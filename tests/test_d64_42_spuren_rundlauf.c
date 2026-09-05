/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * test_d64_42_spuren_rundlauf.c — P3-184 / MF-920.
 *
 * ══════════════════════════════════════════════════════════════════════
 *  DER BEFUND, UND WARUM DIESER BEWEIS NICHTS KOSTET
 * ══════════════════════════════════════════════════════════════════════
 *
 * MF-871 hat dem D64-LESER vier Ausdehnungen gegeben (35, 40, 41, 42),
 * MF-908 dem SCHREIBER dieselben. Was seither offen blieb, steht in
 * P3-184: „**beide Tueren finden Spur 41/42 weiterhin nicht.**"
 *
 * Gemessen sind es DREI Tueren, nicht zwei:
 *
 *   1  `g64_to_d64()`                    src/formats/c64/uft_d64_g64.c
 *      sondiert `for (t = 36*2; t <= 40*2; t += 2)` -> Deckel 40
 *   2  `uft_cbm_d64_decode_via_plugin()` dieselbe Datei
 *      sondiert `for (probe = 36; probe <= 40; probe++)` -> Deckel 40
 *   3  `g64_export_d64()`                src/formats/g64/uft_g64_parser_v3.c
 *      fest verdrahtet auf 35 Spuren und 683 Bloecke
 *
 * Tuer 3 hat **null Aufrufer** (`uft_g64_v3_export_d64` wird nur in
 * seiner eigenen Datei genannt) und bleibt deshalb unangetastet —
 * dieselbe Begruendung wie bei den verwaisten Geometrietabellen in
 * P3-182. Sie ist als P3-193 gefuehrt, damit sie vor einer
 * Verdrahtung der v3-Bruecke nicht uebersehen wird.
 *
 * Tuer 1 und 2 sind erreichbar. Beide kappen still: eine 42-Spur-G64
 * wird zu einer 40-Spur-D64, und niemand erfaehrt, dass zwei Spuren
 * fehlen.
 *
 * ── Warum dieser Beweis KEINE Beschaffung braucht ───────────────────
 *
 * P3-184 hat es selbst vorgerechnet: seit MF-908 kann UFT eine
 * 42-Spur-D64 **selbst erzeugen**, und `D64 -> G64` steht in der
 * Wandlungsmatrix als **verlustfrei gemessen** (MF-533). Also muss
 * `D64(42) -> G64 -> D64` 42 Spuren zurueckgeben. Kein fremdes Abbild
 * noetig.
 *
 * Was dieser Test damit ausdruecklich NICHT belegt: dass UFTs
 * 42-Spur-Geometrie der einer echten 1541 mit 42 Spuren entspricht.
 * Das ist Selbstkonsistenz — genau die Grenze, die MF-916/P3-189 fuer
 * den Zonen-SSOT gezogen hat, und sie gilt hier woertlich weiter. Der
 * Test misst, ob der Baum sich selbst treu bleibt; nicht, ob er die
 * Welt richtig abbildet.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/formats/c64/uft_d64_g64.h"
#include "uft/uft_format_plugin.h"

extern const uft_format_plugin_t uft_format_plugin_d64;
extern const uft_format_plugin_t uft_format_plugin_g64;

static int g_ok = 0;
static void ok(const char *n) { g_ok++; printf("  OK  %s\n", n); }

/* Jede Spur bekommt in Sektor 0 ein Muster, das SIE benennt — sonst
 * beweist ein Vergleich nur, dass irgendwo Nullen stehen. */
static void muster(uint8_t *b, int track)
{
    for (int i = 0; i < 256; i++)
        b[i] = (uint8_t)((track * 7 + i * 3) & 0xFF);
    b[0] = (uint8_t)track;
    b[1] = (uint8_t)~track;
}

static d64_image_t *baue_42(void)
{
    d64_image_t *img = d64_create(42);
    if (!img) return NULL;
    for (int t = 1; t <= 42; t++) {
        uint8_t b[256];
        muster(b, t);
        if (d64_set_sector(img, t, 0, b, D64_ERR_OK) != 0) {
            d64_free(img);
            return NULL;
        }
    }
    return img;
}

static void pruefe_spuren(const d64_image_t *img, int bis, const char *wer)
{
    for (int t = 1; t <= bis; t++) {
        uint8_t erwartet[256], gelesen[256];
        muster(erwartet, t);
        if (d64_get_sector(img, t, 0, gelesen, NULL) != 0) {
            printf("  FEHLER (%s): Spur %d nicht lesbar\n", wer, t);
            assert(0);
        }
        if (memcmp(erwartet, gelesen, 256) != 0) {
            printf("  FEHLER (%s): Spur %d weicht ab (Byte 0: %02X statt %02X)\n",
                   wer, t, gelesen[0], erwartet[0]);
            assert(0);
        }
    }
}

/* ═══════════ 1. Die Vorbedingung: 42 Spuren lassen sich bauen ══════ */

static void t_vorbedingung(void)
{
    d64_image_t *img = baue_42();
    assert(img != NULL);
    assert(img->num_tracks == 42);
    /* Ohne diese Gegenprobe waere der Rest gruen, wenn d64_create()
     * still auf 35 zurueckfaellt (MF-447-Klasse: ein Test, der nichts
     * misst, weil seine Grundlage leer ist). */
    pruefe_spuren(img, 42, "Aufbau");
    d64_free(img);
    ok("42-Spur-D64 laesst sich bauen und tragen (Vorbedingung)");
}

/* ═══════════ 2. Tuer 1: g64_to_d64() ══════════════════════════════ */

static void t_tuer_g64_to_d64(void)
{
    d64_image_t *orig = baue_42();
    assert(orig != NULL);

    convert_options_t opts;
    convert_get_defaults(&opts);
    opts.extended_tracks = true;

    /* Der Encoder bekommt hier ein `result`, weil genau DAS die
     * Neuerung ist: er muss den Verlust melden. */
    convert_result_t enc;
    memset(&enc, 0, sizeof enc);
    g64_image_t *g64 = NULL;
    assert(d64_to_g64(orig, &g64, &opts, &enc) == 0);
    assert(g64 != NULL);

    /* ── DER KERN VON MF-920 ─────────────────────────────────────────
     * Vor dem Eingriff meldete der Encoder `success = true` und
     * „Converted 41 tracks" — eine Spur war weg, und niemand erfuhr
     * es. Ein Aufrufer, der nur `success` liest, haette einen
     * Datenverlust nie bemerkt. */
    if (enc.success) {
        printf("  FEHLER: der Encoder meldet Erfolg, obwohl Spur 42 "
               "abgewiesen wurde — '%s'\n", enc.description);
        assert(0);
    }
    assert(enc.errors_found == 1);
    assert(enc.tracks_converted == 41);
    if (!strstr(enc.description, "abgewiesen")) {
        printf("  FEHLER: die Beschreibung nennt den Verlust nicht: '%s'\n",
               enc.description);
        assert(0);
    }

    d64_image_t *zurueck = NULL;
    convert_result_t res;
    memset(&res, 0, sizeof res);
    assert(g64_to_d64(g64, &zurueck, &opts, &res) == 0);
    assert(zurueck != NULL);

    /* ── WAS HIER ZUGESICHERT WIRD, UND WARUM NICHT 42 ──────────────
     *
     * Vor MF-920 kamen **40** Spuren zurueck: beide Sonden hoerten bei
     * 40 auf. Jetzt kommen **41**.
     *
     * 42 kommen NICHT zurueck, und das ist keine Nachlaessigkeit,
     * sondern eine gemessene Grenze des Speichermodells:
     * `d64_to_g64()` rechnet `halftrack = track * 2`, fuer Spur 42 also
     * 84 — und `g64_set_track()` weist `halftrack >= G64_MAX_TRACKS`
     * (84) ab. Es gibt fuer Spur 42 schlicht keinen Platz. Der Baum
     * traegt dazu ZWEI verschiedene Halbspur-Abbildungen
     * (`track * 2` hier, `track * 2 - 1` im v3-Parser); welche der
     * Datei entspricht, ist ungemessen. Als **P3-194** gefuehrt.
     *
     * Was MF-920 daran geaendert hat: der Verlust ist nicht mehr
     * STILL. Genau das wird unten geprueft — und es ist die
     * Zusicherung, die haelt, egal wie P3-194 ausgeht. */
    if (zurueck->num_tracks != 41) {
        printf("  FEHLER: g64_to_d64() gibt %d Spuren zurueck, erwartet "
               "sind 41 (Spur 42 hat im Speichermodell keinen Platz, "
               "siehe P3-194)\n", zurueck->num_tracks);
        assert(0);
    }
    pruefe_spuren(zurueck, 41, "g64_to_d64");

    d64_free(zurueck);
    g64_free(g64);
    d64_free(orig);
    ok("D64(42) -> G64 -> D64 -> 41 Spuren (war 40), Verlust BENANNT");
}

/* ═══════════ 3. Tuer 2: der Plugin-Weg ════════════════════════════ */

static void t_tuer_via_plugin(void)
{
    const char *d64_pfad = "test_d64_42_quelle.d64";
    const char *g64_pfad = "test_d64_42_zwischen.g64";

    d64_image_t *orig = baue_42();
    assert(orig != NULL);
    assert(d64_save(d64_pfad, orig, false) == 0);

    /* D64-Datei -> Plugin -> G64 (derselbe Weg, den der Wandler geht) */
    uft_disk_t dq;
    memset(&dq, 0, sizeof dq);
    dq.read_only = true;
    assert(uft_format_plugin_d64.open(&dq, d64_pfad, true) == UFT_OK);

    convert_options_t opts;
    convert_get_defaults(&opts);
    opts.extended_tracks = true;

    g64_image_t *g64 = NULL;
    convert_result_t r1;
    memset(&r1, 0, sizeof r1);
    int rc = uft_cbm_g64_encode_via_plugin(&uft_format_plugin_d64, &dq,
                                           &opts, &g64, &r1);
    uft_format_plugin_d64.close(&dq);
    assert(rc == 0 && g64 != NULL);
    /* Auch dieser Encoder muss den Verlust melden — er ist eine zweite
     * Kopie derselben Schleife (MF-433), und beide sind zu berichtigen,
     * sonst haengt die Ehrlichkeit am gewaehlten Weg. */
    if (r1.success) {
        printf("  FEHLER: der Plugin-Encoder meldet Erfolg trotz "
               "abgewiesener Spur — '%s'\n", r1.description);
        assert(0);
    }
    assert(r1.errors_found == 1);
    assert(g64_save(g64_pfad, g64) == 0);
    g64_free(g64);

    /* G64-Datei -> Plugin -> D64 */
    uft_disk_t dz;
    memset(&dz, 0, sizeof dz);
    dz.read_only = true;
    assert(uft_format_plugin_g64.open(&dz, g64_pfad, true) == UFT_OK);

    /* ── EIN DRITTER BEFUND, HIER GEMESSEN (P3-195) ─────────────────
     *
     * Diese Zahl ist der Grund, warum der DATEI-Weg schlechter
     * abschneidet als der Speicher-Weg oben. Gemessen: eine G64 mit 41
     * Spuren wird beim Wiederoeffnen als **21 Zylinder** gemeldet.
     *
     * Ursache, Glied fuer Glied:
     *
     *   Schreiber  `g64_create(n, include_halftracks=false)` setzt
     *              `img->num_tracks = n` — also 42.
     *   Leser      `uft_g64.c` rechnet `(num_tracks + 1) / 2` = 21,
     *              deutet das Feld also als HALBSPUR-Zahl.
     *   Wahrheit   `tests/corpus_free/vice_c1541_35trk.g64`, von VICE
     *              erzeugt, hat an Byte 9 den Wert **0x54 = 84**.
     *              Der Leser hat recht, der SCHREIBER ist falsch.
     *
     * Folge fuer diese Pruefung: `uft_cbm_d64_decode_via_plugin()`
     * bricht seine Sonde bei `probe > geometry.cylinders` ab — also
     * schon bei Spur 22. Die Spuren 36..41 werden nie gesucht, und
     * zwar UNABHAENGIG von der Deckelung, die MF-920 behoben hat.
     *
     * Der Wert unten wird deshalb FESTGENAGELT, nicht erwartet: wird
     * er besser, faellt diese Pruefung mit einer Anleitung statt
     * still zu driften. */
    printf("       (G64-Datei geoeffnet: %d Zylinder, %d Koepfe)\n",
           dz.geometry.cylinders, dz.geometry.heads);
    if (dz.geometry.cylinders != 21) {
        printf("  HINWEIS: die G64-Datei meldet jetzt %d Zylinder statt 21 "
               "— dann ist P3-195 angefasst worden. Gut; diese Pruefung "
               "ist nachzuziehen (erwartet dann 41).\n",
               dz.geometry.cylinders);
        assert(0);
    }
    d64_image_t *zurueck = NULL;
    convert_result_t r2;
    memset(&r2, 0, sizeof r2);
    rc = uft_cbm_d64_decode_via_plugin(&uft_format_plugin_g64, &dz,
                                       &opts, &zurueck, &r2);
    uft_format_plugin_g64.close(&dz);
    assert(rc == 0 && zurueck != NULL);

    /* 35, nicht 41 — und der Grund steht oben (P3-195). Die
     * Deckelung, die MF-920 behoben hat, kommt auf diesem Weg gar
     * nicht zum Tragen, weil die Sonde vorher an der falschen
     * Geometrie abbricht. */
    if (zurueck->num_tracks != 35) {
        printf("  HINWEIS: der Datei-Weg gibt jetzt %d Spuren zurueck "
               "statt 35 — dann ist P3-195 behoben. Gut; auf 41 "
               "nachziehen.\n", zurueck->num_tracks);
        d64_free(zurueck); d64_free(orig);
        remove(d64_pfad); remove(g64_pfad);
        assert(0);
    }

    /* ── UND JETZT DER TEURE TEIL DES BEFUNDS ────────────────────────
     *
     * Es sind nicht nur die Spurzahlen falsch. Der Serialisierer
     * `g64_save_buffer()` schreibt
     *
     *     for (i = 0; i < image->num_tracks && i < 82; i++)
     *         halftrack = i + 2;
     *
     * Dateieintrag `i` traegt also Spur `i/2 + 1` — das ist die
     * richtige G64-Anordnung. Aber die SCHLEIFENGRENZE ist
     * `image->num_tracks`, und das ist bei `include_halftracks=false`
     * die VOLLSPURZAHL (42). Geschrieben werden damit die Eintraege
     * 0..41, also die Spuren **1 bis 21**. Alles darueber faellt
     * lautlos weg.
     *
     * Gemessen: der Rundlauf gibt zwar 35 Spuren zurueck, aber ab
     * Spur 22 stehen NULLEN. UFT schreibt eine G64, die es selbst
     * nicht mehr vollstaendig lesen kann.
     *
     * Der Vollstaendigkeit halber, ebenfalls gemessen und Teil von
     * P3-195: der Schreiber legt je Eintrag EIN Byte Speed-Zone ab
     * (`buf[G64_SPEED_OFFSET + i]`), der Leser liest **vier**
     * (`fread(offset_buf, 4, 1, f)`).
     *
     * Diese Pruefung nagelt beides fest. Wird sie rot, ist P3-195
     * angefasst worden — dann gehoert sie nachgezogen, nicht
     * abgeschwaecht. */
    pruefe_spuren(zurueck, 21, "via_plugin bis Spur 21");
    {
        uint8_t leer[256];
        int erste_leere = 0;
        for (int t = 22; t <= 35 && !erste_leere; t++) {
            if (d64_get_sector(zurueck, t, 0, leer, NULL) == 0) {
                bool alles_null = true;
                for (int i = 0; i < 256; i++) if (leer[i]) alles_null = false;
                if (alles_null) erste_leere = t;
            }
        }
        if (erste_leere != 22) {
            printf("  HINWEIS: die erste leere Spur ist %d statt 22 — "
                   "dann hat sich der G64-Schreiber geaendert (P3-195).\n",
                   erste_leere);
            d64_free(zurueck); d64_free(orig);
            remove(d64_pfad); remove(g64_pfad);
            assert(0);
        }
    }

    d64_free(zurueck);
    d64_free(orig);
    remove(d64_pfad);
    remove(g64_pfad);
    ok("Datei-Weg: Inhalt bis Spur 21, ab 22 leer — festgenagelt (P3-195)");
}

/* ═══════ 3b. Tuer 2 DIREKT — ohne den Datei-Schreiber ════════════
 *
 * WARUM DIESE ATTRAPPE NOETIG IST, und das ist kein Komfort:
 *
 * `uft_cbm_d64_decode_via_plugin()` ist die zweite Tuer, an der MF-920
 * die Deckelung 40 aufgehoben hat. Ueber den DATEI-Weg oben laesst sich
 * das nicht messen — dort bricht die Sonde schon bei Spur 22 ab, weil
 * die geschriebene G64 21 Zylinder meldet (P3-195). Eine Zusicherung,
 * die den geaenderten Zweig nie betritt, ist keine (P3-178).
 *
 * Die Attrappe reicht deshalb die Spuren aus dem SPEICHER-G64 heraus —
 * demselben Objekt, das `d64_to_g64()` erzeugt hat. Sie faelscht
 * nichts: sie ersetzt nur den Weg ueber die Platte, den P3-195
 * unbrauchbar macht.
 */
static const g64_image_t *g_attrappe_quelle = NULL;

static uft_error_t attrappe_read_track(uft_disk_t *disk, int cyl, int head,
                                       uft_track_t *track)
{
    (void)disk;
    if (!g_attrappe_quelle || head != 0 || cyl < 0) return UFT_ERROR_INVALID_PARAM;
    uft_track_init(track, cyl, head);

    int halftrack = (cyl + 1) * 2;          /* dieselbe Abbildung wie der Encoder */
    if (halftrack >= 84) return UFT_ERR_MISSING_SECTOR;
    const uint8_t *d = NULL; size_t len = 0;
    if (g64_get_track(g_attrappe_quelle, halftrack, &d, &len, NULL) != 0 ||
        !d || len == 0) {
        return UFT_ERR_MISSING_SECTOR;
    }
    uint8_t *kopie = malloc(len);
    if (!kopie) return UFT_ERROR_NO_MEMORY;
    memcpy(kopie, d, len);
    track->raw_data     = kopie;
    track->raw_size     = len;
    track->raw_len      = len;
    track->raw_capacity = len;
    track->owns_data    = true;
    return UFT_OK;
}

static const uft_format_plugin_t attrappe_plugin = {
    .name = "ATTRAPPE", .description = "Spuren aus dem Speicher-G64",
    .read_track = attrappe_read_track,
};

static void t_tuer_via_plugin_direkt(void)
{
    d64_image_t *orig = baue_42();
    assert(orig != NULL);

    convert_options_t opts;
    convert_get_defaults(&opts);
    opts.extended_tracks = true;

    g64_image_t *g64 = NULL;
    assert(d64_to_g64(orig, &g64, &opts, NULL) == 0);
    g_attrappe_quelle = g64;

    uft_disk_t d;
    memset(&d, 0, sizeof d);
    d.geometry.cylinders = 42;     /* was der Datei-Leser nicht hinbekommt */
    d.geometry.heads = 1;

    d64_image_t *zurueck = NULL;
    convert_result_t r;
    memset(&r, 0, sizeof r);
    int rc = uft_cbm_d64_decode_via_plugin(&attrappe_plugin, &d, &opts,
                                           &zurueck, &r);
    assert(rc == 0 && zurueck != NULL);

    /* Vor MF-920: 40 (die Sonde lief nur bis 36..40 und brach beim
     * ersten Treffer ab). Jetzt: 41 — die hoechste Spur mit Inhalt,
     * aufgerundet auf die naechste gueltige Ausdehnung. */
    if (zurueck->num_tracks != 41) {
        printf("  FEHLER: die zweite Tuer gibt %d Spuren zurueck, "
               "erwartet sind 41\n", zurueck->num_tracks);
        assert(0);
    }
    pruefe_spuren(zurueck, 41, "via_plugin direkt");

    g_attrappe_quelle = NULL;
    d64_free(zurueck);
    g64_free(g64);
    d64_free(orig);
    ok("zweite Tuer direkt gemessen -> 41 Spuren (war 40)");
}

/* ═══════════ 4. Der Waechter: 35 bleibt 35 ════════════════════════ */

static void t_35_bleibt_35(void)
{
    /* Ohne diese Pruefung waere „nimm immer 42" ein gruener Fix — und
     * genau das waere die Fabrikation aus MF-436: 85 Bloecke Fuellung,
     * als geborgene Daten ausgegeben. */
    d64_image_t *orig = d64_create(35);
    assert(orig != NULL);
    for (int t = 1; t <= 35; t++) {
        uint8_t b[256];
        muster(b, t);
        assert(d64_set_sector(orig, t, 0, b, D64_ERR_OK) == 0);
    }

    convert_options_t opts;
    convert_get_defaults(&opts);
    opts.extended_tracks = false;

    convert_result_t enc;
    memset(&enc, 0, sizeof enc);
    g64_image_t *g64 = NULL;
    assert(d64_to_g64(orig, &g64, &opts, &enc) == 0);
    /* Der Gegen-Waechter zur Meldung oben: bei einer 35-Spur-Diskette
     * darf NICHTS abgewiesen werden. Ohne ihn waere „melde immer
     * Verlust" ein gruener Fix. */
    assert(enc.success);
    assert(enc.errors_found == 0);

    d64_image_t *zurueck = NULL;
    assert(g64_to_d64(g64, &zurueck, &opts, NULL) == 0);
    if (zurueck->num_tracks != 35) {
        printf("  FEHLER: eine 35-Spur-Diskette kommt als %d Spuren "
               "zurueck — das ist erfundene Flaeche\n", zurueck->num_tracks);
        assert(0);
    }

    d64_free(zurueck);
    g64_free(g64);
    d64_free(orig);
    ok("35 bleibt 35 — keine erfundene Flaeche");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("test_d64_42_spuren_rundlauf — P3-184 (MF-920)\n");
    t_vorbedingung();
    t_35_bleibt_35();
    t_tuer_g64_to_d64();
    t_tuer_via_plugin();
    t_tuer_via_plugin_direkt();
    printf("%d/%d Pruefungen gruen\n", g_ok, g_ok);
    return 0;
}
