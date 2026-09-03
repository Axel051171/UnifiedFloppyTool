/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_fat12_fremd.c
 * @brief Das erste FAT12, das UFT nicht selbst erzeugt hat (MF-789).
 *
 * ── Warum das nötig war ──────────────────────────────────────────────────
 *
 * `src/fs/uft_fat12.c` steht in `docs/VERIFICATION_TIERS_FS.md` auf
 * **FS-T1** — mit dem Vermerk *„alle Tests bauen ihre Eingabe selbst —
 * **geprüft gegen den eigenen Erzeuger**."*
 *
 * Das ist die Selbstkonsistenz-Falle **eine Schicht unter** den
 * Format-Plugins, und sie ist dieselbe Klasse, die die fünf fabrizierten
 * Parser ermöglicht hat (FMT-2/3/10/11/12): grüne Tests gegen selbst
 * erzeugte Eingaben beweisen, dass der Leser zum Schreiber passt — nicht,
 * dass beide recht haben.
 *
 * Und sie wiegt hier schwerer als bei einem einzelnen Plugin, weil FAT12
 * **jedes** Format bedient, das es trägt.
 *
 * ── Woher das Abbild kommt ───────────────────────────────────────────────
 *
 * `mformat` aus **mtools 4.0.49** (GPL-3), gebaut unter WSL Ubuntu mit
 * gcc 15.2 — ohne `make`, das dort fehlt: die 68 Quellen ohne die
 * eigenständigen Programme (`floppyd*`, `mkmanifest`, `privtest`,
 * `file_read`) in einem Aufruf. Befehlszeile mit **expliziter** Geometrie:
 *
 *     mformat -i x.img -t 80 -h 2 -n 9 -N 12345678 -v UFTFAT12 -B tpl.bin ::
 *
 * ── Der Bootcode, gemessen und abgeschaltet ──────────────────────────────
 *
 * `mformat` schreibt standardmäßig **44 Bytes** ausführbaren Code hinter
 * den BPB. Gemessen an drei Abbildern:
 *
 *     ohne -B          44 B Code, Sprung eb3c90, Ende 55aa, OEM "MTOO4049"
 *     -B zero.bin       0 B Code, aber AUCH kein Sprung und kein 55aa
 *     -B tpl.bin        0 B Code, Sprung ebfe90, Ende 55aa, OEM "UFTCORP "
 *
 * Die dritte Zeile ist die benutzte: `tpl.bin` trägt nur **fünf eigene
 * Bytes** — `EB FE 90` (`jmp $` + `nop`, der übliche
 * Nicht-startbar-Stumpf) und `55 AA` — plus eine eigene OEM-Kennung. Der
 * **BPB dazwischen stammt von mtools**, und genau der ist die fremde
 * Aussage, die hier geprüft wird.
 *
 * Fremdcode im Abbild: **nein, gemessen** — 0 Bytes ungleich null im
 * Bereich 0x3E…0x1FD.
 *
 * ── Was dieser Test prüft ────────────────────────────────────────────────
 *
 * Nicht „liest UFT irgendetwas", sondern: **stimmt UFTs Deutung des BPB
 * mit dem überein, was eine fremde Hand hineingeschrieben hat.** Jede
 * Zahl unten ist eine Behauptung von mtools, gegen die UFT antreten muss.
 */

#include "uft/fs/uft_fat12.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build (tests/CMakeLists.txt)"
#endif

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-34s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                                   _fail++; return; } } while (0)

static uint8_t *lade(size_t *n)
{
    char p[512];
    snprintf(p, sizeof(p), "%s/mtools_fat12_720k.img", UFT_CORPUS_DIR);
    FILE *f = fopen(p, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    uint8_t *b = (uint8_t *)malloc((size_t)sz);
    if (!b || fread(b, 1, (size_t)sz, f) != (size_t)sz) {
        free(b); fclose(f); return NULL;
    }
    fclose(f);
    *n = (size_t)sz;
    return b;
}

/* Erst der Beleg, dass die Fixture ist, was sie zu sein behauptet:
 * ein fremder BPB, aber KEIN fremder Code. Ohne diesen Fall wäre der
 * Rest eine Prüfung gegen etwas Unbekanntes. */
TEST(kein_fremdcode_aber_ein_fremder_bpb)
{
    size_t n = 0;
    uint8_t *d = lade(&n);
    ASSERT(d != NULL);
    ASSERT(n == 737280);

    /* Die fuenf eigenen Bytes. */
    ASSERT(d[0] == 0xEB && d[1] == 0xFE && d[2] == 0x90);
    ASSERT(d[510] == 0x55 && d[511] == 0xAA);
    ASSERT(memcmp(d + 3, "UFTCORP ", 8) == 0);

    /* Und dazwischen: nichts Ausfuehrbares. */
    int code = 0;
    for (size_t i = 0x3E; i < 0x1FE; i++) if (d[i]) code++;
    ASSERT(code == 0);

    free(d);
}

/* DIE EIGENTLICHE PRUEFUNG: jede Zahl hier hat mtools geschrieben.
 *
 * Der BPB wird direkt aus dem Abbild gelesen (er IST die fremde
 * Aussage), und daneben steht, was UFTs Erkennung daraus macht. */
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

TEST(mtools_bpb_traegt_die_erwartete_geometrie)
{
    size_t n = 0;
    uint8_t *d = lade(&n);
    ASSERT(d != NULL);

    ASSERT(le16(d + 11) == 512);      /* Bytes je Sektor */
    ASSERT(d[13] == 2);               /* Sektoren je Cluster */
    ASSERT(le16(d + 17) == 112);      /* Wurzel-Eintraege */
    ASSERT(le16(d + 19) == 1440);     /* Sektoren gesamt */
    ASSERT(d[21] == 0xF9);            /* Media-Descriptor */
    ASSERT(le16(d + 24) == 9);        /* Sektoren je Spur */
    ASSERT(le16(d + 26) == 2);        /* Koepfe */

    free(d);
}

/* Und jetzt: erkennt UFT dieselbe Diskette darin? */
TEST(uft_erkennt_das_fremde_abbild)
{
    size_t n = 0;
    uint8_t *d = lade(&n);
    ASSERT(d != NULL);

    uft_fat_detect_t r;
    memset(&r, 0, sizeof(r));
    uft_fat12_detect(d, n, &r);

    ASSERT(r.valid);
    ASSERT(r.type == UFT_FAT_TYPE_FAT12);
    ASSERT(r.geometry != NULL);

    /* Die Geometrie, die UFT zuordnet, muss den BPB WIEDERGEBEN —
     * nicht bloss zur Dateigroesse passen. */
    ASSERT(r.geometry->total_sectors      == 1440);
    ASSERT(r.geometry->sectors_per_track  == 9);
    ASSERT(r.geometry->heads              == 2);
    ASSERT(r.geometry->sectors_per_cluster == 2);
    ASSERT(r.geometry->root_entries       == 112);
    ASSERT(r.geometry->media_type         == 0xF9);

    /* Keine der drei Warnungen darf stehen: das Abbild ist stimmig. */
    ASSERT(!r.boot_sig_missing);
    ASSERT(!r.bpb_inconsistent);
    /* MF-829: diese Zeile war gruen, WEIL niemand `fat_mismatch` setzte.
     * `fat_compared` macht sie erst zu einer Aussage. */
    ASSERT(r.fat_compared);
    ASSERT(!r.fat_mismatch);
    ASSERT(r.fat_diff_bytes == 0);

    free(d);
}

/* FAT[0] muss den Media-Descriptor wiederholen — hier F9.
 *
 * Das ist kein Detail: der Scout hat gemessen, dass ein anderer
 * FAT-Erzeuger (Flopgen) dort F8 schreibt, waehrend sein eigener BPB
 * F9 sagt. mtools ist stimmig; sobald eine zweite Hand eingetragen
 * wird, ist dieser Fall die Stelle, an der der Widerspruch auffaellt. */
TEST(fat_null_wiederholt_den_media_descriptor)
{
    size_t n = 0;
    uint8_t *d = lade(&n);
    ASSERT(d != NULL);

    size_t fat0 = (size_t)le16(d + 14) * le16(d + 11);   /* reservierte Sektoren */
    ASSERT(fat0 + 3 <= n);
    ASSERT(d[fat0]     == d[21]);      /* == Media-Descriptor */
    ASSERT(d[fat0 + 1] == 0xFF);
    ASSERT(d[fat0 + 2] == 0xFF);

    free(d);
}

/* ── Der Rotbeweis (MF-829) ───────────────────────────────────────────────
 *
 * `uft_fat_detect_t` fuehrt seit jeher ein Warnfeld `fat_mismatch`
 * („FAT copies don't match"). Gemessen ueber `git ls-files`: das Feld
 * wird im GANZEN Baum an keiner Stelle GESETZT — die einzigen zwei
 * Fundstellen sind seine eigene Deklaration und die Zusicherung
 * `ASSERT(!r.fat_mismatch)` weiter oben in dieser Datei.
 *
 * Damit ist diese Zusicherung gruen, WEIL die Pruefung fehlt. Sie
 * bestuende auch auf einem Abbild, dessen zwei FATs sich vollstaendig
 * unterscheiden. Das ist die vierte belegte Stelle dieser Klasse in
 * diesem Baum, und die guenstigste: die zweite FAT liegt bereits im
 * Puffer, den `uft_fat12_detect()` ohnehin bekommt.
 *
 * Dieser Test veraendert EIN Byte in der zweiten FAT. Vorher meldet
 * `uft_fat12_detect()` „keine Abweichung". */
TEST(zwei_verschiedene_fats_muessen_auffallen)
{
    size_t n = 0;
    uint8_t *d = lade(&n);
    ASSERT(d != NULL);

    uint16_t bps      = (uint16_t)(d[0x0B] | (d[0x0C] << 8));
    uint16_t reserved = (uint16_t)(d[0x0E] | (d[0x0F] << 8));
    uint8_t  num_fats = d[0x10];
    uint16_t fatsz    = (uint16_t)(d[0x16] | (d[0x17] << 8));

    /* Das Abbild muss die Voraussetzung ueberhaupt erfuellen. */
    ASSERT(num_fats == 2);
    ASSERT(fatsz > 0);

    size_t fat2 = (size_t)(reserved + fatsz) * bps;
    ASSERT(fat2 + (size_t)fatsz * bps <= n);

    /* Unveraendert: die beiden Kopien sind gleich — das ist die
     * Gegenprobe, ohne die der Rotbeweis nichts zeigt. */
    ASSERT(memcmp(d + (size_t)reserved * bps, d + fat2,
                  (size_t)fatsz * bps) == 0);

    /* Genau ein Byte in der ZWEITEN Kopie verdrehen. */
    d[fat2 + 3] = (uint8_t)(d[fat2 + 3] ^ 0xFF);

    uft_fat_detect_t r;
    memset(&r, 0, sizeof(r));
    uft_fat12_detect(d, n, &r);

    ASSERT(r.valid);          /* ein Unterschied macht das FS nicht ungueltig */
    ASSERT(r.fat_compared);   /* geprueft — nicht bloss „nichts gemeldet" */
    ASSERT(r.fat_mismatch);
    ASSERT(r.fat_diff_bytes == 1);   /* genau das eine verdrehte Byte */

    free(d);
}

int main(void)
{
    printf("test_fat12_fremd (MF-789) — Abbild von mtools 4.0.49\n");
    RUN(kein_fremdcode_aber_ein_fremder_bpb);
    RUN(mtools_bpb_traegt_die_erwartete_geometrie);
    RUN(uft_erkennt_das_fremde_abbild);
    RUN(fat_null_wiederholt_den_media_descriptor);
    RUN(zwei_verschiedene_fats_muessen_auffallen);
    printf("%d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail ? 1 : 0;
}
