/**
 * @file test_scp_layout.c
 * @brief Eine Teilaufnahme las sich als leere Diskette (MF-481).
 *
 * Punkt 2.3 der a8rawconv-Gap-Analyse fragt nach einem Bedienelement fuer die
 * SCP-Ablage (`scp-ss40` / `ds40` / `ss80` / `ds80`). Beim Nachmessen kam
 * zuerst etwas anderes heraus: das SCP-Plugin verlor **jede** Datei, deren
 * Aufzeichnung nicht bei Spur 0 beginnt — still, ohne Fehler beim Oeffnen.
 *
 * Zwei Haelften desselben Fehlers, beide „relativ statt absolut":
 *
 *   1. `scp_get_geometry()` rechnete die Zylinderzahl aus der ANZAHL
 *      aufgezeichneter Spuren (`end - start + 1`), der Spurzugriff darunter
 *      indiziert aber absolut (`cylinder * 2 + head`).
 *   2. `scp_open()` las die Offset-Tabelle ab Dateianfang und legte sie ab
 *      Index `start_track` ab — die Tabelle ist aber absolut indiziert.
 *
 * Solange `start_track == 0` war, fielen beide zusammen. Nur deshalb ist es
 * nie aufgefallen.
 *
 * Autoritaet: a8rawconv 0.95 (`src/a8rawconv/rawdiskscp.cpp`, GPL-2-or-later,
 * Referenz-Orakel, wird nicht gebaut):
 *
 *   :126  `tracks_to_read = (fileHeader.mEndTrack + 1) / image_track_step`
 *   :144  `fileHeader.mTrackOffsets[image_track]`   — absolut indiziert
 *   :106  „the start/end track range is in terms of tracks per disk and
 *          not tracks per side"
 *
 * Gegengelesen im eigenen Baum: der UFT-SCP-Schreiber legt die Offsets unter
 * `track_offsets[track_num]` ab (uft_scp_writer.c:220) und schreibt die volle
 * 168-Eintrag-Tabelle; der kanonische Parser liest sie absolut
 * (uft_scp_parser.c:247). **Nur dieses Plugin wich ab** — wieder der Fall, in
 * dem ein Fakt mehrfach implementiert ist und die Fassung laeuft, die
 * zufaellig aufgerufen wird.
 */

#include "uft/uft_format_plugin.h"
#include "uft/formats/uft_scp_writer.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_scp;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-52s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                    _fail++; return; } } while (0)

#define FLEN 64

static void get_temp_path(char *path, size_t size, const char *tag)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_scplay_%s_%d.scp", dir, tag, rand() % 100000);
}

/* Jede Spur traegt ihre eigene Nummer im Flux. Ohne das wuerde der Test nur
 * zaehlen, WIE VIELE Spuren kamen, und nicht, ob es die richtigen sind — und
 * eine um 20 verschobene Nummerierung saehe genauso aus wie eine richtige. */
static uint32_t flux_value_for(int scp_track)
{
    return 4000u + (uint32_t)scp_track * 25u;
}

static void fill_flux(uint32_t *f, int scp_track)
{
    for (int i = 0; i < FLEN; i++) f[i] = flux_value_for(scp_track);
}

/**
 * Eine SCP-Datei ueber einen Zylinderbereich schreiben.
 *
 * @param sides  1 = nur Seite 0, 2 = beide
 */
static int write_range(const char *path, int cyl_from, int cyl_to, int sides)
{
    scp_writer_t *w = scp_writer_create(SCP_TYPE_AMIGA, 1);
    if (!w) return -1;

    uint32_t f[FLEN];
    int rc = 0;
    for (int c = cyl_from; c <= cyl_to && rc == 0; c++) {
        for (int s = 0; s < sides && rc == 0; s++) {
            fill_flux(f, c * 2 + s);
            rc = scp_writer_add_track(w, c, s, f, FLEN,
                                      (uint32_t)FLEN * flux_value_for(c * 2 + s),
                                      0);
        }
    }
    if (rc == 0) rc = scp_writer_save(w, path);
    scp_writer_free(w);
    return rc;
}

/**
 * Alle Zylinder der gemeldeten Geometrie durchgehen.
 *
 * @param out_wrong  Spuren, deren Flux zu einer ANDEREN Spurnummer gehoert
 * @return Anzahl Spuren mit Flux
 */
static int read_all(const char *path, int *out_cyls, int *out_heads,
                    int *out_wrong)
{
    uft_disk_t d;
    memset(&d, 0, sizeof(d));
    d.read_only = true;
    if (uft_format_plugin_scp.open(&d, path, true) != UFT_OK) return -1;

    if (out_cyls)  *out_cyls  = d.geometry.cylinders;
    if (out_heads) *out_heads = d.geometry.heads;
    if (out_wrong) *out_wrong = 0;

    int found = 0;
    for (int c = 0; c < d.geometry.cylinders; c++) {
        for (int h = 0; h < d.geometry.heads; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (uft_format_plugin_scp.read_track(&d, c, h, &t) == UFT_OK &&
                t.flux && t.flux_count > 0) {
                found++;
                if (t.flux[0] != flux_value_for(c * 2 + h) && out_wrong)
                    (*out_wrong)++;
            }
            uft_track_release(&t);
        }
    }
    uft_format_plugin_scp.close(&d);
    return found;
}

/* ────────────────────────────────────────────────────────────────────── */

TEST(a_full_double_sided_image_reads_completely)
{
    /* Der Normalfall, gegen den die Sonderfaelle gehalten werden. Er war auch
     * vorher gruen — deshalb sagt er allein nichts. */
    char path[512];
    get_temp_path(path, sizeof(path), "full");
    ASSERT(write_range(path, 0, 9, 2) == 0);

    int cyls = 0, heads = 0, wrong = 0;
    int n = read_all(path, &cyls, &heads, &wrong);
    if (n != 20 || wrong)
        printf("\n        Zylinder=%d Koepfe=%d gelesen=%d falsch=%d\n",
               cyls, heads, n, wrong);
    ASSERT(cyls == 10);
    ASSERT(heads == 2);
    ASSERT(n == 20);
    ASSERT(wrong == 0);

    remove(path);
}

TEST(a_partial_capture_starting_above_zero_is_not_lost)
{
    /* DIE Rotprobe. Eine Aufnahme, die erst bei Zylinder 20 beginnt — normal,
     * wenn nur der aeussere Teil einer Diskette gesichert wurde, und genauso
     * normal fuer eine Nachlese einzelner Spuren. start_track ist dann 40.
     *
     * Vor MF-481: 0 von 20 Spuren, ohne Fehler. */
    char path[512];
    get_temp_path(path, sizeof(path), "partial");
    ASSERT(write_range(path, 20, 29, 2) == 0);

    int cyls = 0, heads = 0, wrong = 0;
    int n = read_all(path, &cyls, &heads, &wrong);
    if (n != 20 || wrong)
        printf("\n        Zylinder=%d Koepfe=%d gelesen=%d falsch=%d\n",
               cyls, heads, n, wrong);
    ASSERT(n == 20);

    /* Und zwar unter ihren EIGENEN Nummern. Eine verschobene Nummerierung —
     * Zylinder 20 als Zylinder 0 ausgeben — waere die bequeme Loesung und
     * forensisch falsch: die Spurnummer steht auf dem Medium. */
    ASSERT(wrong == 0);

    /* Die Zylinderzahl deckt den ganzen adressierbaren Bereich ab, nicht nur
     * das Aufgezeichnete: 30 = (end_track 59 + 1 + 1) / 2. */
    ASSERT(cyls == 30);

    remove(path);
}

TEST(not_recorded_and_unformatted_stay_two_different_answers)
{
    /* Was mit den Zylindern 0..19 passiert, die es in der Datei nicht gibt.
     *
     * Sie melden UFT_ERROR_TRACK_NOT_FOUND — NICHT „unformatiert". Der
     * Unterschied ist forensisch und kein Formalismus: unformatiert ist eine
     * Aussage ueber die DISKETTE, nicht aufgezeichnet eine ueber die AUFNAHME.
     * Wer beides zusammenwirft, behauptet eine Eigenschaft des Mediums, die
     * nie gemessen wurde.
     *
     * Die Gegenprobe steht daneben: eine Spur INNERHALB des Bereichs, deren
     * Offset 0 ist, meldet sehr wohl unformatiert — dort wurde gemessen und
     * nichts gefunden. */
    char path[512];
    get_temp_path(path, sizeof(path), "below");

    /* Zylinder 20..29 beidseitig, aber 25 ausgelassen — eine Luecke MITTEN
     * im aufgezeichneten Bereich, wie sie eine Nachlese hinterlaesst, bei der
     * eine Spur nicht mehr lesbar war. */
    scp_writer_t *w = scp_writer_create(SCP_TYPE_AMIGA, 1);
    ASSERT(w != NULL);
    uint32_t f[FLEN];
    for (int c = 20; c <= 29; c++) {
        if (c == 25) continue;
        for (int s = 0; s < 2; s++) {
            fill_flux(f, c * 2 + s);
            ASSERT(scp_writer_add_track(w, c, s, f, FLEN,
                       (uint32_t)FLEN * flux_value_for(c * 2 + s), 0) == 0);
        }
    }
    ASSERT(scp_writer_save(w, path) == 0);
    scp_writer_free(w);

    uft_disk_t d;
    memset(&d, 0, sizeof(d));
    d.read_only = true;
    ASSERT(uft_format_plugin_scp.open(&d, path, true) == UFT_OK);

    /* Unterhalb der Startspur: nicht aufgezeichnet. */
    uft_track_t t;
    memset(&t, 0, sizeof(t));
    uft_error_t rc = uft_format_plugin_scp.read_track(&d, 5, 0, &t);
    if (rc != UFT_ERROR_TRACK_NOT_FOUND)
        printf("\n        Zylinder 5: rc=%d\n", (int)rc);
    ASSERT(rc == UFT_ERROR_TRACK_NOT_FOUND);
    uft_track_release(&t);

    /* Im Bereich, aber ohne Daten: unformatiert. */
    memset(&t, 0, sizeof(t));
    rc = uft_format_plugin_scp.read_track(&d, 25, 0, &t);
    if (rc != UFT_OK || t.status != UFT_TRACK_UNFORMATTED)
        printf("\n        Zylinder 25 (Luecke im Bereich): rc=%d status=%d\n",
               (int)rc, (int)t.status);
    ASSERT(rc == UFT_OK);
    ASSERT(t.flux_count == 0);
    ASSERT(t.status == UFT_TRACK_UNFORMATTED);
    uft_track_release(&t);

    uft_format_plugin_scp.close(&d);
    remove(path);
}

TEST(a_single_sided_image_keeps_two_head_slots)
{
    /* Nur Seite 0 geschrieben. Die gemeldete Kopfzahl bleibt 2 — das ist
     * keine Nachlaessigkeit, sondern die Regel des Orakels: SCP reserviert
     * Eintraege fuer beide Seiten, auch bei einseitigen Abbildern
     * (rawdiskscp.cpp:111-113), und a8rawconv benutzt das Kopf-Feld `mSides`
     * NICHT fuer die Ablage. Die zweite Seite meldet sich als unformatiert.
     *
     * Ein Bedienelement fuer die erzwungene Deutung (`scp-ss40` …) fehlt
     * weiterhin — siehe KNOWN_ISSUES FLUX-6. */
    char path[512];
    get_temp_path(path, sizeof(path), "ss");
    ASSERT(write_range(path, 0, 9, 1) == 0);

    int cyls = 0, heads = 0, wrong = 0;
    int n = read_all(path, &cyls, &heads, &wrong);
    if (n != 10 || wrong)
        printf("\n        Zylinder=%d Koepfe=%d gelesen=%d falsch=%d\n",
               cyls, heads, n, wrong);
    ASSERT(heads == 2);
    ASSERT(n == 10);            /* die 10 geschriebenen, Seite 1 leer */
    ASSERT(wrong == 0);

    remove(path);
}

int main(void)
{
    printf("=== SCP: Teilaufnahmen gehen nicht mehr verloren (MF-481) ===\n");
    RUN(a_full_double_sided_image_reads_completely);
    RUN(a_partial_capture_starting_above_zero_is_not_lost);
    RUN(not_recorded_and_unformatted_stay_two_different_answers);
    RUN(a_single_sided_image_keeps_two_head_slots);
    printf("\nResults: %d passed, %d failed\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
