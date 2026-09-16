/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_sector_order.h
 * @brief Sektoranordnung als eigene Achse, getrennt von der Geometrie.
 *
 * WARUM
 * -----
 * Dieselben Sektoren koennen in einer Abbilddatei in mindestens sechs
 * verschiedenen Reihenfolgen liegen. OmniFlop fuehrt sie in seiner
 * Merkmalsliste als getrennte "Formate" (Punkte 5, 6, 10, 15, 16, 17, 18) —
 * das sind aber keine Formate, sondern Anordnungen. Sein Treiber liefert
 * ausschliesslich EINE Reihenfolge und schiebt die Umrechnung ausdruecklich
 * zum Aufrufer (Handbuch 5.5.1: "No attempt is made to change the scheme
 * depending on the format ... your software must translate your order into a
 * file offset").
 *
 * UFT vermischt Anordnung und Geometrie heute in jedem Plugin einzeln. Das
 * kostet nicht nur Zeilen, es hat schon einen Fehler erzeugt:
 *
 *   src/formats/atari/uft_xfd_parser_v2.c, XFD_DENSITY_QD
 *     - XFD_TRACKS_QD = 80 statt 40
 *     - keine Spiegelung der Rueckseite
 *   Belegt gegen Erwin Reuss, "Die Formate der XF551" (Compy-Shop-Magazin
 *   4/88): Sektor 721 liegt auf Track 39, Sektor 1440 auf Track 0.
 *   Und gegen das Orakel im eigenen Baum, src/a8rawconv/diskxfd.cpp:67-73
 *   (tracks = 40, sides = 2) und :92-116 (Rueckwaertslauf fuer Seite 1).
 *
 * Mit dieser Achse ist der XF551-Fall eine Datenzeile
 * (UFT_ORDER_SERPENTINE) statt einer fehlenden Rechnung.
 *
 * SPEC_STATUS
 *   Quelle A: OmniFlop User Guide v3.2d, Abschnitt 5.5.1 — belegt, dass der
 *             Treiber CHS-verschachtelt liefert und die Anordnung Sache des
 *             Aufrufers ist. (Dokument nicht mitgeliefert: Reproduktions-
 *             verbot. Zitiert und belegt, nicht kopiert.)
 *   Quelle B: Erwin Reuss, Compy-Shop-Magazin 4/88 — XF551-Serpentine.
 *   Quelle C: a8rawconv 0.95, diskxfd.cpp (GPL-2-or-later, Orakel im Baum).
 */

#ifndef UFT_SECTOR_ORDER_H
#define UFT_SECTOR_ORDER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    /** C0H0S1..n, C0H1S1..n, C1H0..., wie IBM-PC-.img und OmniFlops Treiber. */
    UFT_ORDER_CHS_INTERLEAVED = 0,
    /** Zylinder aussen, Kopf innen — identisch zu CHS_INTERLEAVED, eigener
     *  Name, weil OmniFlop ".chs" getrennt fuehrt. Alias, kein zweiter Fall. */
    UFT_ORDER_CHS,
    /** Kopf aussen, Zylinder innen: H0C0S1..n, H0C1S1..n, ... dann H1C0.
     *  Identisch mit "Kopf 0 ganz, dann Kopf 1 ganz" (OmniFlop 17 und 18,
     *  die dort beide ".hcs" heissen — ein Fehler in dessen Dokumentation). */
    UFT_ORDER_HCS,
    /** Kopf 0 aufwaerts, Kopf 1 ABWAERTS. OmniFlop nennt es FEAT
     *  ("Head 0 out then Head 1 back"). Das ist die XF551-QD-Serpentine. */
    UFT_ORDER_SERPENTINE,
    /** Einseitiges Format auf zweiseitigem Medium, Spuren verschachtelt
     *  abgelegt (OmniFlop ".dsd"). Nur die gewaehlte Seite traegt Daten. */
    UFT_ORDER_INTERLEAVED_DS,
    /** Einseitig, flach (OmniFlop ".ssd"). */
    UFT_ORDER_SINGLE_SIDED
} uft_sector_order_t;

typedef struct {
    uint16_t cylinders;
    uint8_t  heads;
    uint8_t  sectors;       /**< je Spur */
    uint16_t sector_size;
    /** Bei INTERLEAVED_DS: welche physische Seite die Daten traegt. */
    uint8_t  active_head;
} uft_order_geometry_t;

typedef struct { uint16_t cyl; uint8_t head; uint8_t sector; } uft_chs_t;

/** Gesamtzahl der Sektoren, die die Anordnung adressiert. */
uint32_t uft_order_sector_count(uft_sector_order_t order,
                                const uft_order_geometry_t *g);

/**
 * Bildet eine 0-basierte Dateiposition (Sektorindex in der Abbilddatei) auf
 * die physische CHS-Lage ab. @p out->sector ist 1-basiert, wie auf dem Medium.
 * @return false bei Ueberlauf oder unplausibler Geometrie.
 */
bool uft_order_index_to_chs(uft_sector_order_t order,
                            const uft_order_geometry_t *g,
                            uint32_t index, uft_chs_t *out);

/**
 * Umkehrung: physische Lage auf Dateiposition. Muss zu
 * uft_order_index_to_chs() invers sein — der Test prueft das erschoepfend
 * fuer alle Anordnungen und Geometrien.
 * @return false wenn die Lage in dieser Anordnung nicht vorkommt.
 */
bool uft_order_chs_to_index(uft_sector_order_t order,
                            const uft_order_geometry_t *g,
                            const uft_chs_t *chs, uint32_t *out_index);

/** Klartextname, fuer Protokoll und Oberflaeche. */
const char *uft_order_name(uft_sector_order_t order);

#ifdef __cplusplus
}
#endif

#endif /* UFT_SECTOR_ORDER_H */
