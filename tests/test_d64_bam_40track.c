/**
 * @file test_d64_bam_40track.c
 * @brief 40-Spur-D64: die Belegungskarte darf nicht den Disknamen lesen (MF-648).
 *
 * Gemessener Fehler: d64_parse_bam() rechnet `entry_off = 4 + (track-1)*4`.
 * Fuer Spur 36 ergibt das 0x90 — und dieselbe Funktion liest den Disknamen
 * von `bam + 0x90`. Fuer jedes 40-Spur-Abbild sind die BAM-Eintraege der
 * Spuren 36..40 damit Namens- und ID-Bytes, und free_blocks ist falsch.
 *
 * Referenz fuer das erwartete Verhalten, benannt und in Zone GRUEN:
 * lib1541img (BSD-2-Clause, excess-c64), `src/lib/1541img/cbmdosvfsreader.c`
 * — es sondiert die erweiterten Belegungskarten (DolphinDOS bam+0x1c+4*track,
 * SpeedDOS bam+0x30+4*track, PrologicDOS inline) statt die 35-Spur-Formel
 * ueber Spur 35 hinaus fortzuschreiben. Der Kern der Aussage, den dieser
 * Test festhaelt: die Standard-BAM des 1541-Formats endet bei Spur 35,
 * weil ab 0x90 der Diskname steht.
 *
 * Der Test bindet die Uebersetzungseinheit direkt ein, weil d64_disk_v3_t
 * nur in der .c-Datei definiert ist. Damit laeuft er ueber genau den Code,
 * um den es geht — eine nachgebaute Struktur koennte still abdriften.
 * main() der Quelle haengt an D64_V3_TEST und wird hier nicht definiert.
 */

#include "../src/formats/d64/uft_d64_parser_v3.c"

#include <stdio.h>
#include <string.h>

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-38s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* Ein 40-Spur-Abbild mit einer BAM, deren Standardteil (Spuren 1..35)
 * bekannte Werte traegt und deren Disknamen-Bereich ab 0x90 mit einem
 * unverwechselbaren Muster belegt ist. Faellt eines dieser Namensbytes
 * als "freie Sektoren" einer Spur 36..40 heraus, ist der Fehler da. */
#define NAME_BYTE   0x55u   /* 85 — keine gueltige Sektorenzahl (max 21) */
#define STD_FREE    0x11u   /* 17 freie Sektoren je Standardspur */

static uint8_t *make_40track_image(void)
{
    uint8_t *img = (uint8_t *)calloc(1, D64_SIZE_40);
    if (!img) return NULL;

    size_t bam_off = d64_get_sector_offset(D64_BAM_TRACK, D64_BAM_SECTOR);
    uint8_t *bam = img + bam_off;

    bam[0] = 18;    /* Verweis auf erste Verzeichnisspur */
    bam[1] = 1;
    bam[2] = 0x41;  /* DOS-Typ 'A' */

    /* Standard-BAM: Spuren 1..35 bei 0x04 .. 0x8F */
    for (int t = 1; t <= 35; t++) {
        size_t off = 4u + (size_t)(t - 1) * 4u;
        bam[off]     = STD_FREE;
        bam[off + 1] = 0xFF;
        bam[off + 2] = 0xFF;
        bam[off + 3] = 0x1F;
    }

    /* Diskname 0x90..0x9F, Kennung 0xA2/0xA3, DOS-Kennung 0xA5/0xA6.
     * Der ganze Bereich 0x90..0xAA bekommt dasselbe Muster, damit jedes
     * Byte, das faelschlich als BAM gelesen wird, sofort auffaellt. */
    memset(bam + 0x90, NAME_BYTE, 0xAB - 0x90);

    return img;
}

/* Der eigentliche Rotbeweis: keine der Spuren 36..40 darf ein Namensbyte
 * als freie Sektorenzahl fuehren. */
TEST(bam_36_40_liest_nicht_den_disknamen)
{
    uint8_t *img = make_40track_image();
    ASSERT(img != NULL);

    d64_disk_v3_t disk;
    d64_params_t params;
    d64_get_default_params(&params);

    bool ok = d64_parse(img, D64_SIZE_40, &disk, &params);
    ASSERT(ok);
    ASSERT(disk.tracks == 40);

    for (int t = 36; t <= 40; t++) {
        if (disk.bam[t].free_sectors == NAME_BYTE) {
            printf("Spur %d fuehrt 0x%02X als freie Sektoren — das ist ein "
                   "Byte aus dem Disknamen (bam+0x%02zX)\n",
                   t, NAME_BYTE, 4u + (size_t)(t - 1) * 4u);
            _fail++;
            d64_disk_free(&disk);
            free(img);
            return;
        }
    }

    d64_disk_free(&disk);
    free(img);
}

/* free_blocks darf keine Phantombloecke aus dem Namensbereich enthalten.
 * Erwartet: 34 Standardspuren a 17 (Spur 18 zaehlt nicht mit). */
TEST(free_blocks_ohne_phantombloecke)
{
    uint8_t *img = make_40track_image();
    ASSERT(img != NULL);

    d64_disk_v3_t disk;
    d64_params_t params;
    d64_get_default_params(&params);

    ASSERT(d64_parse(img, D64_SIZE_40, &disk, &params));

    const uint16_t erwartet = (uint16_t)(34u * STD_FREE);
    if (disk.free_blocks != erwartet) {
        printf("free_blocks = %u, erwartet %u (Differenz %d)\n",
               disk.free_blocks, erwartet, (int)disk.free_blocks - (int)erwartet);
        _fail++;
    }

    d64_disk_free(&disk);
    free(img);
}

/* Regressionsschutz: das 35-Spur-Abbild muss unveraendert gelesen werden. */
TEST(35_spuren_unveraendert)
{
    uint8_t *img = (uint8_t *)calloc(1, D64_SIZE_35);
    ASSERT(img != NULL);

    uint8_t *bam = img + d64_get_sector_offset(D64_BAM_TRACK, D64_BAM_SECTOR);
    bam[0] = 18; bam[1] = 1; bam[2] = 0x41;
    for (int t = 1; t <= 35; t++) {
        size_t off = 4u + (size_t)(t - 1) * 4u;
        bam[off] = STD_FREE;
        bam[off + 1] = 0xFF; bam[off + 2] = 0xFF; bam[off + 3] = 0x1F;
    }
    memset(bam + 0x90, NAME_BYTE, 0xAB - 0x90);

    d64_disk_v3_t disk;
    d64_params_t params;
    d64_get_default_params(&params);

    ASSERT(d64_parse(img, D64_SIZE_35, &disk, &params));
    ASSERT(disk.tracks == 35);
    ASSERT(disk.free_blocks == (uint16_t)(34u * STD_FREE));
    ASSERT(disk.bam[35].free_sectors == STD_FREE);

    d64_disk_free(&disk);
    free(img);
}

int main(void)
{
    printf("=== D64 40-Spur-BAM (MF-648) ===\n");
    RUN(bam_36_40_liest_nicht_den_disknamen);
    RUN(free_blocks_ohne_phantombloecke);
    RUN(35_spuren_unveraendert);
    printf("  %d bestanden, %d gescheitert\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
