/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_mfi_kennung.c
 * @brief Der ZWEITE MFI-Leser und seine Kennung (MF-863)
 *
 * ── Was hier schiefging ──────────────────────────────────────────────
 *
 * Der Baum hat zwei MFI-Leser. Der grosse (`src/formats/mfi/uft_mfi.c`)
 * prueft die Kennung seit MF-614 richtig — 16 Byte einschliesslich der
 * abschliessenden Null — und wird von `test_mfi_layout` bewacht.
 *
 * Der kleine (`src/formats/flux/mfi.c`) prueft **drei** Byte:
 *
 *     char sig[3] = {0};
 *     if (fread(sig, 1, 3, fp) != 3) { }
 *     if (memcmp(sig, "MFI", 3) != 0) { fclose(fp); return UFT_EINVAL; }
 *
 * `"MFI"` ist weder die Kennung noch ihr Anfang. Die Folge geht in BEIDE
 * Richtungen falsch, und dieser Test prueft beide:
 *
 *   1. jede echte MFI-Datei wurde ABGEWIESEN
 *   2. jede Datei, die zufaellig mit "MFI" beginnt, wurde ANGENOMMEN —
 *      samt `dev->flux_supported = true`, also der Zusage, aus dieser
 *      Datei Flusswerte lesen zu koennen
 *
 * Dazu tat der Fehlerzweig des `fread` nichts: bei einer Datei unter drei
 * Byte lief der Vergleich mit dem genullten Puffer einfach weiter.
 *
 * ── Woher die 16 Byte stammen ────────────────────────────────────────
 *
 * Benannte Quelle: MAME `src/lib/formats/mfi_dsk.cpp:81-82`
 * (`sign` / `sign_old`), im Baum bereits zitiert in
 * `src/formats/mfi/uft_mfi.c:24,53`.
 *
 * **Nicht** an einer echten `.mfi`-Datei gegengeprueft — im Korpus liegt
 * keine. Der Wert kommt aus der benannten Quelle und aus dem zweiten
 * Leser, nicht aus einer Messung an einem Abbild.
 *
 * ── Ehrliche Einordnung der Schwere ──────────────────────────────────
 *
 * `uft_flx_mfi_open()` hat im ganzen Baum **keinen Aufrufer**. Der Fehler
 * war also nicht erreichbar. Er bleibt trotzdem ein Fehler: eine Tuer, die
 * das Falsche tut, ist schlimmer als keine Tuer, weil sie beim Verdrahten
 * niemandem auffaellt.
 */
#include "uft/floppy/uft_floppy_device.h"
#include "uft/formats/mfi.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int uft_flx_mfi_open(FloppyDevice *dev, const char *path);
int uft_flx_mfi_close(FloppyDevice *dev);

static int _pass = 0, _fail = 0, _last = 0;
#define RUN(n)  do { printf("  [TEST] %-46s ... ", #n); test_##n(); \
                     if (_last == _fail) { printf("OK\n"); _pass++; } \
                     _last = _fail; } while (0)
#define TEST(n) static void test_##n(void)
#define ASSERT(c) do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                       _fail++; return; } } while (0)

/** Schreibt @p len Byte nach @p pfad. Gibt 0 zurueck bei Erfolg. */
static int datei_schreiben(const char *pfad, const void *daten, size_t len)
{
    FILE *f = fopen(pfad, "wb");
    if (!f) return -1;
    size_t n = len ? fwrite(daten, 1, len, f) : 0;
    fclose(f);
    return (n == len) ? 0 : -1;
}

/** Ein MFI-Kopf: Kennung + 16 Byte Rest (MFI_HEADER_SIZE == 32). */
static size_t kopf_bauen(uint8_t *puffer, const char *kennung)
{
    memset(puffer, 0, 32);
    memcpy(puffer, kennung, UFT_MFI_MAGIC_LEN);
    puffer[16] = 0x02;                  /* form factor: 5.25" */
    return 32;
}

static const char *TMP_MFI  = "test_mfi_kennung_echt.tmp";
static const char *TMP_FAKE = "test_mfi_kennung_fremd.tmp";
static const char *TMP_KURZ = "test_mfi_kennung_kurz.tmp";

TEST(eine_echte_kennung_wird_angenommen)
{
    /* DER ROTBEWEIS, Richtung 1: vor MF-863 wies der Leser jede echte
     * MFI-Datei ab, weil er auf "MFI" verglich. */
    uint8_t kopf[32];
    size_t n = kopf_bauen(kopf, UFT_MFI_MAGIC);
    ASSERT(datei_schreiben(TMP_MFI, kopf, n) == 0);

    FloppyDevice dev;
    memset(&dev, 0, sizeof dev);
    int rc = uft_flx_mfi_open(&dev, TMP_MFI);

    if (rc != UFT_OK) {
        printf("\n      \"%s\" wurde abgewiesen (rc=%d)\n"
               "      -> der Leser prueft nicht die echte Kennung\n      ",
               UFT_MFI_MAGIC, rc);
        _fail++;
    } else {
        uft_flx_mfi_close(&dev);
    }
    remove(TMP_MFI);
}

TEST(die_alte_mess_kennung_wird_auch_angenommen)
{
    /* MAME mfi_dsk.cpp:81-82 fuehrt zwei Kennungen. Der grosse Leser
     * kennt beide; der kleine kannte keine. */
    uint8_t kopf[32];
    size_t n = kopf_bauen(kopf, UFT_MFI_MAGIC_OLD);
    ASSERT(datei_schreiben(TMP_MFI, kopf, n) == 0);

    FloppyDevice dev;
    memset(&dev, 0, sizeof dev);
    int rc = uft_flx_mfi_open(&dev, TMP_MFI);
    if (rc != UFT_OK) {
        printf("\n      \"%s\" wurde abgewiesen (rc=%d)\n      ",
               UFT_MFI_MAGIC_OLD, rc);
        _fail++;
    } else {
        uft_flx_mfi_close(&dev);
    }
    remove(TMP_MFI);
}

TEST(eine_fremde_datei_mit_MFI_am_anfang_wird_abgewiesen)
{
    /* DER ROTBEWEIS, Richtung 2 — und die wichtigere. Vor MF-863 nahm der
     * Leser diese Datei an und setzte `flux_supported = true`: eine
     * Zusage, aus ihr Flusswerte lesen zu koennen. */
    static const char FREMD[] = "MFI ist hier nur ein Wort im Text.";
    ASSERT(datei_schreiben(TMP_FAKE, FREMD, sizeof FREMD - 1) == 0);

    FloppyDevice dev;
    memset(&dev, 0, sizeof dev);
    int rc = uft_flx_mfi_open(&dev, TMP_FAKE);

    if (rc == UFT_OK) {
        printf("\n      eine Textdatei wurde als MFI angenommen"
               " (flux_supported=%d)\n      ", (int)dev.flux_supported);
        uft_flx_mfi_close(&dev);
        _fail++;
    }
    ASSERT(dev.flux_supported == false);
    remove(TMP_FAKE);
}

TEST(eine_zu_kurze_datei_ist_ein_befund_kein_nebenweg)
{
    /* EHRLICHE EINORDNUNG: dieser Fall war auch gegen den alten Leser
     * gruen — er ist KEIN Rotbeweis. Mit der 3-Byte-Pruefung konnte der
     * leere `fread`-Zweig nichts anrichten, weil eine zu kurze Datei am
     * genullten Puffer ohnehin an "MFI" scheiterte.
     *
     * Er wird trotzdem gebraucht, und zwar erst JETZT: mit der 16-Byte-
     * Pruefung wuerde eine 15-Byte-Datei ohne Laengenpruefung gegen ein
     * uninitialisiertes 16. Byte verglichen. Die Laengenpruefung ist
     * damit keine Kosmetik, sondern das, was den neuen Vergleich
     * definiert macht. */
    ASSERT(datei_schreiben(TMP_KURZ, "MA", 2) == 0);

    FloppyDevice dev;
    memset(&dev, 0, sizeof dev);
    int rc = uft_flx_mfi_open(&dev, TMP_KURZ);
    if (rc == UFT_OK) {
        printf("\n      eine 2-Byte-Datei wurde als MFI angenommen\n      ");
        uft_flx_mfi_close(&dev);
        _fail++;
    }
    remove(TMP_KURZ);
}

TEST(eine_leere_datei_wird_abgewiesen)
{
    ASSERT(datei_schreiben(TMP_KURZ, "", 0) == 0);

    FloppyDevice dev;
    memset(&dev, 0, sizeof dev);
    int rc = uft_flx_mfi_open(&dev, TMP_KURZ);
    if (rc == UFT_OK) {
        printf("\n      eine leere Datei wurde als MFI angenommen\n      ");
        uft_flx_mfi_close(&dev);
        _fail++;
    }
    remove(TMP_KURZ);
}

TEST(beide_leser_teilen_dieselbe_kennung)
{
    /* Gegenprobe zur Ursache, nicht zum Symptom: der Fehler konnte nur
     * entstehen, weil die Kennung an zwei Stellen stand. Wer sie kuenftig
     * wieder lokal definiert, faellt hier auf. */
    ASSERT(UFT_MFI_MAGIC_LEN == 16u);
    ASSERT(sizeof UFT_MFI_MAGIC == UFT_MFI_MAGIC_LEN);
    ASSERT(sizeof UFT_MFI_MAGIC_OLD == UFT_MFI_MAGIC_LEN);
    ASSERT(memcmp(UFT_MFI_MAGIC, "MAMEFLOPPYIMAGE", 15) == 0);
    ASSERT(memcmp(UFT_MFI_MAGIC_OLD, "MESSFLOPPYIMAGE", 15) == 0);
    /* Beide enden auf die Null — das ist das 16. Byte. */
    ASSERT(UFT_MFI_MAGIC[15] == '\0');
    ASSERT(UFT_MFI_MAGIC_OLD[15] == '\0');
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("=== MFI: die Kennung des zweiten Lesers (MF-863) ===\n");
    RUN(eine_echte_kennung_wird_angenommen);
    RUN(die_alte_mess_kennung_wird_auch_angenommen);
    RUN(eine_fremde_datei_mit_MFI_am_anfang_wird_abgewiesen);
    RUN(eine_zu_kurze_datei_ist_ein_befund_kein_nebenweg);
    RUN(eine_leere_datei_wird_abgewiesen);
    RUN(beide_leser_teilen_dieselbe_kennung);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
