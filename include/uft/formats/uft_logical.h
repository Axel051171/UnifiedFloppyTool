/**
 * @file uft_logical.h
 * @brief „Logical" — ein flaches Abbild in LOGISCHER Sektorreihenfolge
 *
 * ── Was dieses Format ist, und was es NICHT ist ─────────────────────
 *
 * Es gibt **keinen Dateikopf und keine Kennung**. libdsks
 * `logical_open()` (`lib/drvlogi.c` Z. 67-85) tut nichts als `fopen()`
 * und die Groesse merken; die Datei beginnt mit dem ersten Sektor.
 *
 * Der Unterschied zu einem gewoehnlichen flachen Abbild steckt
 * ausschliesslich in der **Reihenfolge der Spuren**. Der Kopfkommentar
 * des Treibers sagt es in einem Satz (Z. 24-26):
 *
 *     „a flat file, like drvposix, but with the sides laid out in the
 *      order specified by the disk geometry"
 *
 * Diese Reihenfolge ist `dg_sidedness`, und es gibt **vier Gesetze**
 * (`lib/dsklphys.c` `dg_pt2lt()` Z. 91-118):
 *
 *   | Gesetz          | logische Spur                       |
 *   |-----------------|-------------------------------------|
 *   | `SIDES_ALT`     | `zyl * koepfe + kopf`               |
 *   | `SIDES_OUTOUT`  | `kopf * zylinder + zyl`             |
 *   | `SIDES_OUTBACK` | Kopf 0: `zyl`; Kopf 1: `2*zylinder - 1 - zyl` |
 *   | `SIDES_EXTSURFACE` | rechnet wie `SIDES_ALT`          |
 *
 * Darin liegt der Sektor dann bei
 * `(spur * sektoren + (sektor - erster_sektor)) * sektorgroesse`
 * (`dg_ps2ls()` Z. 28-48, `logical_read()` Z. 124-147).
 *
 * **Dieselben vier Gesetze hat dieser Baum zweimal einzeln
 * wiederentdeckt:** `v9t9` brauchte OUTBACK (MF-1027 — „auf Seite 1
 * laeuft die Spurzahl rueckwaerts") und `nanowasp` OUTOUT (MF-1030 —
 * kopf-dur). Hier stehen sie an einer Stelle und sind gegen die
 * Referenz abgenommen.
 *
 * ── Warum es keine Sonde gibt, und das gemessen ist ─────────────────
 *
 * Aus dem Inhalt laesst sich das Gesetz nicht ablesen — die Datei sagt
 * nichts ueber sich. Aus der **Groesse** auch nicht, und das ist keine
 * Vermutung: in libdsks **eigener** Geometrietafel (`lib/dsksgeom.c`)
 * teilt **jede** der acht nicht-ALT-Geometrien ihre Dateigroesse mit
 * mindestens einer ALT-Geometrie —
 *
 *   ibm720/pcw720 737280 · ibm1200/pcw1200 1228800 ·
 *   ibm1440/pcw1440 1474560 · acorn160/ibm160 163840 ·
 *   acorn320/ibm320/pcpm320 327680 · acorn640/trdos640/scp640 655360 ·
 *   mgt800/acorn800/pcw800/ampro800/scp800 819200 · pcpm320/ibm320 327680
 *
 * — acht von acht. Die Groesse kann die Anordnung also in **keinem**
 * Fall entscheiden. libdsk loest das, indem der Aufrufer die Geometrie
 * **nennt** (`dsktrans -itype logical -format acorn640`); UFTs
 * Plugin-Schnittstelle hat dafuer keinen Kanal, und deshalb sagt
 * `logical_open()` ehrlich ab statt zu raten. Dieselbe Lage wie bei
 * `posix` (MF-546), dort steckt die Identitaet in einer Nachbardatei.
 *
 * Die gepruefte Leseseite ist `uft_logical_read_mem()` — sie nimmt die
 * Geometrie als Argument, weil das Format sie nicht mitbringt.
 *
 * Referenz: libdsk (John Elliott, **LGPL-2+**), `lib/drvlogi.c`,
 * `lib/dsklphys.c`, `lib/dsksgeom.c` — **nur gelesen**, Kanal *Spec*
 * nach MF-695; zusaetzlich **ausgefuehrt** (`dsktrans`) zur Abnahme.
 */

#ifndef UFT_LOGICAL_H
#define UFT_LOGICAL_H

#include "uft/uft_format_common.h"
#include "uft/core/uft_disk_image_compat.h"
#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Die vier Anordnungsgesetze aus libdsks `dg_pt2lt()`.
 *
 * Die Zahlenwerte sind UFT-eigen; libdsks `dsk_sides_t` wird nicht
 * uebernommen, nur sein Verhalten.
 */
typedef enum {
    UFT_LOGI_SIDES_ALT        = 0, /* dsklphys.c:106 */
    UFT_LOGI_SIDES_OUTOUT     = 1, /* dsklphys.c:111 */
    UFT_LOGI_SIDES_OUTBACK    = 2, /* dsklphys.c:107-110 */
    UFT_LOGI_SIDES_EXTSURFACE = 3  /* dsklphys.c:105 — wie ALT */
} uft_logical_sides_t;

/** Obergrenzen (MF-543): dieselben wie in jedem anderen Einstieg. */
#define UFT_LOGI_MAX_CYLINDERS   256
#define UFT_LOGI_MAX_HEADS         4
#define UFT_LOGI_MAX_SECTORS      64
#define UFT_LOGI_MAX_SECTOR_SIZE 1024

/**
 * @brief Die Geometrie, die das Format NICHT mitbringt.
 *
 * `first_sector` ist libdsks `dg_secbase`. Er ist **0** bei den
 * Acorn-Formaten (`dsksgeom.c` Z. 53-55) und **1** bei den PC-/PCW-
 * Formaten; ein stilles Heben von 0 auf 1 verschiebt jede Sektornummer.
 */
typedef struct {
    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors;
    uint16_t sector_size;
    uint8_t  first_sector;
    uft_logical_sides_t sides;
    uft_encoding_t encoding;
} uft_logical_geometry_t;

/** Ergebnis eines Lesevorgangs. */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;

    uint16_t cylinders;
    uint16_t heads;
    uint16_t sectors;
    uint16_t sector_size;
    size_t   image_size;
} logical_read_result_t;

/**
 * @brief Geometrie auf Plausibilitaet und Schranken pruefen.
 * @return 1 wenn brauchbar, sonst 0
 */
int uft_logical_geometry_ok(const uft_logical_geometry_t *g);

/** @brief Erwartete Dateigroesse dieser Geometrie, 0 bei Unsinn. */
size_t uft_logical_image_size(const uft_logical_geometry_t *g);

/**
 * @brief Logische Spurnummer aus Zylinder und Kopf — die vier Gesetze.
 *
 * libdsk `dg_pt2lt()` (`lib/dsklphys.c` Z. 91-118).
 *
 * @return 0..(zylinder*koepfe - 1), oder -1 bei unmoeglichen Koordinaten
 */
long uft_logical_track_index(int cyl, int head,
                             const uft_logical_geometry_t *g);

/**
 * @brief Byteversatz eines Sektors.
 *
 * `dg_ps2ls()` (Z. 28-48) mal `dg_secsize` (`logical_read()` Z. 137-139):
 * `(spur * sektoren + (sektor - first_sector)) * sektorgroesse`.
 *
 * `sector` ist die **auf der Diskette stehende** Nummer, also ab
 * `first_sector` zu zaehlen — nicht ein 0-basierter Index.
 *
 * @return Versatz, oder -1 bei unmoeglichen Koordinaten
 */
long uft_logical_offset(int cyl, int head, int sector,
                        const uft_logical_geometry_t *g);

/**
 * @brief Abbild aus dem Speicher lesen — die Geometrie kommt von aussen.
 *
 * Es gibt keinen Kopf; `data` beginnt mit dem ersten Sektor.
 */
uft_error_t uft_logical_read_mem(const uint8_t *data, size_t size,
                                 const uft_logical_geometry_t *g,
                                 uft_disk_image_t **out_disk,
                                 logical_read_result_t *result);

/** @brief Wie `uft_logical_read_mem()`, aber von einem Pfad. */
uft_error_t uft_logical_read(const char *path,
                             const uft_logical_geometry_t *g,
                             uft_disk_image_t **out_disk,
                             logical_read_result_t *result);

/**
 * @brief Abbild schreiben — ohne Kopf, in der Anordnung von `g`.
 *
 * Ohne Aufrufer (P3-204, MF-930). Vor MF-1032 schrieb diese Funktion
 * einen **erfundenen 32-Byte-Kopf** in jede Datei; keine fremde
 * Umsetzung haette sie lesen koennen.
 */
uft_error_t uft_logical_write(const uft_disk_image_t *disk,
                              const uft_logical_geometry_t *g,
                              const char *path);

#ifdef __cplusplus
}
#endif

#endif /* UFT_LOGICAL_H */
