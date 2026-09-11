/**
 * @file test_fdi_pc98_gegen_mame.c
 * @brief PC-98 FDI: der Feldabgleich fand KEINEN Fehler (MF-1026)
 *
 * ── Das Orakel ──────────────────────────────────────────────────────
 *
 * MAME `src/lib/formats/pc98fdi_dsk.cpp`, **BSD-3-Clause**, Copyright
 * Olivier Galibert. 110 Zeilen, und jede pruefbare Aussage steht
 * woertlich darin:
 *
 *   Kopffelder, alle `u32le` (Z. 48-53 / 71-75):
 *       0x08 hsize · 0x0C psize · 0x10 ssize · 0x14 scnt
 *       0x18 sides · 0x1C ntrk
 *   `identify()` (Z. 55) — ZWEI Bedingungen zugleich:
 *       `size == hsize + psize` UND `psize == ssize*scnt*sides*ntrk`
 *   Versatz (Z. 85):
 *       `hsize + ssize*scnt*(track*head_count + head)`  — zylinder-dur
 *   Sektornummern (Z. 92):
 *       `sects[i].sector = i + 1`  — **1-basiert**
 *
 * Bemerkenswert ist, was MAME NICHT liest: das Feld bei 0x04
 * („FDD type") kommt in `identify()` und `load()` nicht vor. Die
 * Geometrie steht explizit im Kopf; der Typ ist redundant, und wer ihn
 * auswertet, kann sich nur widersprechen.
 *
 * ── Was dieser Test ist, und was er nicht ist ───────────────────────
 *
 * Der Feldabgleich gegen MAME hat an
 * `src/formats/fdi_pc98/uft_fdi_pc98.c` **keinen Fehler gefunden**:
 * Versaetze, beide Konsistenzbedingungen, die Versatzformel und die
 * 1-basierten Sektornummern stimmen ueberein. Das ist die Lage aus
 * MF-1006 (`mgt`) — und dort steht auch die Lehre:
 *
 *   **Uebereinstimmung reicht nur, wenn sie BEWACHT ist statt
 *   behauptet.**
 *
 * Dieser Test nagelt sie deshalb fest. Er ist kein Beweis, dass UFT
 * PC-98-FDI richtig liest; er ist der Beweis, dass UFT und MAME
 * dieselbe Anordnung rechnen, und der Schutz davor, dass sich das
 * unbemerkt aendert.
 *
 * Jeder Sektor ist **selbstbeschreibend** (Byte 0 Spur, 1 Kopf,
 * 2 Sektor), damit ein Versatzfehler den NACHBARN nennt und nicht
 * bloss „falsche Bytes".
 *
 * **T2 und nicht T1b:** die Pruefdatei ist hauseigen. Es liegt keine
 * fremd erzeugte PC-98-FDI im Korpus, und keines der hier verfuegbaren
 * Werkzeuge schreibt eine (hxcfe kennt das Format nicht).
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

extern const uft_format_plugin_t uft_format_plugin_fdi_pc98;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int ok, const char *detail)
{
    if (ok) { gruen++; printf("  [OK]   %s\n", was); }
    else { rot++; printf("  [ROT]  %s\n         -> %s\n", was, detail); }
}

static void setze_le32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

/* Eine PC-98-FDI nach MAMEs Kopflayout und Versatzformel. */
static uint8_t *baue(uint32_t hsize, uint32_t ssize, uint32_t scnt,
                     uint32_t sides, uint32_t ntrk, size_t *groesse)
{
    uint32_t psize = ssize * scnt * sides * ntrk;
    size_t n = (size_t)hsize + psize;
    uint8_t *b = (uint8_t *)calloc(1, n);
    uint32_t t, h, s;
    assert(b != NULL);

    setze_le32(b + 0x00, 0);        /* reserved */
    setze_le32(b + 0x04, 0x90);     /* FDD type — von MAME ignoriert */
    setze_le32(b + 0x08, hsize);
    setze_le32(b + 0x0C, psize);
    setze_le32(b + 0x10, ssize);
    setze_le32(b + 0x14, scnt);
    setze_le32(b + 0x18, sides);
    setze_le32(b + 0x1C, ntrk);

    for (t = 0; t < ntrk; t++) {
        for (h = 0; h < sides; h++) {
            /* MAME Z. 85: hsize + ssize*scnt*(track*head_count + head) */
            size_t basis = (size_t)hsize
                           + (size_t)ssize * scnt * (t * sides + h);
            for (s = 0; s < scnt; s++) {
                uint8_t *z = b + basis + (size_t)s * ssize;
                memset(z, 0x3C, ssize);
                z[0] = (uint8_t)t;
                z[1] = (uint8_t)h;
                z[2] = (uint8_t)s;
            }
        }
    }
    *groesse = n;
    return b;
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_fdi_pc98;
    const char *tmp = getenv("TEMP");
    char pfad[512];
    uint8_t *b;
    size_t n;
    FILE *f;
    /* 2HD 1.2M: 77 Spuren, 2 Koepfe, 8 Sektoren, 1024 Byte. */
    const uint32_t HS = 4096, SS = 1024, SC = 8, SI = 2, NT = 77;

    printf("PC-98 FDI gegen MAME pc98fdi_dsk.cpp (BSD-3-Clause)\n");
    printf("====================================================\n");

    b = baue(HS, SS, SC, SI, NT, &n);
    {
        char d[120];
        snprintf(d, sizeof(d), "%zu Byte = 4096 + 77*2*8*1024", n);
        pruefe("MAMEs identify: size == hsize + ssize*scnt*sides*ntrk",
               n == (size_t)HS + (size_t)SS * SC * SI * NT, d);
    }

    snprintf(pfad, sizeof(pfad), "%s/uft_pc98.fdi", tmp ? tmp : ".");
    f = fopen(pfad, "wb");
    assert(f != NULL);
    assert(fwrite(b, 1, n, f) == n);
    fclose(f);

    /* ── 1. Sonde ──────────────────────────────────────────────────── */
    {
        int conf = -1;
        bool ja = p->probe(b, 4096, n, &conf);
        char d[120];
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", ja, conf);
        /* MF-729: >= 80 heisst „Merkmal getroffen" — hier sind es zwei
         * Konsistenzbedingungen, die zufaellig nicht zusammenpassen
         * koennen. */
        pruefe("Sonde nimmt an, Konfidenz >= 80 (zwei Bedingungen "
               "getroffen)", ja && conf >= 80, d);
    }

    /* ── 2. Gegenprobe: NUR MAMEs erste Bedingung verletzt ─────────────
     *
     * Hier stand zuerst `psize + 1 Sektor`. Das war gruen **aus dem
     * falschen Grund**: mit zu grossem `psize` faellt schon die ZWEITE
     * Bedingung (`size == hsize + psize`), und die erste kam nie zum
     * Tragen. Die Mutationsmatrix hat es bewiesen — F-M3 (die erste
     * Bedingung entfernen) rutschte **durch**. Dieselbe Falle wie in
     * MF-1014, wo 200 Dateien die Laengenpruefung trafen statt der
     * 128er-Grenze.
     *
     * Isoliert wird sie, indem die GEOMETRIE widerspricht und die
     * Dateigroesse stimmt: `scnt` auf 9 gesetzt heisst
     * `ssize*9*sides*ntrk != psize`, waehrend `size == hsize + psize`
     * weiterhin gilt. Damit ist die erste Bedingung der einzige
     * Ablehnungsgrund. */
    {
        uint8_t *kaputt = (uint8_t *)malloc(n);
        int conf = -1;
        bool ja;
        char d[200];
        memcpy(kaputt, b, n);
        setze_le32(kaputt + 0x14, SC + 1);          /* scnt 8 -> 9 */
        ja = p->probe(kaputt, 4096, n, &conf);
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d; psize=%u bleibt, "
                 "Geometrie sagt jetzt %u", ja, conf,
                 (unsigned)(SS * SC * SI * NT),
                 (unsigned)(SS * (SC + 1) * SI * NT));
        pruefe("Gegenprobe: NUR psize widerspricht der Geometrie "
               "(Dateigroesse stimmt) -> ABGEWIESEN", !ja, d);
        free(kaputt);
    }

    /* ── 3. Gegenprobe: MAMEs zweite Bedingung verletzt ────────────── */
    {
        int conf = -1;
        bool ja = p->probe(b, 4096, n + 512, &conf);   /* Datei zu gross */
        char d[140];
        snprintf(d, sizeof(d), "probe=%d, Konfidenz=%d", ja, conf);
        pruefe("Gegenprobe: Dateigroesse != hsize + psize -> ABGEWIESEN",
               !ja, d);
    }

    /* ── 4. Oeffnen und jede Spur pruefen ─────────────────────────── */
    {
        uft_disk_t disk;
        uft_error_t rc;
        memset(&disk, 0, sizeof(disk));
        rc = p->open(&disk, pfad, true);
        {
            char d[180];
            snprintf(d, sizeof(d), "rc=%d, %u Zyl, %u Koepfe, %u spt, "
                     "%u Byte/Sektor", (int)rc, disk.geometry.cylinders,
                     disk.geometry.heads, disk.geometry.sectors,
                     disk.geometry.sector_size);
            pruefe("open liest die Geometrie aus dem Kopf: 77 / 2 / 8 / 1024",
                   rc == UFT_OK && disk.geometry.cylinders == 77
                   && disk.geometry.heads == 2
                   && disk.geometry.sectors == 8
                   && disk.geometry.sector_size == 1024, d);
        }
        if (rc != UFT_OK) {
            printf("\n%d gruen, %d rot\n", gruen, rot);
            free(b);
            return 1;
        }

        {
            uint32_t t, h;
            int s;
            int falsche_zahl = 0, falsche_id = 0, fremd = 0;
            char erstes[220] = "";
            for (t = 0; t < NT; t++) {
                for (h = 0; h < SI; h++) {
                    uft_track_t tr;
                    memset(&tr, 0, sizeof(tr));
                    if (p->read_track(&disk, (int)t, (int)h, &tr) != UFT_OK
                        || tr.sector_count != SC) {
                        falsche_zahl++;
                        free(tr.sectors);
                        free(tr.raw_data);
                        continue;
                    }
                    for (s = 0; s < (int)SC; s++) {
                        const uint8_t *dd = tr.sectors[s].data;
                        /* MAME Z. 92: sects[i].sector = i + 1 */
                        if (tr.sectors[s].id.sector != (uint8_t)(s + 1)) {
                            falsche_id++;
                            if (!erstes[0])
                                snprintf(erstes, sizeof(erstes),
                                         "S%u K%u Sektor %d hat ID %u "
                                         "(MAME: i + 1)", t, h, s,
                                         (unsigned)tr.sectors[s].id.sector);
                        }
                        if (!dd || dd[0] != (uint8_t)t || dd[1] != (uint8_t)h
                            || dd[2] != (uint8_t)s) {
                            fremd++;
                            if (!erstes[0] && dd)
                                snprintf(erstes, sizeof(erstes),
                                         "S%u K%u Sektor %d liefert die "
                                         "Bytes von S%u K%u Sektor %u",
                                         t, h, s, (unsigned)dd[0],
                                         (unsigned)dd[1], (unsigned)dd[2]);
                        }
                    }
                    free(tr.sectors);
                    free(tr.raw_data);
                }
            }
            {
                char d[300];
                snprintf(d, sizeof(d), "%d Spuren mit falscher Sektorzahl, "
                         "%d falsche IDs, %d fremde Sektoren; erster: %s",
                         falsche_zahl, falsche_id, fremd,
                         erstes[0] ? erstes : "-");
                pruefe("alle 154 Spuren: 8 Sektoren, IDs 1..8, jeder "
                       "Sektor liefert seine EIGENEN Bytes (Versatzformel "
                       "zylinder-dur wie MAME)",
                       falsche_zahl == 0 && falsche_id == 0 && fremd == 0,
                       d);
            }
        }

        /* ── 5. Grenzen ────────────────────────────────────────────── */
        {
            uft_track_t tr;
            uft_error_t a, c;
            char d[160];
            memset(&tr, 0, sizeof(tr));
            a = p->read_track(&disk, 77, 0, &tr);
            free(tr.sectors); free(tr.raw_data);
            memset(&tr, 0, sizeof(tr));
            c = p->read_track(&disk, 0, 2, &tr);
            free(tr.sectors); free(tr.raw_data);
            snprintf(d, sizeof(d), "Spur 77 -> rc=%d, Kopf 2 -> rc=%d",
                     (int)a, (int)c);
            pruefe("Spur 77 und Kopf 2 werden ABGEWIESEN (0..76 / 0..1)",
                   a != UFT_OK && c != UFT_OK, d);
        }

        p->close(&disk);
    }

    remove(pfad);
    free(b);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
