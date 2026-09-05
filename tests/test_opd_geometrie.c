/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_opd_geometrie.c
 * @brief OPD liest die Geometrie, die im Bootsektor steht (MF-905)
 *
 * ── Was gemessen wurde ────────────────────────────────────────────────────
 *
 * `uft_opus_probe()` begann mit
 *
 *     if (size != OPUS_DISK_SIZE) return false;
 *
 * und `OPUS_DISK_SIZE` ist in `include/uft/formats/uft_opus.h` fest als
 * 40 x 1 x 18 x 256 = 184320 verdrahtet. `uft_opus_read_mem()` legte die
 * Spuren mit denselben Konstanten an, und `opus_read_track()` wies jede
 * Seite ausser 0 ab (`head != 0`) und indizierte `track_data[cyl]` statt
 * `[cyl * heads + head]` — obwohl der Container ausweislich seines
 * eigenen Kommentars `[track * heads + head]` vorsieht.
 *
 * Folge: jede doppelseitige Opus-Discovery-Diskette wird **still
 * abgelehnt**. Das Plugin ist in `uft_format_registry.c` registriert,
 * also fuer einen Benutzer erreichbar, und hatte **null Tests**.
 *
 * ── Die Referenz liegt im eigenen Baum, und zwar doppelt ──────────────────
 *
 * `src/samdisk/` ist eine vendorte SAMdisk-4.0-ALPHA-Kopie unter **MIT**
 * (`src/samdisk/License.txt`, © 2002-2020 Simon Owen), im Baum
 * ausdruecklich als Referenz-Orakel gefuehrt (`src/samdisk/README.md`).
 * Dort steht der Bytespiegel:
 *
 *     struct OPD_BOOT {            // src/samdisk/opd.h
 *         uint8_t jr_boot[2];      // Z80 JR (0x18) auf den Startcode
 *         uint8_t cyls;
 *         uint8_t sectors;
 *         uint8_t flags;           // b7-6 FDC-Groessencode
 *                                  // b4   Seiten (0 = eine, 1 = zwei)
 *     };
 *
 * und **Leser wie Schreiber** werten ihn gleich aus — `ReadOPD()` und
 * `WriteOPD()` in `src/samdisk/opd.cpp` enthalten beide woertlich:
 *
 *     fmt.cyls    = ob.cyls;
 *     fmt.heads   = (ob.flags & 0x10) ? 2 : 1;
 *     fmt.sectors = ob.sectors;
 *     fmt.size    = ob.flags >> 6;
 *
 * Das ist die Doppelbestaetigung, die dieser Baum verlangt: nicht eine
 * Quelle, die etwas behauptet, sondern zwei Richtungen derselben
 * Umsetzung, die sich decken.
 *
 * **Und es ist ein Beleg-Aufstieg.** Der Kopf von `uft_opus.c` nannte
 * bisher `libdsk drvopus.c` mit dem Zusatz, die Datei liege *nicht* in
 * der geprueften Fassung vor — also eine **unverifizierte** Referenz
 * (MF-651). Die neue liegt im Baum und ist lesbar.
 *
 * ── Der Groessencode ──────────────────────────────────────────────────────
 *
 * `flags >> 6` ist der FDC-Groessencode, nicht die Bytezahl:
 * 0 = 128, 1 = 256, 2 = 512, 3 = 1024, also `128 << code`. Eine
 * gewoehnliche Opus-Diskette traegt Code 1.
 *
 * ── Was der Test NICHT prueft ─────────────────────────────────────────────
 *
 * Das Verhalten an einer ECHTEN Opus-Discovery-Aufnahme — im Korpus
 * liegt keine. Die Abbilder hier sind **synthetisch** und unten Byte fuer
 * Byte aufgebaut; sie behaupten nicht, wie eine OPD in freier Wildbahn
 * aussieht, sondern nur, was der Leser mit einem dokumentierten
 * Kopfwert tun muss. Die BEDEUTUNG der Werte stammt aus der oben
 * benannten Referenz, nicht aus dieser Datei.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_track.h"
#include "uft/formats/uft_opus.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_opus;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-40s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

#define OP_JR 0x18   /* Z80: unbedingter relativer Sprung */

/**
 * @brief Baut eine OPD, deren Bootsektor die Geometrie ANSAGT.
 *
 * @param groessencode FDC-Code: 0=128, 1=256, 2=512, 3=1024
 * @param zwei_seiten  setzt Bit 4 der Flags
 * @param luegen       wenn wahr, wird die Datei absichtlich in einer
 *                     Groesse geschrieben, die NICHT zur Ansage passt
 * @return Zeiger auf malloc'ten Puffer, Groesse in *out_size
 */
static uint8_t *baue_opd(uint8_t cyls, uint8_t sectors, uint8_t groessencode,
                         int zwei_seiten, int luegen, size_t *out_size)
{
    const uint8_t heads = zwei_seiten ? 2 : 1;
    const size_t sektorgroesse = (size_t)128u << groessencode;
    size_t groesse = (size_t)cyls * heads * sectors * sektorgroesse;
    if (luegen) groesse += sektorgroesse;      /* ein Sektor zu viel */

    uint8_t *p = calloc(1, groesse);
    if (!p) return NULL;

    p[0] = OP_JR;                              /* jr_boot[0] */
    p[1] = 0x28;                               /* jr_boot[1], beliebig  */
    p[2] = cyls;
    p[3] = sectors;
    p[4] = (uint8_t)((groessencode << 6) | (zwei_seiten ? 0x10 : 0x00));

    /* Jeden Sektor unterscheidbar fuellen, damit ein Verwechseln von
     * Spur oder Seite auffiele. */
    const size_t sektoren = groesse / sektorgroesse;
    for (size_t s = 1; s < sektoren; s++) {
        memset(p + s * sektorgroesse, (int)(s & 0xFF), sektorgroesse);
    }

    *out_size = groesse;
    return p;
}

static const char *schreibe(const char *name, const uint8_t *p, size_t n)
{
    static char pfad[512];
    snprintf(pfad, sizeof(pfad), "%s", name);
    FILE *f = fopen(pfad, "wb");
    if (!f) return NULL;
    size_t w = fwrite(p, 1, n, f);
    fclose(f);
    return (w == n) ? pfad : NULL;
}

/* ─────────────────────────────────────────────────────────────────────────
 *  1. Zwei Seiten im Bootsektor -> zwei Seiten in der Geometrie.
 *
 *  40 x 2 x 18 x 256 = 368640 Byte. Heute weist `uft_opus_probe()` das
 *  ab, weil es nur 184320 kennt.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(zwei_seiten_werden_gelesen)
{
    size_t n = 0;
    uint8_t *p = baue_opd(40, 18, 1, /*zwei_seiten*/1, /*luegen*/0, &n);
    ASSERT(p != NULL);
    ASSERT(n == 368640);

    const char *pfad = schreibe("uft_opd_ds.opd", p, n);
    free(p);
    ASSERT(pfad != NULL);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_opus.open(&disk, pfad, true) == UFT_OK);
    ASSERT(disk.geometry.cylinders   == 40);
    ASSERT(disk.geometry.heads       == 2);
    ASSERT(disk.geometry.sectors     == 18);
    ASSERT(disk.geometry.sector_size == 256);
    uft_format_plugin_opus.close(&disk);
    remove(pfad);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  2. Seite 1 ist auch LESBAR, nicht nur gezaehlt.
 *
 *  `opus_read_track()` wies `head != 0` ab und indizierte
 *  `track_data[cyl]` statt `[cyl * heads + head]` — der Container sieht
 *  ausweislich seines eigenen Kommentars `[track * heads + head]` vor.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(seite_eins_ist_lesbar)
{
    size_t n = 0;
    uint8_t *p = baue_opd(40, 18, 1, 1, 0, &n);
    ASSERT(p != NULL);
    const char *pfad = schreibe("uft_opd_ds2.opd", p, n);
    free(p);
    ASSERT(pfad != NULL);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_opus.open(&disk, pfad, true) == UFT_OK);

    uft_track_t t0, t1;
    memset(&t0, 0, sizeof(t0));
    memset(&t1, 0, sizeof(t1));
    ASSERT(uft_format_plugin_opus.read_track(&disk, 0, 0, &t0) == UFT_OK);
    ASSERT(uft_format_plugin_opus.read_track(&disk, 0, 1, &t1) == UFT_OK);

    /* MF-905: hier stand nur `== UFT_OK`. Das TRAEGT NICHT — indiziert
     * der Leser `track_data[cyl]` statt `[cyl * heads + head]`, liefert
     * er fuer beide Seiten DIESELBE Spur und meldet zweimal UFT_OK.
     * Die Gegenprobe fiel deshalb nicht. Gemessen wird jetzt der INHALT:
     * das Fixture fuellt jeden Sektor mit seinem laufenden Index, Spur 0
     * Seite 0 traegt also die Sektoren 0..17, Seite 1 die Sektoren
     * 18..35 — die beiden koennen nicht gleich aussehen. */
    ASSERT(t0.sector_count > 0 && t1.sector_count > 0);
    ASSERT(t0.sectors[0].data != NULL && t1.sectors[0].data != NULL);
    ASSERT(t0.sectors[0].data_size == 256 && t1.sectors[0].data_size == 256);
    ASSERT(memcmp(t0.sectors[0].data, t1.sectors[0].data, 256) != 0);

    /* Und zwar genau die erwarteten Bytes: Sektor 18 traegt 18. */
    ASSERT(t1.sectors[0].data[0] == 18);
    ASSERT(t1.sectors[0].id.head == 1);

    uft_track_release(&t0);
    uft_track_release(&t1);

    uft_format_plugin_opus.close(&disk);
    remove(pfad);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  3. WAECHTER: die gewoehnliche einseitige Diskette bleibt lesbar.
 *
 *  40 x 1 x 18 x 256 = 184320 — der Fall, den das Plugin bisher als
 *  EINZIGEN kannte. Er muss der Umstellung standhalten.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(waechter_einseitig_bleibt)
{
    size_t n = 0;
    uint8_t *p = baue_opd(40, 18, 1, 0, 0, &n);
    ASSERT(p != NULL);
    ASSERT(n == 184320);
    const char *pfad = schreibe("uft_opd_ss.opd", p, n);
    free(p);
    ASSERT(pfad != NULL);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_opus.open(&disk, pfad, true) == UFT_OK);
    ASSERT(disk.geometry.cylinders == 40);
    ASSERT(disk.geometry.heads     == 1);
    ASSERT(disk.geometry.sectors   == 18);
    uft_format_plugin_opus.close(&disk);
    remove(pfad);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  4. Ein Erkenner, der nicht "nein" sagen kann, ist keiner (MF-729).
 *
 *  Zwei Abweisungen:
 *    a) der Bootsektor sagt eine Geometrie an, die NICHT zur Dateigroesse
 *       passt — das Oracle verlangt `file.size() == fmt.disk_size()`
 *    b) kein JR-Opcode am Anfang — dasselbe Oracle verlangt ihn, wenn die
 *       Dateiendung nicht buergt; eine Sonde hat keine Endung
 * ───────────────────────────────────────────────────────────────────────── */
TEST(sonde_kann_nein_sagen)
{
    int konfidenz = 0;

    /* a) Ansage passt nicht zur Groesse */
    size_t n = 0;
    uint8_t *p = baue_opd(40, 18, 1, 1, /*luegen*/1, &n);
    ASSERT(p != NULL);
    ASSERT(n == 368640 + 256);
    ASSERT(uft_opus_probe(p, n, &konfidenz) == false);
    free(p);

    /* b) kein JR-Opcode, sonst korrekte Groesse */
    n = 0;
    p = baue_opd(40, 18, 1, 0, 0, &n);
    ASSERT(p != NULL);
    p[0] = 0x00;                       /* Sprung weg */
    ASSERT(uft_opus_probe(p, n, &konfidenz) == false);
    free(p);

    /* c) lauter Nullen in Opus-Groesse: kein Anspruch */
    uint8_t *nullen = calloc(1, 184320);
    ASSERT(nullen != NULL);
    ASSERT(uft_opus_probe(nullen, 184320, &konfidenz) == false);
    free(nullen);
}

/* ─────────────────────────────────────────────────────────────────────────
 *  5. Andere Sektorgroessen folgen dem Groessencode.
 *
 *  `flags >> 6` ist der FDC-Code, nicht die Bytezahl: 128 << code.
 * ───────────────────────────────────────────────────────────────────────── */
TEST(groessencode_wird_ausgewertet)
{
    size_t n = 0;
    /* Code 2 = 512 Byte, einseitig, 40 x 9 -> 184320 (dieselbe Groesse
     * wie die Vorgabe, aber eine ANDERE Geometrie). Wer nur auf die
     * Dateigroesse sieht, kann die beiden nicht unterscheiden. */
    uint8_t *p = baue_opd(40, 9, 2, 0, 0, &n);
    ASSERT(p != NULL);
    ASSERT(n == 184320);
    const char *pfad = schreibe("uft_opd_512.opd", p, n);
    free(p);
    ASSERT(pfad != NULL);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_opus.open(&disk, pfad, true) == UFT_OK);
    ASSERT(disk.geometry.sectors     == 9);
    ASSERT(disk.geometry.sector_size == 512);
    uft_format_plugin_opus.close(&disk);
    remove(pfad);
}

/* ────────────────────────────────────────────────────────────────────────
 *  6. Der Schreibpfad schneidet nicht ab.
 *
 *  `uft_opus_write()` legte den Ausgabepuffer mit OPUS_DISK_SIZE an und
 *  schrieb genau so viele Bytes. Ein doppelseitiges Abbild waere damit
 *  auf 184320 Byte ABGESCHNITTEN worden — richtiger Name, richtige
 *  Endung, halber Inhalt. Dieselbe Klasse wie MF-877.
 * ──────────────────────────────────────────────────────────────────────── */
TEST(schreibpfad_schneidet_nicht_ab)
{
    size_t n = 0;
    uint8_t *p = baue_opd(40, 18, 1, 1, 0, &n);
    ASSERT(p != NULL);
    ASSERT(n == 368640);
    const char *quelle = schreibe("uft_opd_w_in.opd", p, n);
    free(p);
    ASSERT(quelle != NULL);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    disk.read_only = true;
    ASSERT(uft_format_plugin_opus.open(&disk, quelle, true) == UFT_OK);

    const uft_disk_image_t *bild = (const uft_disk_image_t *)disk.plugin_data;
    ASSERT(bild != NULL);
    ASSERT(uft_opus_write(bild, "uft_opd_w_out.opd") == UFT_OK);
    uft_format_plugin_opus.close(&disk);

    FILE *f = fopen("uft_opd_w_out.opd", "rb");
    ASSERT(f != NULL);
    fseek(f, 0, SEEK_END);
    const long geschrieben = ftell(f);
    fclose(f);
    ASSERT(geschrieben == 368640);

    /* Und der Rundlauf traegt den Inhalt: erneut oeffnen, Seite 1 lesen. */
    uft_disk_t d2;
    memset(&d2, 0, sizeof(d2));
    d2.read_only = true;
    ASSERT(uft_format_plugin_opus.open(&d2, "uft_opd_w_out.opd", true) == UFT_OK);
    ASSERT(d2.geometry.heads == 2);
    uft_track_t t1;
    memset(&t1, 0, sizeof(t1));
    ASSERT(uft_format_plugin_opus.read_track(&d2, 0, 1, &t1) == UFT_OK);
    uft_track_release(&t1);
    uft_format_plugin_opus.close(&d2);

    remove("uft_opd_w_in.opd");
    remove("uft_opd_w_out.opd");
}

int main(void)
{
    printf("OPD: die Geometrie steht im Bootsektor (MF-905)\n");
    RUN(zwei_seiten_werden_gelesen);
    RUN(seite_eins_ist_lesbar);
    RUN(waechter_einseitig_bleibt);
    RUN(sonde_kann_nein_sagen);
    RUN(groessencode_wird_ausgewertet);
    RUN(schreibpfad_schneidet_nicht_ab);
    printf("\n  %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail ? 1 : 0;
}
