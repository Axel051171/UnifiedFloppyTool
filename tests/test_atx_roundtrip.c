/**
 * @file test_atx_roundtrip.c
 * @brief ATX schreiben und wieder lesen: nichts geht verloren (MF-474).
 *
 * Links den echten ATX-Leser und -Schreiber (src/formats/atx/uft_atx.c).
 *
 * Das write-read-Muster hat in diesem Projekt schon zwei echte
 * Datenkorruptions-Fehler gefunden (SCP MF-318 — Spurlaenge in Bytes statt
 * Bitzellen, jede geschriebene Datei unlesbar; IMD MF-320 — jede Schreibung
 * ein stiller No-Op). Deshalb steht es auch hier am Anfang und nicht am Ende.
 *
 * Geprueft wird nicht Byte-Identitaet der Datei, sondern **semantische
 * Identitaet der Spur**: Sektornummern, Inhalte, Statusbits, Weak-Masken und
 * Winkelpositionen muessen den Rundlauf ueberstehen. Byte-Identitaet waere
 * die staerkere Aussage, aber sie haengt an Dingen, die das Format offen
 * laesst (Creator-Kennung, Reihenfolge gleichrangiger Chunks) — sie zu
 * verlangen hiesse, eine Konvention zu pruefen statt den Inhalt.
 *
 * Authority fuer das Layout: src/a8rawconv/diskatx.cpp (write_atx:216-416).
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_format_common.h"  /* uft_format_add_sector_with_id */
#include "uft/formats/atx.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_atx;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

#define SEC   128
#define NSEC  4

static void get_temp_path(char *path, size_t size, const char *tag)
{
    const char *dir = getenv("TMPDIR");
    if (!dir || !dir[0]) dir = getenv("TMP");
    if (!dir || !dir[0]) dir = getenv("TEMP");
    if (!dir || !dir[0]) dir = ".";
    snprintf(path, size, "%s/uft_atxrt_%s_%d.atx", dir, tag, rand() % 100000);
}

static void free_track(uft_track_t *tr)
{
    for (size_t i = 0; i < tr->sector_count; i++) {
        free(tr->sectors[i].data);
        free(tr->sectors[i].weak_mask);
    }
    free(tr->sectors);
    tr->sectors = NULL; tr->sector_count = 0;
}

/**
 * Eine Spur, die alles traegt, was ATX ausdruecken kann: ein gesunder
 * Sektor, einer mit Datenfeld-CRC-Fehler, ein geloeschter, und ein weaker
 * mit Maske. Dazu vier verschiedene Winkelpositionen.
 */
static void build_track(uft_track_t *tr)
{
    memset(tr, 0, sizeof(*tr));
    uft_track_init(tr, 0, 0);
    tr->encoding = UFT_ENCODING_FM;

    static const uint8_t fill[NSEC] = { 0xA0, 0xB1, 0xC2, 0xD3 };
    for (int s = 0; s < NSEC; s++) {
        uint8_t payload[SEC];
        memset(payload, fill[s], sizeof(payload));
        uft_format_add_sector_with_id(tr, (uint8_t)(s + 1), payload, SEC, 0, 0);
    }

    /* Sektor 0: gesund, Position knapp nach dem Index. */
    tr->sectors[0].angular_position = 0.01;
    tr->sectors[0].has_angular_position = true;

    /* Sektor 1: Datenfeld-CRC falsch. */
    uft_sector_set_crc(&tr->sectors[1], false);
    tr->sectors[1].status |= UFT_SECTOR_CRC_ERROR;
    tr->sectors[1].angular_position = 0.25;
    tr->sectors[1].has_angular_position = true;

    /* Sektor 2: Deleted-Mark. */
    tr->sectors[2].deleted = true;
    tr->sectors[2].data_mark = 0xF8;
    tr->sectors[2].status |= UFT_SECTOR_DELETED;
    tr->sectors[2].angular_position = 0.5;
    tr->sectors[2].has_angular_position = true;

    /* Sektor 3: weak ab Byte 64. */
    tr->sectors[3].weak = true;
    tr->sectors[3].status |= UFT_SECTOR_WEAK;
    uft_sector_set_crc(&tr->sectors[3], false);
    tr->sectors[3].weak_mask = calloc(SEC, 1);
    if (tr->sectors[3].weak_mask)
        memset(tr->sectors[3].weak_mask + 64, 0xFF, SEC - 64);
    tr->sectors[3].angular_position = 0.75;
    tr->sectors[3].has_angular_position = true;
}

TEST(everything_the_track_carries_survives_the_round_trip)
{
    char path[512];
    get_temp_path(path, sizeof(path), "full");

    uft_track_t src;
    build_track(&src);
    ASSERT(src.sector_count == NSEC);

    ASSERT(uft_atx_write(path, &src, 1, false, UFT_INTERLEAVE_AUTO) == UFT_OK);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_atx.open(&disk, path, true) == UFT_OK);
    ASSERT(disk.geometry.cylinders == 1);
    ASSERT(disk.geometry.sector_size == SEC);

    uft_track_t back;
    memset(&back, 0, sizeof(back));
    ASSERT(uft_format_plugin_atx.read_track(&disk, 0, 0, &back) == UFT_OK);
    ASSERT(back.sector_count == NSEC);

    for (int s = 0; s < NSEC; s++) {
        const uft_sector_t *a = &src.sectors[s];
        const uft_sector_t *b = &back.sectors[s];

        ASSERT(b->id.sector == a->id.sector);
        ASSERT(b->data != NULL && b->data_len == SEC);
        ASSERT(memcmp(b->data, a->data, SEC) == 0);

        /* Winkelposition: ATX zaehlt in 1/26042 Umdrehung, der Rundlauf
         * quantisiert also. Eine halbe Einheit Toleranz ist der Fehler der
         * Darstellung, nicht des Codes. */
        ASSERT(b->has_angular_position);
        double diff = b->angular_position - a->angular_position;
        if (diff < 0) diff = -diff;
        ASSERT(diff < (1.0 / 26042.0));
    }

    /* Statusbits, einzeln — eine Sammelpruefung wuerde verschweigen,
     * WELCHES Bit verloren ging. */
    ASSERT(back.sectors[0].crc_ok == true);
    ASSERT(back.sectors[0].deleted == false);

    ASSERT(back.sectors[1].crc_ok == false);
    ASSERT((back.sectors[1].status & UFT_SECTOR_CRC_ERROR) != 0);
    ASSERT((back.sectors[1].status & UFT_SECTOR_ID_CRC_ERROR) == 0);

    ASSERT(back.sectors[2].deleted == true);
    ASSERT(back.sectors[2].data_mark == 0xF8);

    ASSERT(back.sectors[3].weak == true);
    ASSERT(back.sectors[3].weak_mask != NULL);
    ASSERT(back.sectors[3].weak_mask[63] == 0);
    ASSERT(back.sectors[3].weak_mask[64] != 0);
    ASSERT(back.sectors[3].weak_mask[SEC - 1] != 0);

    free_track(&back);
    uft_format_plugin_atx.close(&disk);
    free_track(&src);
    remove(path);
}

TEST(duplicate_sector_numbers_survive_as_phantom_sectors)
{
    char path[512];
    get_temp_path(path, sizeof(path), "phantom");

    /* Zwei Sektoren mit derselben Nummer an verschiedenen Winkelpositionen —
     * der Phantomsektor-Kopierschutz. Nur die Position unterscheidet sie;
     * ginge sie verloren, waeren es zwei nicht unterscheidbare Kopien. */
    uft_track_t src;
    memset(&src, 0, sizeof(src));
    uft_track_init(&src, 0, 0);

    uint8_t a[SEC], b[SEC];
    memset(a, 0x11, sizeof(a));
    memset(b, 0x22, sizeof(b));
    uft_format_add_sector_with_id(&src, 7, a, SEC, 0, 0);
    uft_format_add_sector_with_id(&src, 7, b, SEC, 0, 0);
    src.sectors[0].angular_position = 0.10; src.sectors[0].has_angular_position = true;
    src.sectors[1].angular_position = 0.60; src.sectors[1].has_angular_position = true;

    ASSERT(uft_atx_write(path, &src, 1, false, UFT_INTERLEAVE_AUTO) == UFT_OK);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_atx.open(&disk, path, true) == UFT_OK);

    uft_track_t back;
    memset(&back, 0, sizeof(back));
    ASSERT(uft_format_plugin_atx.read_track(&disk, 0, 0, &back) == UFT_OK);

    ASSERT(back.sector_count == 2);
    ASSERT(back.sectors[0].id.sector == 7);
    ASSERT(back.sectors[1].id.sector == 7);
    ASSERT(back.sectors[0].data[0] == 0x11);
    ASSERT(back.sectors[1].data[0] == 0x22);

    /* Der Leser erkennt sie als Phantomsektoren (MF-467) ... */
    ASSERT((back.sectors[0].status & UFT_SECTOR_DUPLICATE) != 0);
    ASSERT((back.sectors[1].status & UFT_SECTOR_DUPLICATE) != 0);
    /* ... und die Positionen, die sie unterscheiden, sind noch da. */
    ASSERT(back.sectors[0].angular_position < back.sectors[1].angular_position);

    free_track(&back);
    uft_format_plugin_atx.close(&disk);
    free_track(&src);
    remove(path);
}

TEST(a_track_without_positions_still_writes_and_reads)
{
    char path[512];
    get_temp_path(path, sizeof(path), "nopos");

    /* Eine Spur aus einer anderen Quelle — kein Format ausser ATX liefert
     * Winkelpositionen. Der Schreiber rechnet dann das Atari-Layout und
     * meldet das (MF-479); das Ergebnis muss trotzdem ein lesbares ATX
     * sein. */
    uft_track_t src;
    memset(&src, 0, sizeof(src));
    uft_track_init(&src, 0, 0);
    for (int s = 0; s < NSEC; s++) {
        uint8_t payload[SEC];
        memset(payload, (uint8_t)(0x40 + s), sizeof(payload));
        uft_format_add_sector_with_id(&src, (uint8_t)(s + 1), payload, SEC, 0, 0);
        ASSERT(src.sectors[s].has_angular_position == false);
    }

    ASSERT(uft_atx_write(path, &src, 1, false, UFT_INTERLEAVE_AUTO) == UFT_OK);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    ASSERT(uft_format_plugin_atx.open(&disk, path, true) == UFT_OK);

    uft_track_t back;
    memset(&back, 0, sizeof(back));
    ASSERT(uft_format_plugin_atx.read_track(&disk, 0, 0, &back) == UFT_OK);
    ASSERT(back.sector_count == NSEC);
    for (int s = 0; s < NSEC; s++) {
        ASSERT(back.sectors[s].id.sector == (uint8_t)(s + 1));
        ASSERT(back.sectors[s].data[0] == (uint8_t)(0x40 + s));
    }

    /* Die Positionen sind gerechnet — der Schreiber hat davor gewarnt — aber
     * sie duerfen nicht alle aufeinanderliegen, sonst waere die Datei fuer
     * einen Leser, der Positionen auswertet, unbrauchbar.
     *
     * MF-479: hier stand `aufsteigend`. Das war keine Eigenschaft des
     * Formats, sondern des alten Ersatzlayouts `s / n`. Verschraenkung heisst
     * gerade, dass aufeinanderfolgende Sektornummern NICHT aufeinanderfolgend
     * auf der Spur liegen — die Forderung haette das richtige Layout
     * ausgeschlossen. Gefordert ist, was der Kommentar immer schon meinte:
     * paarweise verschieden und im gueltigen Bereich. */
    for (int s = 0; s < NSEC; s++) {
        ASSERT(back.sectors[s].angular_position >= 0.0);
        ASSERT(back.sectors[s].angular_position < 1.0);
        for (int t = s + 1; t < NSEC; t++) {
            double d = back.sectors[s].angular_position
                     - back.sectors[t].angular_position;
            if (d < 0) d = -d;
            ASSERT(d >= (1.0 / 26042.0));
        }
    }

    free_track(&back);
    uft_format_plugin_atx.close(&disk);
    free_track(&src);
    remove(path);
}

TEST(write_refuses_what_it_cannot_represent)
{
    uft_track_t tr;
    build_track(&tr);

    ASSERT(uft_atx_write(NULL, &tr, 1, false, UFT_INTERLEAVE_AUTO) == UFT_ERROR_NULL_POINTER);
    ASSERT(uft_atx_write("x.atx", NULL, 1, false, UFT_INTERLEAVE_AUTO) == UFT_ERROR_NULL_POINTER);
    ASSERT(uft_atx_write("x.atx", &tr, 0, false, UFT_INTERLEAVE_AUTO) == UFT_ERROR_NULL_POINTER);
    /* ATX kennt keine 8-bit-Spurnummer jenseits der Tabelle des Lesers. */
    ASSERT(uft_atx_write("x.atx", &tr, 999, false, UFT_INTERLEAVE_AUTO) == UFT_ERROR_OUT_OF_RANGE);

    free_track(&tr);
}

int main(void)
{
    printf("=== ATX write-read-Rundlauf (MF-474) ===\n");
    RUN(everything_the_track_carries_survives_the_round_trip);
    RUN(duplicate_sector_numbers_survive_as_phantom_sectors);
    RUN(a_track_without_positions_still_writes_and_reads);
    RUN(write_refuses_what_it_cannot_represent);
    printf("=== %d passed, %d failed ===\n", _pass, _fail);
    return _fail ? 1 : 0;
}
