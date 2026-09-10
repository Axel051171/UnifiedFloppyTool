/**
 * @file test_cfi_schreibt_in_die_datei.c
 * @brief `cfi_write_track()` muss die Datei erreichen, nicht nur den
 *        Speicher (MF-1004).
 *
 * ── Woher die Frage kommt ────────────────────────────────────────────
 *
 * **MF-930** fand elf Plugins mit einem vollstaendigen, aber
 * unerreichbaren Dateischreiber: `write_track` machte ein `memcpy` in
 * die Speicherkopie und meldete `UFT_OK`, `close()` gab den Puffer frei,
 * `plugin->flush` wird im ganzen Baum von niemandem gerufen. Der
 * Benutzer glaubte, gespeichert zu haben.
 *
 * Seither antworten sie `UFT_ERROR_NOT_SUPPORTED`. Die Verdrahtung ist
 * je Format eine eigene Aufgabe mit eigenem Rundlaufbeweis — **P3-204**.
 * `opus` war der erste (MF-931), `cfi` ist der zweite.
 *
 * ── Warum `cfi` als zweiter drankam ─────────────────────────────────
 *
 * **Weil seine Leseseite zuerst gehoben wurde.** MF-1004 hat
 * `uft_cfi.c` Feld fuer Feld gegen `src/samdisk/cfi.cpp` (`ReadCFI()`)
 * gehalten und vier stille Kuerzungen beseitigt; `cfi` steht seitdem
 * auf **T2** statt T3.
 *
 * Das ist die Reihenfolge aus P3-204, und sie ist im Baum belegt:
 * `f3c8099a` (MF-905) hob den OPD-Leser gegen ein Orakel, erst danach
 * verdrahtete `c601a789` (MF-931) den Schreiber. Ein Rundlauf durch
 * einen UNGEPRUEFTEN Leser beweist nur, dass unser Leser unseren
 * Schreiber versteht — das ist die Selbstbestaetigung aus MF-992.
 *
 * ── Was dieser Test belegt, und was ausdruecklich NICHT ─────────────
 *
 * Er belegt: die Bytes erreichen die **Datei**. Gemessen wird am
 * Ergebnis — schreiben, `close()`, NEU oeffnen, zuruecklesen —, nicht
 * am Funktionszeiger. Genau diese Form verlangt P3-154 seit MF-880,
 * weil `test_capability_manifest.c:147` nur prueft, ob ein Zeiger
 * existiert.
 *
 * Er belegt **nicht**, dass die erzeugte CFI-Datei kanonisch ist: sie
 * wird von `cfi_compress_track()` gepackt und von
 * `cfi_decompress_track()` wieder gelesen, beide aus diesem Baum. Dafuer
 * braeuchte es eine fremde Hand, und im Korpus liegt kein CFI.
 *
 * ── Die Pruefdatei ──────────────────────────────────────────────────
 *
 * Von Hand gebaut wie in `test_cfi_gegen_samdisk.c`, damit kein
 * erzeugtes Abbild in den Beweis eingeht:
 *
 *   BPB: 512 Byte/Sektor, 9 Sektoren/Spur, 2 Koepfe, 18 gesamt
 *        -> 1 Zylinder, zwei Spuren zu 4608 Byte
 */
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "uft/formats/uft_cfi.h"
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

static int gruen = 0;
static int rot = 0;

static void pruefe(const char *name, int bedingung, const char *hinweis)
{
    if (bedingung) {
        printf("  [OK]   %s\n", name);
        gruen++;
    } else {
        printf("  [ROT]  %s%s%s\n", name, hinweis ? "  -- " : "",
               hinweis ? hinweis : "");
        rot++;
    }
}

extern const uft_format_plugin_t uft_format_plugin_cfi;

#define SEKTORGROESSE 512
#define SPURGROESSE   4608
#define FUELL_KOPF0   0xAA
#define FUELL_KOPF1   0x5A
#define NEUER_WERT    0x3C

typedef struct { uint8_t *p; size_t n, kap; } puffer_t;

static void schreib(puffer_t *b, const void *q, size_t n)
{
    if (b->n + n > b->kap) {
        b->kap = (b->n + n) * 2 + 64;
        b->p = (uint8_t *)realloc(b->p, b->kap);
    }
    memcpy(b->p + b->n, q, n);
    b->n += n;
}

static void le16(puffer_t *b, uint16_t v)
{
    uint8_t z[2] = { (uint8_t)(v & 0xFF), (uint8_t)(v >> 8) };
    schreib(b, z, 2);
}

static void teilblock_rle(puffer_t *b, uint16_t len, uint8_t fuell)
{
    uint8_t z[3] = { (uint8_t)(len & 0xFF),
                     (uint8_t)(((len >> 8) & 0x7F) | 0x80), fuell };
    schreib(b, z, 3);
}

static void teilblock_roh(puffer_t *b, const uint8_t *daten, uint16_t len)
{
    le16(b, (uint16_t)(len & 0x7FFF));
    schreib(b, daten, len);
}

static void baue_cfi(const char *pfad)
{
    uint8_t boot[SEKTORGROESSE];
    memset(boot, 0x00, sizeof(boot));
    boot[0] = 0xEB;
    boot[11] = 0x00; boot[12] = 0x02;   /* 512 Byte je Sektor */
    boot[19] = 0x12; boot[20] = 0x00;   /* 18 Sektoren gesamt */
    boot[24] = 0x09; boot[25] = 0x00;   /* 9 je Spur          */
    boot[26] = 0x02; boot[27] = 0x00;   /* 2 Koepfe           */

    puffer_t datei = { NULL, 0, 0 };

    puffer_t s0 = { NULL, 0, 0 };
    teilblock_roh(&s0, boot, SEKTORGROESSE);
    teilblock_rle(&s0, SPURGROESSE - SEKTORGROESSE, FUELL_KOPF0);
    le16(&datei, (uint16_t)s0.n);
    schreib(&datei, s0.p, s0.n);
    free(s0.p);

    puffer_t s1 = { NULL, 0, 0 };
    teilblock_rle(&s1, SPURGROESSE, FUELL_KOPF1);
    le16(&datei, (uint16_t)s1.n);
    schreib(&datei, s1.p, s1.n);
    free(s1.p);

    FILE *f = fopen(pfad, "wb");
    if (f) { fwrite(datei.p, 1, datei.n, f); fclose(f); }
    free(datei.p);
}

/* Spiegelt, was `uft_disk_open()` tut, bevor es das Plugin ruft
 * (`src/core/uft_core_stubs.c`, gemessen MF-931). Ohne diesen Schritt
 * hat das Plugin keinen Ort, an den es schreiben koennte. */
static void setze_pfad(uft_disk_t *d, const char *pfad)
{
    snprintf(d->path_buf, sizeof(d->path_buf), "%s", pfad);
    d->path = d->path_buf;
}

static void spur_frei(uft_track_t *tr)
{
    if (!tr) return;
    for (uint8_t s = 0; s < tr->sector_count; s++)
        free(tr->sectors[s].data);
    free(tr->sectors);
    memset(tr, 0, sizeof(*tr));
}

int main(void)
{
    printf("=== cfi_write_track erreicht die Datei (MF-1004) ===\n");

    char pfad[512];
    const char *tmp = getenv("TEMP");
    if (!tmp) tmp = getenv("TMPDIR");
    if (!tmp) tmp = ".";
    /* Kein `getpid()`: das braeuchte <unistd.h>, das es auf MinGW so
     * nicht gibt, und ein implizit deklariertes `getpid` ist auf
     * macOS-clang ein FEHLER, waehrend gcc nur warnt (Lektion aus der
     * Emulator-Welle). Der Dateiname muss nur eindeutig genug sein. */
    snprintf(pfad, sizeof(pfad), "%s/uft_mf1004_cfi_rundlauf.cfi", tmp);
    baue_cfi(pfad);

    pruefe("das Plugin hat ein write_track",
           uft_format_plugin_cfi.write_track != NULL,
           "ohne Zeiger sagt der Rest nichts");
    if (!uft_format_plugin_cfi.write_track) {
        remove(pfad);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    /* ── schreiben ─────────────────────────────────────────────────── */
    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    setze_pfad(&disk, pfad);

    uft_error_t rc = uft_format_plugin_cfi.open(&disk, pfad, false);
    pruefe("die Pruefdatei laesst sich oeffnen", rc == UFT_OK, NULL);
    if (rc != UFT_OK) {
        remove(pfad);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    uft_track_t t;
    memset(&t, 0, sizeof(t));
    rc = uft_format_plugin_cfi.read_track(&disk, 0, 1, &t);
    pruefe("Spur (0,1) laesst sich lesen", rc == UFT_OK && t.sector_count > 0,
           NULL);

    if (rc == UFT_OK && t.sector_count > 0 && t.sectors[0].data)
        memset(t.sectors[0].data, NEUER_WERT, t.sectors[0].data_size);

    uft_error_t wrc = uft_format_plugin_cfi.write_track(&disk, 0, 1, &t);
    char h[160];
    snprintf(h, sizeof(h),
             "write_track meldete %d -- vor MF-1004 stand hier "
             "UFT_ERROR_NOT_SUPPORTED, davor UFT_OK ohne Tat", (int)wrc);
    pruefe("write_track meldet Erfolg", wrc == UFT_OK, h);

    spur_frei(&t);
    uft_format_plugin_cfi.close(&disk);

    /* ── zuruecklesen: DAS ist die Messung ─────────────────────────── */
    uft_disk_t d2;
    memset(&d2, 0, sizeof(d2));
    setze_pfad(&d2, pfad);
    rc = uft_format_plugin_cfi.open(&d2, pfad, true);
    pruefe("die geschriebene Datei laesst sich neu oeffnen", rc == UFT_OK,
           "der Schreiber hat die Datei unbrauchbar gemacht");

    if (rc == UFT_OK) {
        uft_track_t t2;
        memset(&t2, 0, sizeof(t2));
        uft_error_t r2 = uft_format_plugin_cfi.read_track(&d2, 0, 1, &t2);
        int traegt = (r2 == UFT_OK && t2.sector_count > 0
                      && t2.sectors[0].data
                      && t2.sectors[0].data[0] == NEUER_WERT);
        snprintf(h, sizeof(h),
                 "Sektor (0,1,1) traegt 0x%02X statt 0x%02X -- die "
                 "Aenderung blieb im Speicher",
                 (r2 == UFT_OK && t2.sector_count > 0 && t2.sectors[0].data)
                     ? (unsigned)t2.sectors[0].data[0] : 0u,
                 (unsigned)NEUER_WERT);
        pruefe("die Aenderung steht in der DATEI", traegt, h);
        spur_frei(&t2);

        /* Gegenprobe (MF-931): Kopf 0 darf sich NICHT geaendert haben.
         * Lesen und Schreiben mit demselben falschen Index liefen sonst
         * rund und deckten einander. */
        uft_track_t t0;
        memset(&t0, 0, sizeof(t0));
        uft_error_t r0 = uft_format_plugin_cfi.read_track(&d2, 0, 0, &t0);
        int kopf0_heil = (r0 == UFT_OK && t0.sector_count > 1
                          && t0.sectors[1].data
                          && t0.sectors[1].data[0] == FUELL_KOPF0);
        pruefe("Kopf 0 ist unveraendert geblieben", kopf0_heil,
               "der Schreibindex trifft die falsche Spur");
        spur_frei(&t0);

        uft_format_plugin_cfi.close(&d2);
    }

    remove(pfad);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot == 0 ? 0 : 1;
}
