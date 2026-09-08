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
 * Gemessen waren es DREI Tueren, nicht zwei:
 *
 *   1  `g64_to_d64()`                    src/formats/c64/uft_d64_g64.c
 *      sondierte `for (t = 36*2; t <= 40*2; t += 2)` -> Deckel 40
 *   2  `uft_cbm_d64_decode_via_plugin()` dieselbe Datei
 *      sondierte `for (probe = 36; probe <= 40; probe++)` -> Deckel 40
 *   3  `g64_export_d64()`                src/formats/g64/uft_g64_parser_v3.c
 *      fest verdrahtet auf 35 Spuren und 683 Bloecke
 *
 * Tuer 3 hat **null Aufrufer** (`uft_g64_v3_export_d64` wird nur in
 * seiner eigenen Datei genannt) und bleibt deshalb unangetastet —
 * dieselbe Begruendung wie bei den verwaisten Geometrietabellen in
 * P3-182. Sie ist als P3-193 gefuehrt, damit sie vor einer
 * Verdrahtung der v3-Bruecke nicht uebersehen wird.
 *
 * Tuer 1 und 2 sind erreichbar. Beide kappten still bei 40; MF-920 hat
 * die Deckelung gehoben und den Verlust melden lassen, MF-928 die
 * Halbspur-Abbildung auf die der Datei gezogen. Seither laeuft der
 * Rundlauf D64(42) -> G64 -> Datei -> D64 vollstaendig durch, und
 * dieser Test misst genau das.
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

    /* ── MF-920, dann MF-928 ─────────────────────────────────────────
     * MF-920 fand hier: der Encoder meldete `success = true` und
     * „Converted 41 tracks" — eine Spur war weg, und niemand erfuhr
     * es. Die Meldung wurde nachgeruestet.
     *
     * MF-928: JETZT WIRD NICHTS MEHR ABGEWIESEN.
     *
     * MF-920 hat hier zugesichert, dass der Encoder den Verlust der
     * Spur 42 MELDET. Das war richtig, solange es den Verlust gab.
     * Seit MF-928 gibt es ihn nicht mehr: die Halbspur-Abbildung ist
     * die der Datei (2*(Spur-1)), Spur 42 liegt auf Platz 82 und
     * passt. Gemessen: "Converted 42 tracks, 802 sectors".
     *
     * Die Zusicherung dreht sich um — der Waechter bleibt scharf:
     * meldet der Encoder je wieder eine Abweisung, faellt sie. */
    if (!enc.success) {
        printf("  FEHLER: der Encoder meldet einen Verlust — '%s'\n",
               enc.description);
        assert(0);
    }
    assert(enc.errors_found == 0);
    if (enc.tracks_converted != 42) {
        printf("  FEHLER: %d Spuren kodiert, erwartet 42\n",
               enc.tracks_converted);
        assert(0);
    }

    d64_image_t *zurueck = NULL;
    convert_result_t res;
    memset(&res, 0, sizeof res);
    assert(g64_to_d64(g64, &zurueck, &opts, &res) == 0);
    assert(zurueck != NULL);

    /* ── DIE ZAHL, DREIMAL BEWEGT ────────────────────────────────────
     *
     *   vor MF-920   40 — beide Sonden hoerten bei 40 auf
     *   nach MF-920  41 — Sonde bis 42, aber Spur 42 hatte im
     *                     Speichermodell keinen Platz (P3-194)
     *   seit MF-928  42 — die Halbspur-Abbildung ist die der DATEI,
     *                     2*(Spur-1); Spur 42 liegt auf Platz 82
     *
     * Womit auch die Frage aus P3-194 beantwortet ist, und zwar nicht
     * durch Auslegung: `tests/corpus_free/vice_c1541_35trk.g64` sagt
     * es. Eintrag 0 traegt Spur 1.0, Eintrag 1 ist der leere
     * Halbspur-Slot. */
    if (zurueck->num_tracks != 42) {
        printf("  FEHLER: g64_to_d64() gibt %d Spuren zurueck, "
               "erwartet 42\n", zurueck->num_tracks);
        assert(0);
    }
    pruefe_spuren(zurueck, 42, "g64_to_d64");

    d64_free(zurueck);
    g64_free(g64);
    d64_free(orig);
    ok("D64(42) -> G64 -> D64 -> alle 42 Spuren, Inhalt gleich");
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
    /* MF-928: auch dieser Encoder weist nichts mehr ab. Der Waechter
     * bleibt scharf, nur andersherum. */
    if (!r1.success) {
        printf("  FEHLER: der Plugin-Encoder meldet einen Verlust — '%s'\n",
               r1.description);
        assert(0);
    }
    assert(r1.errors_found == 0);
    assert(g64_save(g64_pfad, g64) == 0);
    g64_free(g64);

    /* G64-Datei -> Plugin -> D64 */
    uft_disk_t dz;
    memset(&dz, 0, sizeof dz);
    dz.read_only = true;
    assert(uft_format_plugin_g64.open(&dz, g64_pfad, true) == UFT_OK);

    /* MF-928: hier stand die Charakterisierung des kaputten
     * Schreibers — 21 Zylinder, Inhalt bis Spur 21, ab 22 leer.
     * Ursache war Kopfbyte 9 (Vollspurzahl statt Eintragszahl) und
     * eine Schreibschleife, die danach lief. Beides behoben. */
    printf("       (G64-Datei geoeffnet: %d Zylinder, %d Koepfe)\n",
           dz.geometry.cylinders, dz.geometry.heads);
    if (dz.geometry.cylinders != 42) {
        printf("  FEHLER: die geschriebene G64 meldet %d Zylinder, "
               "erwartet 42\n", dz.geometry.cylinders);
        assert(0);
    }

    d64_image_t *zurueck = NULL;
    convert_result_t r2;
    memset(&r2, 0, sizeof r2);
    rc = uft_cbm_d64_decode_via_plugin(&uft_format_plugin_g64, &dz,
                                       &opts, &zurueck, &r2);
    uft_format_plugin_g64.close(&dz);
    assert(rc == 0 && zurueck != NULL);

    if (zurueck->num_tracks != 42) {
        printf("  FEHLER: der Datei-Weg gibt %d Spuren zurueck, "
               "erwartet 42\n", zurueck->num_tracks);
        d64_free(zurueck); d64_free(orig);
        remove(d64_pfad); remove(g64_pfad);
        assert(0);
    }
    pruefe_spuren(zurueck, 42, "via_plugin ueber die Datei");

    ok("Datei-Weg: alle 42 Spuren byteidentisch (P3-195 behoben)");
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

    int halftrack = cyl * 2;   /* MF-928: 2*(Spur-1), Spur = cyl+1 */
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
     * ersten Treffer ab). Nach MF-920: 41. Seit MF-928: 42 — die
     * Halbspur-Abbildung ist die der Datei, Spur 42 hat einen Platz. */
    if (zurueck->num_tracks != 42) {
        printf("  FEHLER: die zweite Tuer gibt %d Spuren zurueck, "
               "erwartet sind 42\n", zurueck->num_tracks);
        assert(0);
    }
    pruefe_spuren(zurueck, 42, "via_plugin direkt");

    g_attrappe_quelle = NULL;
    d64_free(zurueck);
    g64_free(g64);
    d64_free(orig);
    ok("zweite Tuer direkt gemessen -> 42 Spuren (war 40)");
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
    printf("%d Pruefungen gruen\n", g_ok);
    return 0;
}
