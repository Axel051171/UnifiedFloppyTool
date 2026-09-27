/**
 * @file test_86f_crc_flaggen.c
 * @brief 86F: ein Sektor mit falscher Pruefsumme ist kein guter Sektor
 *        (P3-595, MF-1377)
 *
 * ── Der Befund ──────────────────────────────────────────────────────────
 *
 * `f86_read_track()` dekodiert die Spur mit `uft_mfm_decode_track()` und
 * legt jeden Sektor mit Datenfeld ueber `uft_format_add_sector_with_id()`
 * an. Diese Funktion setzt unbedingt `UFT_SECTOR_OK` und beide
 * CRC-Flaggen auf gut; `id_crc_ok`, `data_crc_ok` und `deleted`, die der
 * Dekoder gemessen hatte, wurden nicht gelesen. Ein Sektor, dessen
 * Pruefsumme nicht stimmt, kam als gueltig heraus — Klasse MF-980/P3-300,
 * hier nicht erfunden, aber falsch bewertet.
 *
 * ── Die Probe ───────────────────────────────────────────────────────────
 *
 * An einer echten Diskette (`fluxfox_sector_test_360k.86f`, dbalsom/fluxfox,
 * MIT; Herkunft im Korpus-Manifest) werden in der Datei zwei Datenzellen
 * gekippt: eine im Datenfeld eines Sektors, eine im Zylinderfeld des
 * Kopfes eines ANDEREN Sektors. Die Spurlage rechnet der Test selbst nach
 * der 86Box-Spezifikation (docs/dev/formats/86f.rst: 8 Byte Kopf, dann
 * 32-Bit-Spur-Offsets; Spurkopf 2 Byte Flaggen + 32-Bit-Zellzahl).
 * WELCHE Zelle es ist, sagt der Dekoder selbst (`data_start_bit`,
 * `id_sync_bit`) — er wird hier zum Finden benutzt, nicht als Pruefling.
 *
 * Danach muss genau der eine Sektor als Daten-CRC-Fehler und genau der
 * andere als Kopf-CRC-Fehler herauskommen; alle uebrigen bleiben gut,
 * und alle tragen `UFT_SECTOR_CRC_CHECKED`, weil die Pruefsumme
 * nachgerechnet wurde.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"
#include "uft/flux/uft_mfm_sector_parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR "tests/corpus_free"
#endif

extern const uft_format_plugin_t uft_format_plugin_86f;

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s\n", was); }
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static const char *wegwerf(void)
{
    static char pfad[1024];
    const char *d = getenv("TMPDIR");
    if (!d || !*d) d = getenv("TMP");
    if (!d || !*d) d = getenv("TEMP");
    if (!d || !*d) d = ".";
    snprintf(pfad, sizeof pfad, "%s/uft_86f_crc_flaggen.86f", d);
    return pfad;
}

typedef struct {
    unsigned n, gut, geprueft, daten_falsch, kopf_falsch;
    int daten_falsch_r, kopf_falsch_c;
} bilanz_t;

static bool spur_lesen(const uint8_t *p, size_t n, bilanz_t *b)
{
    const char *tmp = wegwerf();
    FILE *f = fopen(tmp, "wb");
    if (!f) { printf("Wegwerf-Datei fehlgeschlagen: %s\n", tmp); exit(1); }
    fwrite(p, 1, n, f);
    fclose(f);
    memset(b, 0, sizeof *b);
    b->daten_falsch_r = -1; b->kopf_falsch_c = -1;
    uft_disk_t disk;
    uft_track_t t;
    memset(&disk, 0, sizeof disk);
    memset(&t, 0, sizeof t);
    bool ok = uft_format_plugin_86f.open(&disk, tmp, true) == UFT_OK &&
              uft_format_plugin_86f.read_track(&disk, 0, 0, &t) == UFT_OK;
    if (ok) {
        for (size_t k = 0; k < t.sector_count; k++) {
            const uft_sector_t *s = &t.sectors[k];
            b->n++;
            if (s->status & UFT_SECTOR_CRC_CHECKED) b->geprueft++;
            const bool d_falsch = !s->crc_ok && (s->status & UFT_SECTOR_CRC_ERROR);
            const bool k_falsch = !s->id_crc_ok && (s->status & UFT_SECTOR_ID_CRC_ERROR);
            if (d_falsch) { b->daten_falsch++; b->daten_falsch_r = s->id.sector; }
            if (k_falsch) { b->kopf_falsch++; b->kopf_falsch_c = s->id.cylinder; }
            if (!d_falsch && !k_falsch && s->crc_ok && s->id_crc_ok &&
                !(s->status & (UFT_SECTOR_CRC_ERROR | UFT_SECTOR_ID_CRC_ERROR)))
                b->gut++;
        }
    }
    uft_track_release(&t);
    if (disk.plugin_data) uft_format_plugin_86f.close(&disk);
    remove(tmp);
    return ok;
}

static void zelle_kippen(uint8_t *p, size_t bit)
{
    p[bit >> 3] ^= (uint8_t)(0x80u >> (bit & 7u));
}

int main(void)
{
    const char *pfad = UFT_CORPUS_DIR "/fluxfox_sector_test_360k.86f";
    FILE *f = fopen(pfad, "rb");
    if (!f) { printf("[ROT] Korpusdatei fehlt: %s\n", pfad); return 1; }
    fseek(f, 0, SEEK_END);
    const size_t n = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *p = (uint8_t *)malloc(n);
    if (!p || fread(p, 1, n, f) != n) { printf("[ROT] lesen\n"); return 1; }
    fclose(f);

    /* Spurlage nach der Spezifikation. */
    const uint16_t df = (uint16_t)(p[6] | (p[7] << 8));
    const uint32_t toff = le32(p + 8);
    pruefe("S1 Kennung 86BF und Disk-Flag Bit 7+12 (Zellzahl als Gesamtzahl)",
           memcmp(p, "86BF", 4) == 0 && (df & 0x0080u) && (df & 0x1000u));
    const uint32_t zellen = le32(p + toff + 2);
    const size_t daten = (size_t)toff + 10u;
    pruefe("S2 Spur 0/0 liegt ganz in der Datei",
           zellen > 0 && daten + (zellen + 7u) / 8u <= n);

    bilanz_t b;
    pruefe("A1 unveraendert: Spur 0/0 lesbar", spur_lesen(p, n, &b));
    char text[160];
    snprintf(text, sizeof text, "A2 unveraendert: %u von %u Sektoren gut", b.gut, b.n);
    pruefe(text, b.n == 9 && b.gut == 9);
    pruefe("A3 alle als geprueft gekennzeichnet (UFT_SECTOR_CRC_CHECKED)",
           b.geprueft == b.n && b.n > 0);

    /* Wo die Felder liegen, sagt der Dekoder. */
    uft_mfm_sector_t recs[32];
    static uint8_t pool[32u * 1024u];
    const size_t m = uft_mfm_decode_track(p + daten, zellen, pool, sizeof pool,
                                          recs, 32, NULL);
    int s_daten = -1, s_kopf = -1;
    for (size_t k = 0; k < m; k++) {
        if (!recs[k].dam_present) continue;
        if (recs[k].sector == 5) s_daten = (int)k;
        if (recs[k].sector == 7) s_kopf = (int)k;
    }
    pruefe("S3 Sektor 5 und Sektor 7 im Bitstrom gefunden", s_daten >= 0 && s_kopf >= 0);
    if (s_daten < 0 || s_kopf < 0) goto ende;

    /* Datenbyte 100 von Sektor 5: Datenzelle = Datenbeginn + 2*8*100 + 1.
     * Kopf von Sektor 7: 3 x A1 (48 Zellen) + FE (16) -> Zylinderfeld,
     * dessen hoechstes Datenbit bei +65 liegt. */
    zelle_kippen(p + daten, recs[s_daten].data_start_bit + 1600u + 1u);
    zelle_kippen(p + daten, recs[s_kopf].id_sync_bit + 65u);

    pruefe("B1 veraendert: Spur 0/0 lesbar", spur_lesen(p, n, &b));
    snprintf(text, sizeof text, "B2 genau EIN Sektor mit Daten-CRC-Fehler (%u), "
             "und es ist Sektor 5 (%d)", b.daten_falsch, b.daten_falsch_r);
    pruefe(text, b.daten_falsch == 1 && b.daten_falsch_r == 5);
    snprintf(text, sizeof text, "B3 genau EIN Sektor mit Kopf-CRC-Fehler (%u), "
             "sein Zylinderfeld traegt das gekippte Bit (%d)",
             b.kopf_falsch, b.kopf_falsch_c);
    pruefe(text, b.kopf_falsch == 1 && b.kopf_falsch_c == 0x80);
    snprintf(text, sizeof text, "B4 die uebrigen %u Sektoren bleiben gut", b.gut);
    pruefe(text, b.gut == 7 && b.n == 9);
    pruefe("B5 auch die schlechten sind als geprueft gekennzeichnet",
           b.geprueft == b.n);

ende:
    free(p);
    printf("test_86f_crc_flaggen: %d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}
