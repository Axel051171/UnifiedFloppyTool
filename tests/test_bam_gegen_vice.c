/**
 * @file test_bam_gegen_vice.c
 * @brief Die erzeugte BAM muss dort stehen, wo ein 1541 sie sucht (MF-992).
 *
 * ── Der Befund ───────────────────────────────────────────────────────
 *
 * `bam_create_d64()` legt eine formatierte D64 im Speicher an. Der Baum
 * hat dafuer `tests/test_bam_editor.c` mit zehn Faellen, alle gruen.
 *
 * Gemessen gegen ein **fremd erzeugtes** Abbild —
 * `tests/corpus_free/vice_c1541_35trk.d64`, von VICEs `c1541`, genau der
 * Referenz, die `uft_d64_plugin.c` in seinem `spec_status` selbst nennt
 * — war die BAM an keiner einzigen Stelle dort, wo sie hingehoert:
 *
 *     erzeugt:  12 01 41 00 | 00 00 00 00 | 15 FF FF 1F ...
 *     VICE   :  12 01 41 00 | 15 FF FF 1F | 15 FF FF 1F ...
 *
 * Ursache: `bam_track_entry_t tracks[BAM_MAX_TRACKS + 1]` mit
 * BAM_MAX_TRACKS = 42 — 43 Eintraege, wo das Format 35 hat, und ein
 * Phantom-Eintrag fuer eine „Spur 0", die es nicht gibt. Folge:
 *
 *     Spur-1-Eintrag    vier Byte zu spaet
 *     Spur-18-Eintrag   vier Byte zu spaet
 *     Diskettenname     ZWEIUNDDREISSIG Byte zu spaet
 *
 * Ein 1541-DOS liest bei +0x04 die Belegung von Spur 1 und findet
 * Nullen — „keine freien Sektoren" — und bei +0x90 den Diskettennamen,
 * wo ebenfalls nichts steht.
 *
 * ── Warum es niemandem auffiel ───────────────────────────────────────
 *
 * `test_bam_editor.c` liest durch **dieselbe** Struktur zurueck, mit der
 * geschrieben wurde. Die zehn Faelle waren in sich schluessig und gegen
 * die Welt falsch. Das ist die Fehlerklasse, gegen die dieser Baum sein
 * Korpus aufgebaut hat: eine Umsetzung, die nur sich selbst befragt,
 * kann jede Zusage halten und trotzdem unbrauchbar sein.
 *
 * Dieser Test befragt eine fremde Hand.
 *
 * ── Was NICHT verglichen wird ────────────────────────────────────────
 *
 * VICEs Abbild traegt eine Datei (Spur 17 belegt, Name „UFTCORPUS").
 * Verglichen wird deshalb, was **Format** ist und nicht Inhalt: die vier
 * Kopfbytes, der Spur-18-Eintrag (der auf einer frisch formatierten
 * Diskette wie auf dieser gleich aussieht), die Lage des Namensfeldes
 * und die DOS-Kennung. Der Unterschied steht hier, damit niemand den
 * Vergleich spaeter auf die ganze BAM ausdehnt und sich wundert.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "uft/formats/c64/uft_bam_editor.h"

#ifndef UFT_CORPUS_DIR
#error "UFT_CORPUS_DIR must be defined by the build"
#endif

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

/* Spur 18, Sektor 0. Spuren 1..17 haben je 21 Sektoren. */
#define BAM_OFF        (17 * 21 * 256)      /* 91392 */
/* Lage der Felder IM BAM-Sektor — aus dem VICE-Abbild gelesen. */
#define OFF_SPUR(t)    (4 + ((t) - 1) * 4)
#define OFF_NAME       0x90
#define OFF_KENNUNG    0xA2
#define OFF_DOSTYP     0xA5

static uint8_t *vice_bam(void)
{
    FILE *f = fopen(UFT_CORPUS_DIR "/vice_c1541_35trk.d64", "rb");
    if (!f) return NULL;
    uint8_t *s = malloc(256);
    if (!s) { fclose(f); return NULL; }
    if (fseek(f, BAM_OFF, SEEK_SET) != 0 || fread(s, 1, 256, f) != 256) {
        free(s); s = NULL;
    }
    fclose(f);
    return s;
}

static void zeige(const char *was, const uint8_t *a, const uint8_t *b,
                  int off, int len)
{
    printf("\n        %-14s erzeugt", was);
    for (int i = 0; i < len; i++) printf(" %02X", a[off + i]);
    printf("\n        %-14s VICE   ", "");
    for (int i = 0; i < len; i++) printf(" %02X", b[off + i]);
    printf("\n        ");
}

/* ═══ 1. Die vier Kopfbytes und die Lage des ersten Spureintrags ═════
 *
 * Der schaerfste Einzelbefund: auf einer echten D64 steht der Eintrag
 * fuer Spur 1 DIREKT hinter den vier Kopfbytes. Es gibt keinen Platz
 * fuer eine „Spur 0".                                                  */
static void test_spur_eins_steht_direkt_hinter_dem_kopf(void)
{
    uint8_t *vice = vice_bam();
    ASSERT(vice != NULL);

    uint8_t *d = NULL; size_t n = 0;
    ASSERT(bam_create_d64(35, "", "\xA0\xA0", &d, &n) == 0);
    ASSERT(n == BAM_D64_35_TRACKS);
    const uint8_t *bam = d + BAM_OFF;

    /* Kopf: 18, 1, 'A', 0 */
    ASSERT(memcmp(bam, vice, 4) == 0);

    /* Spur 1: 21 freie Sektoren, Bitmap FF FF 1F — auf einer frischen
     * Diskette wie auf VICEs (dort ist Spur 1 unbelegt). */
    if (memcmp(bam + OFF_SPUR(1), vice + OFF_SPUR(1), 4) != 0)
        zeige("Spur 1", bam, vice, OFF_SPUR(1), 4);
    ASSERT(memcmp(bam + OFF_SPUR(1), vice + OFF_SPUR(1), 4) == 0);

    free(d); free(vice);
}

/* ═══ 2. Die Verzeichnisspur ════════════════════════════════════════
 *
 * Spur 18 traegt BAM (18/0) und den ersten Verzeichnissektor (18/1) als
 * belegt, die uebrigen 17 frei — das Verzeichnis waechst hinein. Genau
 * so steht es in VICEs Abbild, und dieser Fall haelt das fest.
 *
 * Nebenbei nimmt er eine Vermutung zurueck: der Verdacht war, die ganze
 * Spur 18 muesse belegt sein („664 BLOCKS FREE"). Die Messung sagt
 * nein — die 664 entstehen dadurch, dass der ZAEHLER die Spur 18
 * ueberspringt, nicht dadurch, dass sie belegt waere.                  */
static void test_verzeichnisspur_gleicht_der_von_vice(void)
{
    uint8_t *vice = vice_bam();
    ASSERT(vice != NULL);

    uint8_t *d = NULL; size_t n = 0;
    ASSERT(bam_create_d64(35, "", "\xA0\xA0", &d, &n) == 0);
    const uint8_t *bam = d + BAM_OFF;

    if (memcmp(bam + OFF_SPUR(18), vice + OFF_SPUR(18), 4) != 0)
        zeige("Spur 18", bam, vice, OFF_SPUR(18), 4);
    ASSERT(memcmp(bam + OFF_SPUR(18), vice + OFF_SPUR(18), 4) == 0);

    /* Und die Werte ausgeschrieben, damit der Sollwert im Klartext
     * dasteht und nicht nur als Vergleich: 17 frei, 18/0 und 18/1 weg. */
    ASSERT(bam[OFF_SPUR(18) + 0] == 17);
    ASSERT(bam[OFF_SPUR(18) + 1] == 0xFC);

    free(d); free(vice);
}

/* ═══ 3. Der Diskettenname liegt, wo er hingehoert ══════════════════
 *
 * Verglichen wird die LAGE, nicht der Inhalt: VICEs Abbild heisst
 * „UFTCORPUS", unseres ist unbenannt. Beide muessen ihr Namensfeld bei
 * +0x90 haben, und die DOS-Kennung „2A" bei +0xA5.                     */
static void test_namensfeld_liegt_bei_0x90(void)
{
    uint8_t *vice = vice_bam();
    ASSERT(vice != NULL);

    uint8_t *d = NULL; size_t n = 0;
    ASSERT(bam_create_d64(35, "", "\xA0\xA0", &d, &n) == 0);
    const uint8_t *bam = d + BAM_OFF;

    /* VICE traegt dort einen Namen — also darf dort nicht Null stehen. */
    ASSERT(vice[OFF_NAME] != 0x00);

    /* Unser Name ist blank: 16x 0xA0 ab +0x90. */
    if (bam[OFF_NAME] != 0xA0)
        zeige("Name", bam, vice, OFF_NAME, 8);
    for (int i = 0; i < 16; i++)
        ASSERT(bam[OFF_NAME + i] == 0xA0);

    /* DOS-Typ „2A" an derselben Stelle wie bei VICE. */
    ASSERT(bam[OFF_DOSTYP + 0] == vice[OFF_DOSTYP + 0]);   /* '2' */
    ASSERT(bam[OFF_DOSTYP + 1] == vice[OFF_DOSTYP + 1]);   /* 'A' */

    /* Die Kennung ist bei uns blank, bei VICE „42" — geprueft wird nur,
     * dass an dieser Stelle ueberhaupt unser Wert steht. */
    ASSERT(bam[OFF_KENNUNG + 0] == 0xA0);
    ASSERT(bam[OFF_KENNUNG + 1] == 0xA0);

    free(d); free(vice);
}

/* ═══ 4. Gegenprobe: der Zaehler bleibt bei 664 ═════════════════════
 *
 * Ohne diesen Fall koennte die Verschiebung „behoben" werden, indem
 * irgendetwas anderes kaputtgeht. 664 ist die Zahl, die eine frisch
 * formatierte 1541-Diskette meldet: 683 Sektoren minus die 19 der
 * Verzeichnisspur.                                                     */
static void test_freie_bloecke_sind_664(void)
{
    uint8_t *d = NULL; size_t n = 0;
    ASSERT(bam_create_d64(35, "", "\xA0\xA0", &d, &n) == 0);

    bam_editor_t *ed = bam_editor_create(d, n);
    ASSERT(ed != NULL);
    const int frei = bam_get_free_blocks(ed);
    if (frei != 664) printf("\n        frei = %d, erwartet 664\n        ", frei);
    ASSERT(frei == 664);

    bam_editor_free(ed);
    free(d);
}

/* ═══ 5. Ein 40-Spur-Abbild hat ein blankes Namensfeld ══════════════
 *
 * Ein VERTRAG, kein Beweis — und der Unterschied ist gemessen.
 *
 * Die Formatierschleife lief frueher bis `num_tracks`. Bei 40 Spuren
 * schrieb sie fuenf Eintraege ueber das Ende der BAM-Tafel hinaus, also
 * in den Bereich von Name, Kennung und DOS-Typ. Seit MF-992 endet sie
 * bei 35, und `bam_spur()` gibt jenseits davon NULL.
 *
 * **Die Mutationsmatrix faellt diesen Fall trotzdem nicht** — weder M3
 * (Schranke im Zugriffshelfer entfernt) noch M4 (Schranke zusaetzlich in
 * allen drei Schleifen entfernt). Der Grund: `bam_format_disk()` setzt
 * den Diskettennamen NACH der Spurschleife und ueberschreibt den
 * Ueberlauf damit selbst.
 *
 * Harmlos ist er deshalb nicht: er schreibt weiterhin ausserhalb von
 * `tracks[35]`, und das ist undefiniertes Verhalten, kein Schoenheits-
 * fehler. Nur faengt es kein Funktionstest, sondern der **Sanitizer** —
 * und der laeuft in CI ueber genau diese Datei. Die Schranke bleibt
 * deshalb, und dieser Fall haelt fest, was ein Anwender sieht: ein
 * 40-Spur-Abbild hat ein blankes Namensfeld und einen lesbaren DOS-Typ.
 *
 * Was hier NICHT entschieden wird: wo die Belegung der Spuren 36..40
 * hingehoert. SpeedDOS und DolphinDOS legen sie verschieden ab, und im
 * Korpus liegt dafuer kein Abbild. UFT schreibt dort nichts, statt zu
 * raten. */
static void test_vierzig_spuren_lassen_den_namen_in_ruhe(void)
{
    uint8_t *d = NULL; size_t n = 0;
    ASSERT(bam_create_d64(40, "", "  ", &d, &n) == 0);
    ASSERT(n == BAM_D64_40_TRACKS);
    const uint8_t *bam = d + BAM_OFF;

    /* Das Namensfeld muss blank sein, nicht mit Spureintraegen gefuellt. */
    for (int i = 0; i < 16; i++) {
        if (bam[OFF_NAME + i] != 0xA0) {
            printf("\n        Name +%d = %02X statt A0 (Spureintrag?)\n        ",
                   i, bam[OFF_NAME + i]);
            break;
        }
    }
    for (int i = 0; i < 16; i++)
        ASSERT(bam[OFF_NAME + i] == 0xA0);

    /* Und der DOS-Typ steht noch da. */
    ASSERT(bam[OFF_DOSTYP + 0] == '2');
    ASSERT(bam[OFF_DOSTYP + 1] == 'A');

    free(d);
}

int main(void)
{
    printf("=== Die BAM muss dort stehen, wo ein 1541 sie sucht (MF-992) ===\n");
    RUN(spur_eins_steht_direkt_hinter_dem_kopf);
    RUN(verzeichnisspur_gleicht_der_von_vice);
    RUN(namensfeld_liegt_bei_0x90);
    RUN(freie_bloecke_sind_664);
    RUN(vierzig_spuren_lassen_den_namen_in_ruhe);
    printf("\n%d Pruefungen gruen, %d rot\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
