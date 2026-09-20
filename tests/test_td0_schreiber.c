/**
 * @file test_td0_schreiber.c
 * @brief UFT schreibt gepackte TD0 — und zwei fremde Leser urteilen (MF-1297).
 *
 * AUFRUFER: `tests/CMakeLists.txt` registriert diese Datei als ctest-Ziel.
 * BERUEHRTE API: `uft_td0_schreibe_gepackt()`, `uft_disk_open()`,
 *   `uft_format_plugin_td0.read_track()`.
 * DATENSCHEMA: keines. Schreibt zwei TD0 ins Arbeitsverzeichnis und
 *   vergleicht sie byteweise mit den committeten Korpusdateien.
 * ANWEISUNG (woertlich): "ja mach es fertig Schreiber selbst (G6) und
 *   Korpus >= 3 (G7)."
 *
 * ── Warum es einen eigenen Schreiber gibt ────────────────────────────
 *
 * Fuer Teledisks Advanced Compression gibt es im Baum keinen fremden
 * SCHREIBER — gemessen, nicht angenommen: fluxfox
 * `src/file_parsers/td0.rs:228` gibt `UnsupportedFormat` zurueck, libdsk
 * schreibt TD0 nur unkomprimiert. Damit fehlte dem Format ein Korpus
 * fuer seine eigene Packung (P3-520).
 *
 * Regel E-14: fehlt ein fremder Schreiber, schreibt UFT es selbst und
 * laesst ZWEI fremde Leser urteilen.
 *
 * ── WAS DIESER TEST NICHT BEWEIST ────────────────────────────────────
 *
 * Er beweist NICHT, dass die Dateien gueltig sind. Packer und Entpacker
 * stammen aus derselben Hand; ein gruener Rundlauf durch den EIGENEN
 * Leser waere die Gestalt von MF-1009 (`apridisk`), wo Packer und
 * Entpacker Spiegelbilder derselben Erfindung waren.
 *
 * Der Beleg sind die beiden fremden Leser, und er ist am Objekt
 * gemessen (MF-1297):
 *
 *   libdsk 1.5 `dskid`  "TeleDisk advanced compression",
 *                       720K: 80 x 2 x 9, 1.2M: 80 x 2 x 15
 *   libdsk `dsktrans -otype raw`
 *                       720K: 737 280 Byte, 1439 von 1439 Marken richtig
 *                       1.2M: 1 228 800 Byte, 2399 von 2399 richtig
 *   hxcfe 2.x  `-conv:IMD_IMG`
 *                       720K: 1440 Sektoren, 1439 Marken richtig
 *                       1.2M: 2400 Sektoren, 2399 Marken richtig
 *
 * (Der jeweils fehlende ist Sektor 1 von Spur 0/0 — er traegt den
 * FAT-BPB statt einer Marke. Warum er ihn traegt, steht unten.)
 *
 * Keines der beiden Werkzeuge laeuft in CI. Was dieser Test deshalb
 * leistet, ist das, was ein Test leisten KANN:
 *
 *   1  der Schreiber ist DETERMINISTISCH — neu geschrieben kommt
 *      byteweise die committete Korpusdatei heraus. Damit ist der
 *      Erzeugungsweg nachvollziehbar und das Orakel-Urteil oben
 *      bezieht sich nachweislich auf DIESE Bytes.
 *   2  UFTs eigener Leser holt aus allen DREI gepackten Korpusdateien
 *      jeden Sektor an seiner eigenen Stelle.
 *   3  die Packung ist keine Identitaet — Kennung klein ('td'), und
 *      die Datei ist kleiner als das Sektorabbild.
 *
 * ── Warum die Dateien einen FAT-BPB tragen ───────────────────────────
 *
 * libdsk hat fuer TD0 kein eigenes `tele_getgeom` und nimmt die
 * Geometrie aus dem Bootsektor. Gemessen an fluxfox'
 * `sector_test_360k.td0`, deren Bootsektor komplett null ist: libdsk
 * meldet 40 x 1 x 8 und schreibt 163 840 statt 368 640 Byte — mehr als
 * die halbe Diskette. Ohne BPB koennte das erste der beiden Orakel gar
 * nicht urteilen.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/uft_error.h"
#include "uft/uft_core.h"
#include "uft/formats/uft_td0.h"

/* Siehe test_td0_pruefsummen.c: `uft_track.h` und `uft_format_plugin.h`
 * sind nicht zusammen einbindbar (P3-530). */
extern void uft_track_release(uft_track_t *track);
extern uft_error_t uft_register_all_formats(void);
extern const uft_format_plugin_t uft_format_plugin_td0;

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR fehlt — tests/CMakeLists.txt muss es setzen"
#endif

#define K_720   UFT_CORPUS_DIR "/uft_gepackt_pc720.td0"
#define K_1200  UFT_CORPUS_DIR "/uft_gepackt_pc1200.td0"
#define K_FLUX  UFT_CORPUS_DIR "/fluxfox_sector_test_360k.td0"

static int fehler = 0;
#define PRUEFE(bed, text) do { \
    if (!(bed)) { printf("    [ROT] %s (Zeile %d)\n", (text), __LINE__); fehler++; } \
} while (0)

/* ── Die beiden Abbilder, genau wie der Korpus-Erzeuger sie baut ───── */

static void bpb_setzen(uint8_t *s0, unsigned gesamt, unsigned sek_je_cluster,
                       unsigned wurzel, unsigned sek_je_fat,
                       unsigned spt, unsigned koepfe)
{
    memset(s0, 0, 512);
    s0[0] = 0xEB; s0[1] = 0x3C; s0[2] = 0x90;
    memcpy(s0 + 3, "UFT-K   ", 8);
    s0[11] = 0x00; s0[12] = 0x02;
    s0[13] = (uint8_t)sek_je_cluster;
    s0[14] = 1; s0[15] = 0;
    s0[16] = 2;
    s0[17] = (uint8_t)(wurzel & 0xFFu); s0[18] = (uint8_t)(wurzel >> 8);
    s0[19] = (uint8_t)(gesamt & 0xFFu); s0[20] = (uint8_t)(gesamt >> 8);
    s0[21] = 0xF9;
    s0[22] = (uint8_t)sek_je_fat; s0[23] = 0;
    s0[24] = (uint8_t)spt;    s0[25] = 0;
    s0[26] = (uint8_t)koepfe; s0[27] = 0;
    s0[510] = 0x55; s0[511] = 0xAA;
}

static void marke_setzen(uint8_t *s, unsigned c, unsigned h, unsigned r)
{
    char m[33];
    snprintf(m, sizeof(m), "UFT-K C%02u H%u S%02u ", c, h, r);
    const size_t ml = strlen(m);
    for (size_t i = 0; i + ml <= 512u; i += ml) memcpy(s + i, m, ml);
}

static uint8_t *abbild_bauen(unsigned zyl, unsigned kpf, unsigned spt,
                             unsigned gesamt, unsigned sjc, unsigned wurzel,
                             unsigned sjf)
{
    const size_t n = (size_t)zyl * kpf * spt * 512u;
    uint8_t *d = (uint8_t *)calloc(1, n);
    if (!d) return NULL;
    for (unsigned c = 0; c < zyl; c++)
        for (unsigned h = 0; h < kpf; h++)
            for (unsigned r = 0; r < spt; r++)
                marke_setzen(d + (((size_t)c * kpf + h) * spt + r) * 512u,
                             c, h, r + 1u);
    bpb_setzen(d, gesamt, sjc, wurzel, sjf, spt, kpf);
    return d;
}

static uint8_t *datei_lesen(const char *pfad, size_t *len_aus)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return NULL;
    if (fseek(f, 0, SEEK_END) != 0) { fclose(f); return NULL; }
    long n = ftell(f);
    if (n <= 0) { fclose(f); return NULL; }
    rewind(f);
    uint8_t *b = (uint8_t *)malloc((size_t)n);
    if (!b) { fclose(f); return NULL; }
    size_t g = fread(b, 1, (size_t)n, f);
    fclose(f);
    if (g != (size_t)n) { free(b); return NULL; }
    *len_aus = g;
    return b;
}

/* ── 1./2. Determinismus gegen die committete Datei ────────────────── */
static void gruppe_deterministisch(const char *name, const char *korpus,
                                   unsigned zyl, unsigned kpf, unsigned spt,
                                   unsigned gesamt, unsigned sjc,
                                   unsigned wurzel, unsigned sjf,
                                   uint8_t rate, const char *kommentar)
{
    printf("  [*] %s: neu geschrieben == committete Korpusdatei?\n", name);

    uint8_t *d = abbild_bauen(zyl, kpf, spt, gesamt, sjc, wurzel, sjf);
    PRUEFE(d != NULL, "kein Speicher fuer das Sektorabbild");
    if (!d) return;

    char ziel[512];
    snprintf(ziel, sizeof(ziel), "td0schr_%s.td0", name);

    uft_td0_schreibsatz_t satz;
    memset(&satz, 0, sizeof(satz));
    satz.daten = d;
    satz.zylinder = zyl; satz.koepfe = kpf;
    satz.sektoren_je_spur = spt; satz.sektorgroesse = 512;
    satz.data_rate = rate; satz.drive_type = 2;
    satz.kommentar = kommentar;
    satz.jahr = 126; satz.monat = 8; satz.tag = 20;
    satz.stunde = 12; satz.minute = 0; satz.sekunde = 0;

    int rc = uft_td0_schreibe_gepackt(&satz, ziel);
    free(d);
    PRUEFE(rc == UFT_OK, "uft_td0_schreibe_gepackt() fehlgeschlagen");
    if (rc != UFT_OK) return;

    size_t nl = 0, kl = 0;
    uint8_t *neu = datei_lesen(ziel, &nl);
    uint8_t *alt = datei_lesen(korpus, &kl);
    PRUEFE(neu != NULL, "geschriebene Datei nicht lesbar");
    PRUEFE(alt != NULL, "Korpusdatei nicht lesbar");
    if (neu && alt) {
        printf("      neu %zu Byte, Korpus %zu Byte\n", nl, kl);
        PRUEFE(nl == kl && memcmp(neu, alt, nl) == 0,
               "die neu geschriebene Datei weicht von der committeten ab — "
               "dann ist der Erzeugungsweg nicht nachvollziehbar, und das "
               "Orakel-Urteil bezieht sich auf ANDERE Bytes");
        /* Die Packung ist keine Identitaet. */
        PRUEFE(nl >= 2u && neu[0] == 't' && neu[1] == 'd',
               "die Kennung ist nicht klein geschrieben — dann ist die "
               "Datei gar nicht gepackt");
        PRUEFE(nl < (size_t)zyl * kpf * spt * 512u,
               "die gepackte Datei ist nicht kleiner als das Sektorabbild");
    }
    free(neu); free(alt);
    remove(ziel);
}

/* ── 3. Der eigene Leser holt jeden Sektor an seiner Stelle ────────── */
static void gruppe_lesen_marken(const char *name, const char *pfad,
                                unsigned zyl, unsigned kpf, unsigned spt)
{
    printf("  [*] %s: jeder Sektor an seiner eigenen Marke\n", name);

    uft_disk_t *dk = uft_disk_open(pfad, true);
    PRUEFE(dk != NULL, "Korpusdatei nicht zu oeffnen");
    if (!dk) return;

    size_t richtig = 0, falsch = 0, gelesen = 0;
    for (unsigned c = 0; c < zyl; c++) {
        for (unsigned h = 0; h < kpf; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (uft_format_plugin_td0.read_track(dk, (int)c, (int)h, &t) != UFT_OK)
                continue;
            for (size_t i = 0; i < t.sector_count; i++) {
                const uft_sector_t *s = &t.sectors[i];
                gelesen++;
                if (c == 0u && h == 0u && i == 0u) continue;  /* BPB */
                char m[33];
                snprintf(m, sizeof(m), "UFT-K C%02u H%u S%02u ",
                         c, h, (unsigned)(i + 1u));
                if (s->data && s->data_len >= strlen(m) &&
                    memcmp(s->data, m, strlen(m)) == 0) richtig++;
                else falsch++;
            }
            uft_track_release(&t);
        }
    }
    uft_disk_close(dk);

    printf("      %zu Sektoren gelesen, %zu Marken richtig, %zu falsch "
           "(erwartet %u)\n", gelesen, richtig, falsch, zyl * kpf * spt);
    PRUEFE(gelesen == (size_t)zyl * kpf * spt,
           "nicht alle Sektoren gelesen");
    PRUEFE(falsch == 0u, "eine Marke sitzt an der falschen Stelle");
    PRUEFE(richtig > 0u, "keine einzige Marke geprueft — dann zaehlt "
                         "dieser Durchlauf nichts");
}

/* ── 4. Die fremde gepackte Datei: linear durchnummeriert ──────────── */
static void gruppe_fluxfox(void)
{
    printf("  [*] fluxfox 360K (FREMD, MIT): Block n traegt n & 0xFF\n");

    uft_disk_t *dk = uft_disk_open(K_FLUX, true);
    PRUEFE(dk != NULL, "fluxfox-Korpusdatei nicht zu oeffnen");
    if (!dk) return;

    size_t richtig = 0, falsch = 0, gelesen = 0;
    for (unsigned c = 0; c < 40u; c++) {
        for (unsigned h = 0; h < 2u; h++) {
            uft_track_t t;
            memset(&t, 0, sizeof(t));
            if (uft_format_plugin_td0.read_track(dk, (int)c, (int)h, &t) != UFT_OK)
                continue;
            for (size_t i = 0; i < t.sector_count; i++) {
                const uft_sector_t *s = &t.sectors[i];
                const size_t linear = ((size_t)c * 2u + h) * 9u + i;
                gelesen++;
                if (linear == 0u) continue;             /* Bootsektor, leer */
                const uint8_t soll = (uint8_t)(linear & 0xFFu);
                if (s->data && s->data_len > 0 && s->data[0] == soll) richtig++;
                else falsch++;
            }
            uft_track_release(&t);
        }
    }
    uft_disk_close(dk);

    printf("      %zu Sektoren gelesen, %zu richtig, %zu falsch "
           "(erwartet 720)\n", gelesen, richtig, falsch);
    PRUEFE(gelesen == 720u, "nicht alle 720 Sektoren gelesen");
    PRUEFE(falsch == 0u,
           "ein Block traegt nicht seine eigene laufende Nummer");
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== TD0: UFT schreibt gepackt, zwei fremde Leser urteilen "
           "(MF-1297) ===\n");

    if (uft_register_all_formats() != UFT_OK) {
        printf("  [ROT] uft_register_all_formats() fehlgeschlagen\n");
        return 1;
    }

    gruppe_deterministisch("pc720", K_720, 80, 2, 9, 1440, 2, 112, 3, 0x00,
        "UFT-K gepackte TD0, selbstbenennende Sektoren (MF-1297)");
    gruppe_deterministisch("pc1200", K_1200, 80, 2, 15, 2400, 1, 224, 7, 0x02,
        "UFT-K gepackte TD0 1.2M, selbstbenennende Sektoren (MF-1297)");

    gruppe_lesen_marken("pc720",  K_720,  80, 2, 9);
    gruppe_lesen_marken("pc1200", K_1200, 80, 2, 15);
    gruppe_fluxfox();

    if (fehler) { printf("\n%d Zusage(n) gefallen\n", fehler); return 1; }
    printf("\nalle Zusagen gehalten\n");
    return 0;
}
