/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_os_volume.h
 * @brief HAL-Erweiterung: logischer Sektortransport ueber das Wirtssystem.
 *
 * WARUM DAS FEHLT
 * ---------------
 * UFTs HAL ist um Flux-Controller gebaut. `can_read_sector` ist in
 * uft_hal_profiles.c bei vier Profilen gesetzt, aber alle vier sind
 * Spezialhardware (Greaseweazle-FDC-Modus, FC5025, XUM1541, USB-Floppy).
 * Es gibt KEINEN Transport, der eine gewoehnliche Diskette im gewoehnlichen
 * PC-Laufwerk ueber das Betriebssystem sektorweise liest.
 *
 * Fuer die ganze Klasse der Sampler-Formate ist das aber der richtige und
 * billigste Weg: Roland S-50/S-330/S-550/W-30, Akai S900/S1000, Korg DSS-1,
 * Emu und die meisten PC-kompatiblen Fremdformate liegen als gewoehnliches
 * MFM auf 512-, 1024- oder 2048-Byte-Sektoren. Flux braucht man dort erst,
 * wenn die Diskette beschaedigt oder kopiergeschuetzt ist.
 *
 * VORLAGE
 * -------
 * SDISK for Windows v1.7 (MIT, (c) 2011 Miroslav Svetlik) faehrt genau das,
 * mit zwei Transportwegen, die zur Laufzeit gewaehlt werden — Beleg aus der
 * statischen Analyse von SDISKW.EXE:
 *
 *   Modus 1 (Windows 9x), gesetzt bei VA 0x40341f:
 *     DeviceIoControl(hVWIN32, 4  (VWIN32_DIOC_DOS_INT13),
 *                     &DIOC_REGISTERS (28 Byte), 28, ...)
 *     mit reg_EAX = 0x0201 (INT 13h AH=02 Sektoren lesen, AL=1)
 *          reg_EAX = 0x0301 (INT 13h AH=03 Sektoren schreiben, AL=1)
 *     Sperre vorher ueber Code 1 (VWIN32_DIOC_DOS_IOCTL) mit
 *     reg_EAX = 0x440D, reg_ECX = 0x084B (Lock Physical Volume) —
 *     dazu passt die Zeichenkette "Error Locking Diskette."
 *
 *   Modus 2 (Windows NT), gesetzt bei VA 0x403435:
 *     CreateFileA("\\\\.\\A:", GENERIC_READ|GENERIC_WRITE,
 *                 FILE_SHARE_READ|WRITE, NULL, OPEN_EXISTING, 0, NULL)
 *     SetFilePointer(h, lba << 9, ...)    -- 512-Byte-Sektoren
 *     ReadFile/WriteFile(h, buf, 0x200, ...)
 *     Geometrie ueber DeviceIoControl mit 24-Byte-Ausgabepuffer
 *     (= sizeof(DISK_GEOMETRY)); der Steuercode liegt in einem Global
 *     (VA 0x4074da), wird also zur Laufzeit gewaehlt.
 *
 * Modus 1 ist historisch und wird hier NICHT nachgebaut — VWIN32.VXD gibt es
 * auf keinem unterstuetzten System mehr. Er ist dokumentiert, weil er
 * erklaert, warum das Orakel zwei Wege hat, und weil dieselbe Struktur bei
 * anderen Werkzeugen der Epoche wiederkehrt.
 *
 * WAS HIER BESSER GEMACHT WIRD ALS IM ORAKEL
 * ------------------------------------------
 *  1. Geometrie ist Parameter, nicht Konstante. Das Orakel rechnet
 *     `cyl = lba/18; head = (lba%18)>=9` mit fest verdrahteten 18 und 9
 *     (VA 0x402e44) — damit ist es auf 9-Sektor-DD festgelegt und kann
 *     Akai (5x1024) oder Korg gar nicht.
 *  2. Kein stiller Teilerfolg. Das Orakel liest das ganze Abbild in EINEM
 *     ReadFile ueber 0xB4000 Byte; faellt ein Sektor aus, ist das Ergebnis
 *     ein Puffer mit Loch, ueber das nichts protokolliert wird. Hier bekommt
 *     jeder Sektor einen Status, und der Aufrufer sieht genau, welche fehlen.
 *  3. Wiederholungen pro Sektor, einstellbar, mit Rueckmeldung wie viele
 *     Versuche es gebraucht hat — die Grundlage fuer eine Medienqualitaets-
 *     aussage.
 */

#ifndef UFT_OS_VOLUME_H
#define UFT_OS_VOLUME_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ───────────────────────────── Geometrie ────────────────────────────────── */

typedef struct {
    uint16_t cylinders;      /**< z. B. 80  */
    uint8_t  heads;          /**< z. B. 2   */
    uint8_t  sectors;        /**< je Spur, z. B. 9 */
    uint16_t sector_size;    /**< 512, 1024, 2048 */
    bool     double_step;    /**< 48-TPI-Medium im 96-TPI-Laufwerk */
} uft_osvol_geometry_t;

/** Gesamtzahl logischer Sektoren. */
static inline uint32_t uft_osvol_total_sectors(const uft_osvol_geometry_t *g) {
    return (uint32_t)g->cylinders * g->heads * g->sectors;
}

/** Abbildgroesse in Byte. */
static inline uint64_t uft_osvol_image_size(const uft_osvol_geometry_t *g) {
    return (uint64_t)uft_osvol_total_sectors(g) * g->sector_size;
}

/** Zylinder/Kopf/Sektor zu einem logischen Sektor — parametrisiert. */
typedef struct { uint16_t cyl; uint8_t head; uint8_t sector; } uft_osvol_chs_t;

/**
 * Bildet einen 0-basierten logischen Sektor auf CHS ab.
 * @param sector_base 1, wenn der Controller 1-basierte Sektornummern erwartet
 *                    (INT 13h und die meisten FDC), sonst 0.
 * @return false bei Ueberlauf.
 *
 * Gegenstueck zu VA 0x402e44 im Orakel, dort mit fest verdrahtetem 18/9 und
 * ohne Ueberlaufpruefung.
 */
bool uft_osvol_lba_to_chs(const uft_osvol_geometry_t *g, uint32_t lba,
                          unsigned sector_base, uft_osvol_chs_t *out);

/* ───────────────────────────── Sektorstatus ─────────────────────────────── */

typedef enum {
    UFT_OSVOL_SEC_UNREAD = 0,   /**< nicht versucht                         */
    UFT_OSVOL_SEC_OK,           /**< beim ersten Versuch gelesen            */
    UFT_OSVOL_SEC_OK_RETRY,     /**< erst nach Wiederholung gelesen         */
    UFT_OSVOL_SEC_BAD,          /**< alle Versuche fehlgeschlagen           */
    UFT_OSVOL_SEC_SKIPPED       /**< Aufrufer hat abgebrochen               */
} uft_osvol_sec_state_t;

typedef struct {
    uft_osvol_sec_state_t state;
    uint8_t  attempts;          /**< tatsaechliche Versuche                 */
    int32_t  os_error;          /**< errno bzw. GetLastError beim Fehlschlag*/
} uft_osvol_sec_status_t;

/* ───────────────────────────── Sitzung ─────────────────────────────────── */

typedef struct uft_osvol uft_osvol_t;

typedef struct {
    const char *path;            /**< "/dev/fd0" oder "\\\\.\\A:"            */
    uft_osvol_geometry_t geo;
    uint8_t     retries;         /**< Versuche je Sektor, mindestens 1       */
    bool        write_enable;    /**< false = Geraet nur lesend oeffnen      */
    bool        lock_volume;     /**< Windows: FSCTL_LOCK_VOLUME vorher      */
    bool        no_buffering;    /**< Windows: FILE_FLAG_NO_BUFFERING        */
} uft_osvol_open_params_t;

/** Fortschrittsrueckmeldung; Rueckgabe false bricht ab. */
typedef bool (*uft_osvol_progress_fn)(uint32_t lba, uint32_t total,
                                      const uft_osvol_sec_status_t *st,
                                      void *user);

uft_osvol_t *uft_osvol_open(const uft_osvol_open_params_t *p, int32_t *os_error);
void         uft_osvol_close(uft_osvol_t *v);

/**
 * Liest @p count Sektoren ab @p lba.
 *
 * @param status  Feld mit mindestens @p count Eintraegen. Darf NICHT NULL
 *                sein — der Aufrufer MUSS erfahren, was fehlt. Genau hier
 *                liegt der Unterschied zum Orakel.
 * @return Zahl der erfolgreich gelesenen Sektoren. Ein Rueckgabewert kleiner
 *         als @p count ist KEIN Fehler, sondern ein Teilergebnis mit
 *         vollstaendigem Befund im Statusfeld.
 */
uint32_t uft_osvol_read(uft_osvol_t *v, uint32_t lba, uint32_t count,
                        uint8_t *buf, uft_osvol_sec_status_t *status,
                        uft_osvol_progress_fn progress, void *user);

uint32_t uft_osvol_write(uft_osvol_t *v, uint32_t lba, uint32_t count,
                         const uint8_t *buf, uft_osvol_sec_status_t *status,
                         uft_osvol_progress_fn progress, void *user);

/** Geometrie vom Betriebssystem erfragen, wenn moeglich. */
bool uft_osvol_query_geometry(uft_osvol_t *v, uft_osvol_geometry_t *out);

/** Zusammenfassung des Statusfeldes, mehrzeilig. */
size_t uft_osvol_status_summary(const uft_osvol_sec_status_t *status,
                                uint32_t count, char *buf, size_t buflen);

#ifdef __cplusplus
}
#endif

#endif /* UFT_OS_VOLUME_H */
