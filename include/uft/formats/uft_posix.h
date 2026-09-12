/**
 * @file uft_posix.h
 * @brief POSIX — eine rohe Sektordatei plus eine `.geom`-Nachbardatei
 *
 * ── Was das Format ist ──────────────────────────────────────────────
 *
 * Eine **flache Sektordatei ohne Kopf**. libdsks `drvposix.c` nennt sie
 * „the most basic of the drivers" (Z. 23-24) und definiert davon
 * **drei** Spielarten, die sich ausschliesslich in der Spurreihenfolge
 * unterscheiden (Z. 38-98):
 *
 *   | libdsk-Typ       | Anordnung        |
 *   |------------------|------------------|
 *   | `raw` / `rawalt` | `SIDES_ALT`      |
 *   | `rawoo`          | `SIDES_OUTOUT`   |
 *   | `rawob`          | `SIDES_OUTBACK`  |
 *
 * `posix_offset()` (Z. 232-254) schaltet dabei auf die Sidedness des
 * **Treibers**, und libdsk sagt den Unterschied zu `logical` selbst:
 *
 *     „Work out the offset based on the sidedness of the disk image
 *      (not the sidedness of the geometry)"
 *
 * Bei `logical` (MF-1032) kommt sie aus der **Geometrie**, hier aus dem
 * **Dateityp**. Die Rechnung ist dieselbe — sie steht deshalb nur
 * einmal im Baum, in `uft_logical_track_index()`, und ist dort gegen
 * libdsk abgenommen.
 *
 * ── Die `.geom`-Nachbardatei ist UFT-EIGEN ──────────────────────────
 *
 * **BERICHTIGT MF-1034.** Hier stand „Reference: libdsk drvposix.c
 * (LGPL-2.0-or-later; Fassung 1.5.12 geprueft)", und das trug nicht:
 * die Zeichenfolge `.geom` kommt im **ganzen** libdsk-Baum nicht vor
 * (gemessen ueber `lib/`, `include/`, `tools/`, `doc/`). libdsk loest
 * die Geometriefrage anders — der Aufrufer **nennt** sie
 * (`dsktrans -itype raw -format acorn640`).
 *
 * Die Sidecar-Datei ist damit **UFTs eigene Konvention** und wird auch
 * so geführt. Sie erfindet nichts über das Format; sie hält fest, was
 * der Benutzer weiß und die Datei nicht tragen kann — und sie ist
 * genau der Kanal, den **P3-337** für `logical` sucht.
 *
 * Zeilenformat (eine Zeile, Werte durch Leerzeichen):
 *
 *     <zylinder> <koepfe> <sektoren> <sektorgroesse> [erster] [anordnung]
 *
 * `erster` ist libdsks `dg_secbase` (Vorgabe 1), `anordnung` eines von
 * `alt` · `outout` · `outback` · `extsurface` (Vorgabe `alt`, damit
 * jede bisher geschriebene `.geom` unveraendert weitergilt).
 *
 * Referenz fuer das Byteverhalten: libdsk **1.5.12** (John Elliott,
 * **LGPL-2+**), `lib/drvposix.c` und `lib/dsklphys.c` — **nur
 * gelesen**, Kanal *Spec* nach MF-695; `dsktrans` zusaetzlich
 * **ausgefuehrt** zur Abnahme.
 */

#ifndef UFT_POSIX_H
#define UFT_POSIX_H

#include "uft/uft_format_common.h"
#include "uft/core/uft_disk_image_compat.h"
#include "uft/uft_error.h"
#include "uft/formats/uft_logical.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Endung der UFT-eigenen Geometrie-Nachbardatei */
#define POSIX_GEOM_EXTENSION    ".geom"
#define POSIX_GEOM_MAX_LINE     128

/**
 * @brief Geometrie eines POSIX-Abbilds.
 *
 * `sides` ist neu in MF-1034 und faellt ohne Angabe auf
 * `UFT_LOGI_SIDES_ALT` zurueck — libdsks `raw`/`rawalt`.
 */
typedef struct {
    uint16_t cylinders;
    uint8_t  heads;
    uint8_t  sectors;
    uint16_t sector_size;
    uint8_t  first_sector;      /* libdsks dg_secbase, ueblich 0 oder 1 */
    uft_encoding_t encoding;    /* FM oder MFM */
    uft_logical_sides_t sides;  /* MF-1034 */
} posix_geometry_t;

/**
 * @brief Leseoptionen.
 *
 * **MF-1034: `require_geom` ist jetzt standardmaessig `true`.** Vorher
 * war es `false`, und der Rueckfall 80x2x9x512 wurde zusammen mit
 * `cylinders = gesamtspuren / koepfe` auf **jede** Datei angewandt —
 * eine 174848 Byte grosse D64 wurde damit als 18x2x9x512 gelesen, die
 * restlichen 8960 Byte fielen weg, und alles davon still. Eine
 * Geometrie zu erfinden, die die Datei nicht bestaetigt, ist genau die
 * Klasse, die dieser Baum jagt.
 *
 * Wer den Rueckfall will, setzt `require_geom = false` **und** eine
 * Geometrie in `fallback`; sie wird nur angenommen, wenn sie die
 * Dateigroesse **restlos** erklaert.
 */
typedef struct {
    bool     require_geom;      /* Vorgabe: true (MF-1034) */
    posix_geometry_t fallback;  /* nur mit require_geom == false */
} posix_read_options_t;

/** @brief Ergebnis eines Lesevorgangs. */
typedef struct {
    bool success;
    uft_error_t error;
    const char *error_detail;

    bool geom_found;            /* lag eine `.geom` daneben? */
    posix_geometry_t geometry;  /* benutzte Geometrie */
    size_t image_size;
} posix_read_result_t;

/* ============================================================================
 * Geometrie
 * ========================================================================== */

/** @brief Vorgaben setzen (`require_geom = true`). */
void uft_posix_read_options_init(posix_read_options_t *opts);

/**
 * @brief Namen einer Anordnung, wie er in der `.geom` steht.
 * @return "alt" · "outout" · "outback" · "extsurface", nie NULL
 */
const char *uft_posix_sides_name(uft_logical_sides_t sides);

/**
 * @brief Anordnung aus ihrem Namen.
 * @return 1 bei Erfolg, 0 bei unbekanntem Namen (`*out` unberuehrt)
 */
int uft_posix_sides_from_name(const char *name, uft_logical_sides_t *out);

/**
 * @brief Die POSIX-Geometrie in die abgenommene Logical-Fassung giessen.
 *
 * Damit rechnet der Versatz durch `uft_logical_offset()`, also durch
 * Code, der in MF-1032 gegen libdsk abgenommen wurde — statt durch eine
 * zweite Kopie derselben vier Gesetze.
 */
void uft_posix_to_logical_geometry(const posix_geometry_t *in,
                                   uft_logical_geometry_t *out);

/** @brief `.geom` lesen. Sechster Wert (Anordnung) ist optional. */
uft_error_t uft_posix_read_geometry(const char *geom_path,
                                    posix_geometry_t *geometry);

/** @brief `.geom` schreiben — mit Anordnung, sobald sie nicht `alt` ist. */
uft_error_t uft_posix_write_geometry(const char *geom_path,
                                     const posix_geometry_t *geometry);

/* ============================================================================
 * Datei-Ein- und Ausgabe
 * ========================================================================== */

/**
 * @brief Erkennung — pfadgebunden, weil die Identitaet nebenan liegt.
 *
 * Gefuehrt in `scripts/audit_dead_probe.py`: die Plugin-Sonde sieht nur
 * den Inhalt und kann deshalb prinzipiell nicht zustimmen (MF-546).
 */
bool uft_posix_probe(const char *path, int *confidence);

uft_error_t uft_posix_read(const char *path,
                           uft_disk_image_t **out_disk,
                           const posix_read_options_t *opts,
                           posix_read_result_t *result);

/**
 * @brief Abbild **und** `.geom` schreiben, in der Anordnung von `geometry`.
 *
 * `geometry` darf NULL sein; dann wird die Geometrie des Abbilds
 * genommen und `alt` angesetzt — das Verhalten vor MF-1034.
 */
uft_error_t uft_posix_write(const uft_disk_image_t *disk,
                            const posix_geometry_t *geometry,
                            const char *path);

#ifdef __cplusplus
}
#endif

#endif /* UFT_POSIX_H */
