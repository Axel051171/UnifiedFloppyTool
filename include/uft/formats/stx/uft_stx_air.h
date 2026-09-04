/* SPDX-License-Identifier: GPL-3.0-only */
/**
 * @file uft_stx_air.h
 * @brief Schmale Tuer zum vollstaendigen STX-Leser (MF-854).
 *
 * ── Warum es diesen Header gibt ──────────────────────────────────────
 *
 * `src/formats/stx/uft_stx_air.c` ist ein erklaerter Port von AIR
 * (`pasti/PastiRead.cs`, Jean Louis-Guerin, GPL-3 — MF-698) und liest
 * STX vollstaendig: Fuzzy-Masken, Timing-Saetze, Spurbilder mit
 * Sync-Offset, Standardspuren, CRC. Er hatte bis MF-854 **keinen
 * Aufrufer** ausserhalb eines Tests, weil seine Typen
 * uebersetzungseinheitslokal waren — P3-58 nannte das „es gibt keine
 * Tuer", P3-93 die daraus folgende Eigentuemer-Entscheidung.
 *
 * Der Eigentuemer hat entschieden: verdrahten.
 *
 * ── Warum SCHMAL und nicht die ganze Struktur ────────────────────────
 *
 * `stx_air_disk_t` traegt `tracks[128][2]` mit je 32 Sektoren. Die
 * Struktur offenzulegen hiesse, jeden Aufrufer an ihr Feldlayout zu
 * binden — und dieser Baum hat dreimal erlebt, was passiert, wenn zwei
 * Stellen dasselbe Layout von Hand nachdeklarieren (MF-796: EDSK, 40
 * gegen 32 Byte je Sektor, jede Spur jeder Datei still leer).
 *
 * Deshalb: undurchsichtiges Handle, und je Sektor eine Sicht, die genau
 * das traegt, was ein Plugin braucht. Nach dem Muster von
 * `ipf_air_get_track_loss()`.
 *
 * ── Lizenz ───────────────────────────────────────────────────────────
 *
 * GPL-3.0-only, wie die Uebersetzungseinheit dahinter. Wer diesen
 * Header einbindet, zieht die GPL-3-Bindung in seinen Pfad. MF-698 hat
 * das fuer das verteilbare Gesamtwerk angenommen; mit MF-854 gilt es
 * auch fuer den REGISTRIERTEN Formatpfad.
 */
#ifndef UFT_FORMATS_STX_UFT_STX_AIR_H
#define UFT_FORMATS_STX_UFT_STX_AIR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Undurchsichtiges Handle auf eine geparste STX-Diskette. */
typedef struct stx_air_disk stx_air_handle_t;

/** Sicht auf EINEN Sektor — nur, was ein Plugin braucht. */
typedef struct {
    const uint8_t *data;        /**< Sektordaten, dem Handle gehoerend */
    uint32_t       size;        /**< Laenge in Byte                    */
    uint8_t        id_track;    /**< C aus dem Adressfeld              */
    uint8_t        id_side;     /**< H                                 */
    uint8_t        id_number;   /**< R — die Sektornummer              */
    uint8_t        id_size;     /**< N (Groessenkennung)               */
    bool           deleted;     /**< geloeschte Datenadressmarke       */
    bool           crc_error;   /**< CRC-Fehler                        */
    bool           rnf;         /**< Record Not Found: nur Adresse     */
    bool           fuzzy;       /**< traegt Fuzzy-Bits                 */
} uft_stx_sector_view_t;

/**
 * Parst @p size Byte STX aus @p data.
 * @return Handle oder NULL. Freigabe mit @ref uft_stx_air_close.
 */
stx_air_handle_t *uft_stx_air_open(const uint8_t *data, size_t size);

/** Gibt ein Handle aus @ref uft_stx_air_open frei. NULL ist erlaubt. */
void uft_stx_air_close(stx_air_handle_t *h);

/** Hoechste belegte Zylindernummer + 1 (0, wenn nichts gelesen wurde). */
int uft_stx_air_cylinders(const stx_air_handle_t *h);

/** Ist die Spur (@p cyl, @p head) im Abzug enthalten? */
bool uft_stx_air_track_present(const stx_air_handle_t *h, int cyl, int head);

/**
 * Wie viele Sektoren dieser Spur liegen WIRKLICH vor.
 *
 * Kann kleiner sein als @ref uft_stx_air_track_announced — siehe dort.
 */
int uft_stx_air_track_stored(const stx_air_handle_t *h, int cyl, int head);

/**
 * Wie viele Sektoren die DATEI fuer diese Spur ankuendigt.
 *
 * MF-854 / P3-58: der Leser fasst hoechstens 32 Sektordeskriptoren je
 * Spur. Das ist eine Grenze DIESER Umsetzung, nicht des Formats —
 * `sectorCount` ist ein `uint16`, und belegte Extremfaelle liegen weit
 * darueber („Sherman M4", 70 Sektoren je Spur; DrCoolZic, Atari Copy
 * Protection Rev 1.4, Klasse NOS).
 *
 * Weichen die beiden Zahlen ab, ist der Verlust benannt statt still.
 * Genau dafuer gibt es diese zweite Zusage.
 */
int uft_stx_air_track_announced(const stx_air_handle_t *h, int cyl, int head);

/**
 * Fuellt @p out mit dem @p idx-ten Sektor der Spur.
 * @return false, wenn es ihn nicht gibt.
 */
bool uft_stx_air_sector(const stx_air_handle_t *h, int cyl, int head,
                        int idx, uft_stx_sector_view_t *out);

#ifdef __cplusplus
}
#endif
#endif /* UFT_FORMATS_STX_UFT_STX_AIR_H */
