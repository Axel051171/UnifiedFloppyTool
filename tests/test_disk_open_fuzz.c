/**
 * @file test_disk_open_fuzz.c
 * @brief Missgebildete Abbilder durch den ECHTEN Oeffnungspfad (MF-512)
 *
 * ── Was dieser Test anders macht als test_format_probe_fuzz.c ────────────
 *
 * Der bestehende Fuzz-Test (MF-392) ist gut, hat aber zwei gemessene
 * Grenzen:
 *
 *   1. Er nennt **28** Plugins in einer Hand-Liste (`extern const
 *      uft_format_plugin_t uft_format_plugin_d64;` und so weiter). Die
 *      Registry fuehrt **137**. Rund vier Fuenftel der Plugins sah der
 *      Fuzzer nie — und eine Hand-Liste driftet zwangslaeufig: wer ein
 *      Plugin hinzufuegt, denkt nicht an diese Datei.
 *
 *   2. Er ruft ausschliesslich `probe`. Ein `probe` schaut auf einen
 *      Kopf und gibt bool zurueck. Die Fehler, die einen Benutzer treffen,
 *      stecken in `open`: dort wird geparst, gerechnet und **alloziert**.
 *
 * Dieser Test schliesst beide Luecken, und zwar ueber den Pfad, den ein
 * Benutzer wirklich nimmt:
 *
 *      uft_register_all_formats()      wie in main()
 *      -> Datei schreiben
 *      -> uft_disk_open(pfad, true)    Erkennung + Plugin-Auswahl + open
 *      -> uft_disk_get_geometry()
 *      -> uft_track_read()             mehrere Koordinaten, auch unsinnige
 *      -> uft_disk_close()
 *
 * Das ist ausdruecklich KEIN nachgebauter Pfad. Zweimal in diesem Baum
 * sind veroeffentlichte Zahlen falsch gewesen, weil ein Messaufbau den
 * Produktionspfad nachbaute statt ihn zu benutzen (MF-494, MF-497). Hier
 * wird dieselbe Funktion gerufen, die die Oberflaeche ruft.
 *
 * ── Was als Fehler gilt ──────────────────────────────────────────────────
 *
 * Ein Absturz, eine Endlosschleife, oder ein Speicherfehler unter den
 * ASan/UBSan-Laeufen der CI. **Nicht** als Fehler gilt: `uft_disk_open`
 * gibt NULL zurueck. Muell abzulehnen ist richtiges Verhalten, keine
 * Schwaeche — der Test zaehlt beide Ausgaenge und meldet die Verteilung,
 * damit sichtbar bleibt, wie viel ueberhaupt bis in ein `open` kommt.
 *
 * ── Reproduzierbarkeit ───────────────────────────────────────────────────
 *
 * Der Zufall ist ein eigener, gesetzter xorshift — nicht `rand()`. Der
 * gleiche Lauf erzeugt die gleichen Eingaben auf jeder Plattform, sonst
 * waere ein Fund nicht nachstellbar. Ein Fund druckt seinen Seed.
 */

#include "uft/uft_core.h"
#include "uft/uft_format_plugin.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ── Gesetzter Zufall, plattformgleich ─────────────────────────────────── */

static uint64_t rng_state = 0x9E3779B97F4A7C15ull;

static void rng_seed(uint64_t s)
{
    rng_state = s ? s : 0x9E3779B97F4A7C15ull;
}

static uint32_t rng_next(void)
{
    uint64_t x = rng_state;
    x ^= x << 13;
    x ^= x >> 7;
    x ^= x << 17;
    rng_state = x;
    return (uint32_t)(x >> 32);
}

/* ── Eingabe-Erzeugung ─────────────────────────────────────────────────── */

#define MAX_BLOB (256 * 1024)

static uint8_t blob[MAX_BLOB];

/** Reiner Zufall — trifft selten eine Signatur, deckt dafuer die
 *  Ablehnungspfade ab. */
static size_t make_random(size_t size)
{
    for (size_t i = 0; i < size; i++)
        blob[i] = (uint8_t)rng_next();
    return size;
}

/** Ein echter Plugin-Kopf, danach Muell. Das ist die gefaehrliche Sorte:
 *  die Erkennung greift, und der Parser laeuft auf unsinnigen Feldern
 *  weiter — genau dort entstehen Ueberlaeufe. */
static size_t make_signature_then_garbage(const char *sig, size_t siglen,
                                          size_t size)
{
    if (siglen > size) siglen = size;
    memcpy(blob, sig, siglen);
    for (size_t i = siglen; i < size; i++)
        blob[i] = (uint8_t)rng_next();
    return size;
}

/** Nullen: laesst Laengen- und Zaehlerfelder auf 0 laufen. Klassische
 *  Quelle von Division durch null und Schleifen ohne Abbruch. */
static size_t make_zeros(size_t size)
{
    memset(blob, 0, size);
    return size;
}

/** Nur 0xFF: laesst jedes Feld auf sein Maximum laufen. Klassische
 *  Quelle von Ueberlauf in `count * size`-Rechnungen. */
static size_t make_ones(size_t size)
{
    memset(blob, 0xFF, size);
    return size;
}

/* Koepfe, die in diesem Baum als Format-Signaturen dokumentiert sind.
 * Sie stehen hier NICHT, um ein Format zu pruefen, sondern um die
 * Erkennung absichtlich greifen zu lassen — der interessante Teil kommt
 * danach. */
static const struct { const char *sig; size_t len; const char *label; } SIGS[] = {
    { "SCP",           3, "SCP"      },
    { "HXCPICFE",      8, "HFE"      },
    { "HXCHFEV3",      8, "HFEv3"    },
    { "IMD ",          4, "IMD"      },
    { "TD",            2, "TD0"      },
    { "GCR-1541",      8, "G64"      },
    { "WOZ1",          4, "WOZ1"     },
    { "WOZ2",          4, "WOZ2"     },
    { "A2R2",          4, "A2R"      },
    { "MOOF",          4, "MOOF"     },
    { "MV - CPCEMU",  11, "DSK"      },
    { "EXTENDED",      8, "EDSK"     },
    { "\x96\x02",   2, "ATR"      },   /* ATR-Magie 0x0296, little-endian */
    { "Reduced MFM",  11, "DMK"      },
    { "CAPS",          4, "IPF"      },
    { "KryoFlux",      8, "KryoFlux" },
};
#define N_SIGS ((int)(sizeof(SIGS) / sizeof(SIGS[0])))

/* ── Der Produktionspfad ───────────────────────────────────────────────── */

static const char *tmp_path = "uft_fuzz_tmp.img";

static long n_open_ok, n_open_null, n_track_ok, n_track_null;

/** Schreibt den Blob und schickt ihn durch uft_disk_open() und alles,
 *  was ein Betrachter danach tut. */
static void run_open_path(size_t size)
{
    FILE *f = fopen(tmp_path, "wb");
    if (!f) {
        printf("  KANN TEMPDATEI NICHT SCHREIBEN: %s\n", tmp_path);
        exit(2);
    }
    if (fwrite(blob, 1, size, f) != size) {
        fclose(f);
        printf("  TEMPDATEI UNVOLLSTAENDIG GESCHRIEBEN\n");
        exit(2);
    }
    fclose(f);

    uft_disk_t *disk = uft_disk_open(tmp_path, true);
    if (!disk) {
        n_open_null++;
        return;                 /* Muell abzulehnen ist richtig. */
    }
    n_open_ok++;

    uft_geometry_t geo;
    memset(&geo, 0, sizeof(geo));
    (void)uft_disk_get_geometry(disk, &geo);

    /* Koordinaten absichtlich auch ausserhalb jeder gemeldeten Geometrie:
     * ein Plugin darf nicht darauf bauen, dass der Aufrufer sich an die
     * Geometrie haelt, die es selbst gemeldet hat.
     *
     * Gelesen wird ueber das Plugin, NICHT ueber uft_track_read(): dieser
     * Prototyp steht zwar in uft_core.h, hat aber im ganzen Baum keine
     * Definition und keinen Aufrufer (gemessen, MF-512). Ihn hier zu
     * benutzen wuerde den Test an ein Versprechen haengen, das niemand
     * einloest — siehe KNOWN_ISSUES ARCH-27. */
    static const int CYL[] = { 0, 1, 39, 79, 82, -1, 1000 };
    static const int HEAD[] = { 0, 1, -1, 7 };

    /* disk->plugin, NICHT uft_get_format_plugin(disk->format). MF-445 hat
     * genau das behoben: 82 Plugins teilen sich UFT_FORMAT_DSK, ein
     * Nachschlagen ueber die Format-ID liefert also ein FREMDES Plugin, das
     * dann auf plugin_data losgeht, die ein anderes alloziert hat. Die
     * erste Fassung dieses Tests machte diesen Fehler und stuerzte
     * prompt ab — der Absturz war meiner, nicht der des Werkzeugs. */
    const uft_format_plugin_t *plug = disk->plugin;
    for (size_t c = 0; c < sizeof(CYL) / sizeof(CYL[0]); c++) {
        for (size_t h = 0; h < sizeof(HEAD) / sizeof(HEAD[0]); h++) {
            if (!plug || !plug->read_track) { n_track_null++; continue; }
            uft_track_t trk;
            memset(&trk, 0, sizeof(trk));
            if (plug->read_track(disk, CYL[c], HEAD[h], &trk) == UFT_OK)
                n_track_ok++;
            else
                n_track_null++;
        }
    }

    uft_disk_close(disk);
}

/* ── Registry-getriebene probe-Abdeckung ───────────────────────────────── */

static long n_probe_calls, n_probe_hits, n_probe_bad_conf, n_probe_mutated;

static void run_all_probes_from_registry(size_t n_plugins, size_t size)
{
    static uint8_t shadow[MAX_BLOB];
    memcpy(shadow, blob, size);

    /* `size` selbst gehoert dazu. Die erste Fassung nannte nur feste Werte
     * und verfehlte damit genau den Fall, den uft_disk_open() erzeugt: dort
     * ist file_size die ECHTE Dateigroesse. Ein Fuzzer, der andere
     * Randbedingungen prueft als der Produktionspfad erzeugt, prueft den
     * Produktionspfad nicht. */
    const size_t FILE_SIZES[] = { 0, 1, 512, 174848, 1474560, size };

    for (size_t i = 0; i < n_plugins; i++) {
        const uft_format_plugin_t *p = uft_get_format_by_index(i);
        if (!p || !p->probe) continue;
        for (size_t k = 0; k < sizeof(FILE_SIZES) / sizeof(FILE_SIZES[0]); k++) {
            int confidence = -12345;
                    bool hit = p->probe(blob, size, FILE_SIZES[k], &confidence);
            n_probe_calls++;
            if (hit) {
                n_probe_hits++;
                /* uft_format_plugin.h dokumentiert Konfidenz als 0-100. */
                if (confidence < 0 || confidence > 100) {
                    printf("  KONFIDENZ AUSSERHALB 0-100: %s meldet %d\n",
                           p->name ? p->name : "(namenlos)", confidence);
                    n_probe_bad_conf++;
                }
            }
            /* probe nimmt const und darf nichts veraendern (DESIGN_PRINCIPLES:
             * keine stille Veraenderung). Geprueft statt geglaubt. */
            if (memcmp(shadow, blob, size) != 0) {
                printf("  PUFFER VERAENDERT DURCH: %s\n",
                       p->name ? p->name : "(namenlos)");
                n_probe_mutated++;
                memcpy(blob, shadow, size);
            }
        }
    }
}

/* ── Treiber ───────────────────────────────────────────────────────────── */

/** Wiedergabe: EINE Datei durch den Oeffnungspfad, mit Stufenanzeige.
 *
 *      ./test_disk_open_fuzz <datei>
 *
 * Ein Fuzzer ohne Wiedergabe zwingt dazu, den ganzen Lauf zu wiederholen,
 * um dieselbe Eingabe noch einmal zu sehen — und beim Eingrenzen ist genau
 * das die teuerste Schleife. Die Dateien unter tests/crashers/ sind damit
 * einzeln nachstellbar. */
static int replay(const char *path)
{
    printf("Wiedergabe: %s\n", path);

    printf("  [1] uft_probe_file_format ... ");
    const uft_format_plugin_t *w = uft_probe_file_format(path);
    printf("%s\n", w && w->name ? w->name : "(kein Treffer)");

    printf("  [2] uft_disk_open ... ");
    uft_disk_t *disk = uft_disk_open(path, true);
    if (!disk) { printf("abgelehnt\n"); return 0; }
    printf("geoeffnet, Plugin=%s\n",
           disk->plugin && disk->plugin->name ? disk->plugin->name : "?");

    uft_geometry_t geo;
    memset(&geo, 0, sizeof(geo));
    printf("  [3] uft_disk_get_geometry ... ");
    (void)uft_disk_get_geometry(disk, &geo);
    printf("%u Zylinder x %u Koepfe\n",
           (unsigned)geo.cylinders, (unsigned)geo.heads);

    static const int CYL[] = { 0, 1, 39, 79, 82, -1, 1000 };
    static const int HEAD[] = { 0, 1, -1, 7 };
    const uft_format_plugin_t *plug = disk->plugin;
    for (size_t c = 0; c < sizeof(CYL) / sizeof(CYL[0]); c++)
        for (size_t h = 0; h < sizeof(HEAD) / sizeof(HEAD[0]); h++) {
            if (!plug || !plug->read_track) continue;
            printf("  [4] read_track %d/%d ... ", CYL[c], HEAD[h]);
            uft_track_t trk;
            memset(&trk, 0, sizeof(trk));
            uft_error_t e = plug->read_track(disk, CYL[c], HEAD[h], &trk);
            printf("%d\n", (int)e);
        }

    printf("  [5] uft_disk_close ... ");
    uft_disk_close(disk);
    printf("fertig\n");
    return 0;
}

int main(int argc, char **argv)
{
    /* Ungepuffert. Ein Fuzzer, der abstuerzt, muss noch sagen koennen wo —
     * mit Zeilenpufferung in eine Pipe geht genau die letzte Zeile
     * verloren, also die einzige, die zaehlt. */
    setvbuf(stdout, NULL, _IONBF, 0);

    uft_error_t reg = uft_register_all_formats();
    if (argc > 1) {
        if (reg != UFT_OK) { printf("Registrierung schlug fehl\n"); return 1; }
        return replay(argv[1]);
    }
    size_t n_plugins = 0;
    while (uft_get_format_by_index(n_plugins) != NULL)
        n_plugins++;

    printf("=== Missgebildete Abbilder durch den echten Oeffnungspfad "
           "(MF-512) ===\n");
    printf("uft_register_all_formats() -> %d, Registry fuehrt %zu Plugins\n",
           (int)reg, n_plugins);

    if (n_plugins == 0) {
        printf("FEHLER: die Registry ist leer. Ohne registrierte Plugins\n"
               "        prueft dieser Test nichts — genau die Lage, die\n"
               "        MF-447 behoben hat.\n");
        return 1;
    }

    static const size_t SIZES[] = { 0, 1, 2, 15, 16, 511, 512, 513,
                                    4096, 65536, 174848 };
    const size_t N_SIZES = sizeof(SIZES) / sizeof(SIZES[0]);

    const uint64_t SEED = 0xC0FFEE123456789ull;
    rng_seed(SEED);

    long n_inputs = 0;

    for (size_t s = 0; s < N_SIZES; s++) {
        size_t size = SIZES[s];
        if (size > MAX_BLOB) continue;

        /* Vier feste Muster ... */
        printf("  Groesse %7zu: nullen", size);
        n_inputs++; run_all_probes_from_registry(n_plugins, make_zeros(size));
        run_open_path(size);
        printf(" einsen");
        n_inputs++; run_all_probes_from_registry(n_plugins, make_ones(size));
        run_open_path(size);
        printf(" zufall");
        n_inputs++; run_all_probes_from_registry(n_plugins, make_random(size));
        run_open_path(size);

        /* ... und jede dokumentierte Signatur mit Muell dahinter. */
        for (int g = 0; g < N_SIGS; g++) {
            printf(" %s", SIGS[g].label);
            n_inputs++;
            run_all_probes_from_registry(
                n_plugins,
                make_signature_then_garbage(SIGS[g].sig, SIGS[g].len, size));
            run_open_path(size);
        }
        printf("\n");
    }

    remove(tmp_path);

    printf("\nEingaben               : %ld\n", n_inputs);
    printf("probe-Aufrufe          : %ld  (%zu Plugins x %ld Eingaben x 6 "
           "Dateigroessen)\n", n_probe_calls, n_plugins, n_inputs);
    printf("  davon Treffer        : %ld\n", n_probe_hits);
    printf("uft_disk_open()        : %ld geoeffnet, %ld abgelehnt\n",
           n_open_ok, n_open_null);
    printf("uft_track_read()       : %ld geliefert, %ld abgelehnt\n",
           n_track_ok, n_track_null);

    long fail = n_probe_bad_conf + n_probe_mutated;
    printf("\nVertragsverletzungen   : %ld"
           "  (Konfidenz ausserhalb 0-100: %ld, Puffer veraendert: %ld)\n",
           fail, n_probe_bad_conf, n_probe_mutated);

    if (fail) {
        printf("\nFEHLGESCHLAGEN. Seed zum Nachstellen: 0x%llX\n",
               (unsigned long long)SEED);
        return 1;
    }

    /* Kein Absturz ist hier das eigentliche Ergebnis. Dass er ausbleibt,
     * ist unter ASan/UBSan (CI) eine staerkere Aussage als hier. */
    printf("OK — kein Absturz, keine Vertragsverletzung.\n");
    return 0;
}
