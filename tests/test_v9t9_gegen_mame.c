/**
 * @file test_v9t9_gegen_mame.c
 * @brief V9T9: Seite 1 laeuft rueckwaerts (MF-1027)
 *
 * ── Das Orakel ──────────────────────────────────────────────────────
 *
 * MAME `src/lib/formats/ti99_dsk.cpp` / `.h`, **LGPL-2.1+**, Copyright
 * Michael Zapf. **Gelesen, nicht uebernommen** — Kanal *Spec* nach
 * MF-695. Vier Stellen tragen diesen Test:
 *
 *   Z. 883-908  der dokumentierende Kommentar zur Spurreihenfolge
 *   Z. 936-944  `identify()` — die sieben zulaessigen Dateigroessen
 *   Z. 989-991  die Fehlsektorkarte (768 Byte)
 *   Z. 1077     `logicaltrack = (head==0)? track : (2*trackcount-track-1)`
 *   ti99_dsk.h:70-84  die Feldlagen der VIB
 *
 * Der Kommentar sagt woertlich:
 *
 *     The TI file system orders all tracks on side 0 as going inwards,
 *     and then all tracks on side 1 going outwards.
 *         00 01 02 03 ... 38 39     side 0
 *         79 78 77 76 ... 41 40     side 1
 *     The SDF format stores the tracks and their sectors in logical
 *     order: 00 01 02 03 ... 38 39 [40 41 ... 79]
 *
 * ── Was der Vorzustand gemessen geliefert hat ───────────────────────
 *
 * UFT rechnete `off = (cyl * heads + head) * spt * 256` — zylinder-
 * verschraenkt UND ohne Umkehrung, also zwei unabhaengige Abweichungen:
 *
 *   SSSD  ( 92160 Byte, 40 Spuren)   **0 von 40** falsch
 *   DSSD  (184320 Byte, 80 Spuren)  **78 von 80** falsch, bis 179712 B
 *   DSDD  (368640 Byte, 80 Spuren)  **78 von 80** falsch, bis 359424 B
 *
 * Bei einseitigen Dateien geht die alte Formel in die richtige ueber
 * (`heads == 1` macht aus `cyl*1+0` genau `cyl`) — deshalb ist der
 * Fehler nie aufgefallen. Zweiseitig trafen genau zwei Spuren, und die
 * zweite (Kopf 1, Spur 26) rein zufaellig, weil `2t+1 = 79-t` bei
 * t = 26 aufgeht.
 *
 * Vier weitere Befunde: nur 3 von 7 Groessen angenommen; die
 * Spurzahl fest auf 40 (die beiden 80-Spur-Formate also unerreichbar);
 * die **VIB** in Sektor 0 nie gelesen, obwohl sie Sektoren je Spur,
 * Spuren je Seite und Seitenzahl nennt; und keine obere Schranke fuer
 * Zylinder oder Kopf.
 *
 * ── Warum die VIB hier entscheidet ──────────────────────────────────
 *
 * **184320 Byte sind zweideutig**: SSDD (1 Seite, 40 Spuren, 18
 * Sektoren) und DSSD (2 Seiten, 40 Spuren, 9 Sektoren) haben dieselbe
 * Groesse. MAMEs Kommentar sagt das woertlich und fragt deshalb die
 * VIB; ohne VIB nimmt es DSSD an. Dieser Test prueft alle drei Faelle.
 *
 * ── Und eine Stelle, an der UFT dem Orakel bewusst NICHT folgt ──────
 *
 * MAME benutzt die VIB-Angaben auch dann, wenn sie der Dateigroesse
 * widersprechen, und warnt nur. Hier gilt: die VIB ist eine
 * Behauptung, die Dateigroesse eine Tatsache — bei Widerspruch
 * gewinnt die Groesse, weil eine Geometrie, die nicht in die Datei
 * passt, beim Lesen hinter das Dateiende fuehrt und damit erfundene
 * Daten liefert. Auch das ist hier festgenagelt.
 *
 * Jeder Sektor ist **selbstbeschreibend** (Byte 0 Kopf, 1 Spur,
 * 2 Sektor), damit ein falscher Versatz den NACHBARN nennt. Die
 * Ausnahme ist der logische Sektor 0: dort steht die VIB, und der
 * Test sagt das ausdruecklich statt es zu verschweigen.
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

extern const uft_format_plugin_t uft_format_plugin_v9t9;

#define SS 256

/* VIB-Feldlagen, MAME ti99_dsk.h:70-84 */
#define VIB_SECSPERTRACK   12
#define VIB_ID             13
#define VIB_TRACKSPERSIDE  17
#define VIB_SIDES          18
#define VIB_DENSITY        19

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    if (ok) { gruen++; printf("  [OK]   %s\n", was); }
    else { rot++; printf("  [ROT]  %s\n         -> %s\n", was, detail); }
}

/* MAME ti99_dsk.cpp:1077 */
static long mame_offset(int head, int track, int spuren, int spt)
{
    const int logical = (head == 0) ? track : (2 * spuren - track - 1);
    return (long)logical * spt * SS;
}

/**
 * Eine SDF-Datei nach MAMEs Anordnung.
 *
 * @param vib_sides  0 = keine VIB schreiben, sonst die Seitenzahl, die
 *                   die VIB behaupten soll
 * @param vib_spuren Spuren je Seite, die die VIB behaupten soll
 * @param vib_spt    Sektoren je Spur, die die VIB behaupten soll
 */
static uint8_t *baue(int spuren, int koepfe, int spt, int badmap,
                     int vib_sides, int vib_spuren, int vib_spt,
                     size_t *groesse)
{
    const size_t nutz = (size_t)spuren * koepfe * spt * SS;
    const size_t n = nutz + (size_t)badmap;
    uint8_t *b = (uint8_t *)calloc(1, n);
    int h, t, s;
    assert(b != NULL);

    for (h = 0; h < koepfe; h++) {
        for (t = 0; t < spuren; t++) {
            const long off = mame_offset(h, t, spuren, spt);
            for (s = 0; s < spt; s++) {
                uint8_t *z = b + off + (long)s * SS;
                memset(z, 0x7E, SS);
                z[0] = (uint8_t)h;
                z[1] = (uint8_t)t;
                z[2] = (uint8_t)s;
            }
        }
    }

    if (vib_sides) {
        /* Die VIB liegt im logischen Sektor 0, also bei Versatz 0 —
         * sie ueberschreibt die Selbstbeschreibung von (0,0,0). */
        memcpy(b, "UFTPRUEF  ", 10);
        b[10] = (uint8_t)((vib_spuren * vib_sides * vib_spt) >> 8);
        b[11] = (uint8_t)((vib_spuren * vib_sides * vib_spt) & 0xFF);
        b[VIB_SECSPERTRACK]  = (uint8_t)vib_spt;
        b[VIB_ID]     = 'D';
        b[VIB_ID + 1] = 'S';
        b[VIB_ID + 2] = 'K';
        b[16] = ' ';
        b[VIB_TRACKSPERSIDE] = (uint8_t)vib_spuren;
        b[VIB_SIDES]         = (uint8_t)vib_sides;
        b[VIB_DENSITY]       = (vib_spt > 9) ? 2 : 1;
    }
    *groesse = n;
    return b;
}

static int schreibe(const char *pfad, const uint8_t *b, size_t n)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return 0;
    if (fwrite(b, 1, n, f) != n) { fclose(f); return 0; }
    fclose(f);
    return 1;
}

/* Eine Datei oeffnen und die gemeldete Geometrie zurueckgeben. */
static uft_error_t oeffne(const uint8_t *b, size_t n, const char *name,
                          unsigned *cyl, unsigned *heads, unsigned *spt)
{
    const char *tmp = getenv("TEMP");
    char pfad[512];
    uft_disk_t disk;
    uft_error_t rc;
    snprintf(pfad, sizeof(pfad), "%s/%s", tmp ? tmp : ".", name);
    if (!schreibe(pfad, b, n)) return UFT_ERROR_IO;
    memset(&disk, 0, sizeof(disk));
    rc = uft_format_plugin_v9t9.open(&disk, pfad, true);
    if (rc == UFT_OK) {
        *cyl = disk.geometry.cylinders;
        *heads = disk.geometry.heads;
        *spt = disk.geometry.sectors;
        uft_format_plugin_v9t9.close(&disk);
    } else {
        *cyl = *heads = *spt = 0;
    }
    remove(pfad);
    return rc;
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_v9t9;
    const char *tmp = getenv("TEMP");
    uint8_t *b;
    size_t n;

    printf("V9T9 gegen MAME ti99_dsk.cpp (LGPL-2.1+, nur gelesen)\n");
    printf("======================================================\n");

    /* ── 1. Alle sieben Groessen aus MAMEs identify() ──────────────── */
    {
        static const struct { long size; int cyl, heads, spt;
                              const char *name; } T[7] = {
            {   92160, 40, 1,  9, "SSSD"   },
            {  163840, 40, 1, 16, "SSDD16" },
            {  184320, 40, 2,  9, "DSSD"   },
            {  327680, 40, 2, 16, "DSDD16" },
            {  368640, 40, 2, 18, "DSDD"   },
            {  737280, 80, 2, 18, "DSDD80" },
            { 1474560, 80, 2, 36, "DSQD"   },
        };
        int k, gut = 0;
        char d[220] = "";
        for (k = 0; k < 7; k++) {
            unsigned c = 0, h = 0, s = 0;
            char nm[64];
            uint8_t *bb;
            size_t nn;
            bb = baue(T[k].cyl, T[k].heads, T[k].spt, 0, 0, 0, 0, &nn);
            snprintf(nm, sizeof(nm), "uft_v9t9_%s.dsk", T[k].name);
            if (nn == (size_t)T[k].size
                && oeffne(bb, nn, nm, &c, &h, &s) == UFT_OK
                && c == (unsigned)T[k].cyl && h == (unsigned)T[k].heads
                && s == (unsigned)T[k].spt)
                gut++;
            else if (!d[0])
                snprintf(d, sizeof(d), "%s (%ld Byte): gemeldet %u/%u/%u, "
                         "MAME sagt %d/%d/%d", T[k].name, T[k].size,
                         c, h, s, T[k].cyl, T[k].heads, T[k].spt);
            free(bb);
        }
        {
            char h2[280];
            snprintf(h2, sizeof(h2), "%d von 7 richtig%s%s", gut,
                     d[0] ? "; " : "", d);
            pruefe("alle SIEBEN Groessen aus MAMEs identify() werden "
                   "angenommen, jede mit der richtigen Geometrie",
                   gut == 7, h2);
        }
    }

    /* ── 2. Die Fehlsektorkarte (768 Byte) ─────────────────────────── */
    {
        unsigned c = 0, h = 0, s = 0;
        uft_error_t rc;
        char d[160];
        b = baue(40, 1, 9, 768, 0, 0, 0, &n);
        rc = oeffne(b, n, "uft_v9t9_badmap.dsk", &c, &h, &s);
        snprintf(d, sizeof(d), "%zu Byte -> rc=%d, %u/%u/%u", n, (int)rc,
                 c, h, s);
        pruefe("92160 + 768 Byte Fehlsektorkarte: angenommen als 40/1/9 "
               "(MAME zieht die 768 ab)",
               rc == UFT_OK && n == 92928 && c == 40 && h == 1 && s == 9,
               d);
        free(b);
    }

    /* ── 3. Die Zweideutigkeit bei 184320 Byte ─────────────────────── */
    {
        unsigned c = 0, h = 0, s = 0;
        char d[200];

        /* (a) ohne VIB -> MAMEs Rueckfall DSSD */
        b = baue(40, 2, 9, 0, 0, 0, 0, &n);
        oeffne(b, n, "uft_v9t9_amb_ohne.dsk", &c, &h, &s);
        snprintf(d, sizeof(d), "ohne VIB: %u/%u/%u", c, h, s);
        pruefe("184320 Byte ohne VIB -> 40/2/9 (DSSD, MAMEs Rueckfall)",
               c == 40 && h == 2 && s == 9, d);
        free(b);

        /* (b) VIB sagt DSSD */
        b = baue(40, 2, 9, 0, 2, 40, 9, &n);
        oeffne(b, n, "uft_v9t9_amb_dssd.dsk", &c, &h, &s);
        snprintf(d, sizeof(d), "VIB sagt 2 Seiten / 9 spt: %u/%u/%u",
                 c, h, s);
        pruefe("184320 Byte, VIB sagt DSSD -> 40/2/9",
               c == 40 && h == 2 && s == 9, d);
        free(b);

        /* (c) VIB sagt SSDD — dieselbe Groesse, andere Geometrie.
         * Das ist der Fall, den der Vorzustand NIE treffen konnte. */
        b = baue(40, 1, 18, 0, 1, 40, 18, &n);
        oeffne(b, n, "uft_v9t9_amb_ssdd.dsk", &c, &h, &s);
        snprintf(d, sizeof(d), "VIB sagt 1 Seite / 18 spt: %u/%u/%u "
                 "(Datei ist %zu Byte)", c, h, s, n);
        pruefe("184320 Byte, VIB sagt SSDD -> 40/1/18 (die Zweideutigkeit "
               "wird von der VIB aufgeloest, nicht geraten)",
               n == 184320 && c == 40 && h == 1 && s == 18, d);
        free(b);
    }

    /* ── 4. Eine VIB, die der Dateigroesse widerspricht ────────────── */
    {
        unsigned c = 0, h = 0, s = 0;
        char d[220];
        /* 184320-Byte-Datei, aber die VIB behauptet 80 Spuren. */
        b = baue(40, 2, 9, 0, 2, 80, 9, &n);
        oeffne(b, n, "uft_v9t9_vib_luegt.dsk", &c, &h, &s);
        snprintf(d, sizeof(d), "VIB sagt 80 Spuren, Datei hat %zu Byte "
                 "(= 40 Spuren): gemeldet %u/%u/%u", n, c, h, s);
        pruefe("VIB widerspricht der Dateigroesse -> die GROESSE gewinnt "
               "(40/2/9), denn eine Geometrie, die nicht in die Datei "
               "passt, liest hinter das Ende",
               c == 40 && h == 2 && s == 9, d);
        free(b);
    }

    /* ── 5. Der Spurversatz: alle 80 Spuren einer DSSD ─────────────── */
    {
        char pfad[512];
        uft_disk_t disk;
        int h, t, s;
        int falsche_zahl = 0, falsche_id = 0, fremd = 0;
        char erstes[240] = "";

        b = baue(40, 2, 9, 0, 0, 0, 0, &n);
        snprintf(pfad, sizeof(pfad), "%s/uft_v9t9_dssd.dsk",
                 tmp ? tmp : ".");
        assert(schreibe(pfad, b, n));
        memset(&disk, 0, sizeof(disk));
        if (p->open(&disk, pfad, true) != UFT_OK) {
            pruefe("DSSD laesst sich oeffnen", 0, "open schlug fehl");
        } else {
            for (h = 0; h < 2; h++) {
                for (t = 0; t < 40; t++) {
                    uft_track_t tr;
                    memset(&tr, 0, sizeof(tr));
                    if (p->read_track(&disk, t, h, &tr) != UFT_OK
                        || tr.sector_count != 9) {
                        falsche_zahl++;
                        free(tr.sectors);
                        free(tr.raw_data);
                        continue;
                    }
                    for (s = 0; s < 9; s++) {
                        const uint8_t *dd = tr.sectors[s].data;
                        if (tr.sectors[s].id.sector != (uint8_t)s) {
                            falsche_id++;
                            if (!erstes[0])
                                snprintf(erstes, sizeof(erstes),
                                         "K%d S%d Sektor %d hat ID %u "
                                         "(MAME: 0-basiert)", h, t, s,
                                         (unsigned)tr.sectors[s].id.sector);
                        }
                        /* (0,0,0) traegt die VIB nicht — diese Datei hat
                         * keine —, also ist jeder Sektor pruefbar. */
                        if (!dd || dd[0] != (uint8_t)h
                            || dd[1] != (uint8_t)t || dd[2] != (uint8_t)s) {
                            fremd++;
                            if (!erstes[0] && dd)
                                snprintf(erstes, sizeof(erstes),
                                         "K%d S%d Sektor %d liefert die "
                                         "Bytes von K%u S%u Sektor %u",
                                         h, t, s, (unsigned)dd[0],
                                         (unsigned)dd[1], (unsigned)dd[2]);
                        }
                    }
                    free(tr.sectors);
                    free(tr.raw_data);
                }
            }
            {
                char d[320];
                snprintf(d, sizeof(d), "%d Spuren falsche Sektorzahl, "
                         "%d falsche IDs, %d fremde Sektoren; erster: %s",
                         falsche_zahl, falsche_id, fremd,
                         erstes[0] ? erstes : "-");
                pruefe("alle 80 Spuren: Seite 1 laeuft RUECKWAERTS, "
                       "IDs 0..8, jeder Sektor liefert seine EIGENEN Bytes",
                       falsche_zahl == 0 && falsche_id == 0 && fremd == 0,
                       d);
            }

            /* Die Eckspur, an der der Vorzustand am weitesten daneben
             * lag: Kopf 1 Spur 0 ist die AEUSSERSTE Spur der zweiten
             * Seite und liegt am Dateiende (logische Spur 79). */
            {
                uft_track_t tr;
                char d[200];
                int ok = 0;
                memset(&tr, 0, sizeof(tr));
                if (p->read_track(&disk, 0, 1, &tr) == UFT_OK
                    && tr.sector_count == 9 && tr.sectors[0].data)
                    ok = (tr.sectors[0].data[0] == 1
                          && tr.sectors[0].data[1] == 0);
                snprintf(d, sizeof(d), "liefert K%u S%u (Sollversatz %ld, "
                         "der alte war %d)",
                         tr.sectors && tr.sectors[0].data
                            ? (unsigned)tr.sectors[0].data[0] : 255u,
                         tr.sectors && tr.sectors[0].data
                            ? (unsigned)tr.sectors[0].data[1] : 255u,
                         mame_offset(1, 0, 40, 9), (0 * 2 + 1) * 9 * SS);
                pruefe("Kopf 1 Spur 0 kommt vom DATEIENDE (Versatz 182016), "
                       "nicht von Position 2304", ok, d);
                free(tr.sectors);
                free(tr.raw_data);
            }

            /* Grenzen */
            {
                uft_track_t tr;
                uft_error_t a, c2;
                char d[160];
                memset(&tr, 0, sizeof(tr));
                a = p->read_track(&disk, 40, 0, &tr);
                free(tr.sectors); free(tr.raw_data);
                memset(&tr, 0, sizeof(tr));
                c2 = p->read_track(&disk, 0, 2, &tr);
                free(tr.sectors); free(tr.raw_data);
                snprintf(d, sizeof(d), "Spur 40 -> rc=%d, Kopf 2 -> rc=%d",
                         (int)a, (int)c2);
                pruefe("Spur 40 und Kopf 2 werden ABGEWIESEN — die "
                       "Umkehrung koennte sonst einen NEGATIVEN Versatz "
                       "ergeben", a != UFT_OK && c2 != UFT_OK, d);
            }
            p->close(&disk);
        }
        remove(pfad);
        free(b);
    }

    /* ── 6. Konfidenz: die VIB ist ein Merkmal, die Groesse nicht ──── */
    {
        int conf_ohne = -1, conf_mit = -1;
        bool ja_ohne, ja_mit;
        uint8_t *bo, *bm;
        size_t no, nm;
        char d[200];

        bo = baue(40, 1, 9, 0, 0, 0, 0, &no);
        bm = baue(40, 1, 9, 0, 1, 40, 9, &nm);
        ja_ohne = p->probe(bo, 4096, no, &conf_ohne);
        ja_mit  = p->probe(bm, 4096, nm, &conf_mit);
        snprintf(d, sizeof(d), "ohne VIB %d (probe=%d), mit VIB %d "
                 "(probe=%d)", conf_ohne, ja_ohne, conf_mit, ja_mit);
        /* MF-729: 30..49 = nur die Groesse, 80..100 = Merkmal getroffen */
        pruefe("Konfidenz: ohne VIB im 40er-Band, mit VIB >= 80 "
               "(\"DSK\" bei Versatz 13 ist ein Merkmal)",
               ja_ohne && conf_ohne >= 30 && conf_ohne < 50
               && ja_mit && conf_mit >= 80, d);
        free(bo);
        free(bm);
    }

    /* ── 7. Gegenproben ───────────────────────────────────────────────
     *
     * Die Eichung aus MF-729 verlangt, dass auf einem NULLPUFFER
     * nichts >= 50 meldet. Ein Nullpuffer hat keine VIB, also muss die
     * Konfidenz im 40er-Band bleiben — und eine Groesse, die MAMEs
     * Tafel nicht kennt, darf ueberhaupt nicht angenommen werden. */
    {
        uint8_t *null = (uint8_t *)calloc(1, 4096);
        int conf = -1;
        bool ja;
        char d[200];
        assert(null != NULL);
        ja = p->probe(null, 4096, 92160u, &conf);
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("Nullpuffer in V9T9-Groesse: angenommen, aber Konfidenz "
               "< 50 (MF-729-Eichung)", ja && conf < 50, d);

        conf = -1;
        ja = p->probe(null, 4096, 100000u, &conf);
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("100000 Byte steht in MAMEs Tafel nicht -> ABGEWIESEN",
               !ja, d);

        /* Und eine VIB in einer Datei, deren Groesse gar keine ist,
         * darf nichts retten. */
        memcpy(null + VIB_ID, "DSK", 3);
        null[VIB_SECSPERTRACK] = 9;
        null[VIB_TRACKSPERSIDE] = 40;
        null[VIB_SIDES] = 1;
        conf = -1;
        ja = p->probe(null, 4096, 100000u, &conf);
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("eine gueltige VIB in einer Datei falscher Groesse rettet "
               "sie NICHT", !ja, d);
        free(null);
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
