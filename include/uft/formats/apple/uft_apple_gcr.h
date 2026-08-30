/**
 * @file uft_apple_gcr.h
 * @brief Apple-II-GCR: 6-and-2 und 4-and-4 dekodieren (MF-715)
 *
 * Der Baum konnte bis MF-715 keinen einzigen Apple-Sektor aus einem
 * Bitstrom zurueckholen: `grep` ueber `src/` und `include/` fand
 * Apple-GCR nur als CRC-Tabellen-Treffer. WOZ, NIB und A2R waren damit
 * Container ohne Inhalt.
 *
 * ── Benannte Referenzen (EINFRIER-REGEL, MF-498) ────────────────────────
 *
 * 1. *Beneath Apple DOS* (Worth/Lechner), Kapitel 3: Aufbau von Adress-
 *    und Datenfeld, die 6-and-2-Umsetzungstabelle, die laufende
 *    XOR-Pruefsumme, die 4-and-4-Kodierung („odd-even").
 * 2. WOZ-2.0-Spezifikation (applesaucefdc.com) fuer den Bitstrom, aus
 *    dem hier gelesen wird.
 *
 * ── Und eine Messung, die VOR diesem Code stand ─────────────────────────
 *
 * Die 64 Diskettenbytes der Tabelle wurden nicht aus dem Gedaechtnis
 * uebernommen, sondern gegen die **Ausgabe** des registrierten Oracles
 * `to_woz2` geprueft (`docs/ORACLES.md`) — blackbox, ohne fremden
 * Quelltext zu lesen. Gemessen an Spur 0 eines von `to_woz2` erzeugten
 * WOZ 2.0:
 *
 *     Spur 0: 50 624 Bits, 6224 Nibbles
 *     Adressfelder (D5 AA 96): 16
 *     Datenfelder  (D5 AA AD): 16
 *     Datenbytes geprueft    : 5488  (16 * 343)
 *     nicht in der Tabelle   : 0
 *
 * Ein einziges fremdes Nibble haette die Tabelle widerlegt, bevor sie
 * Code wurde. Es gab keines.
 *
 * ── Was diese Einheit NICHT tut ─────────────────────────────────────────
 *
 * Sie ordnet nichts um. `uft_apple_gcr_scan_track()` liefert die
 * Sektoren so, wie sie **physisch** auf der Spur stehen, mit der
 * Sektornummer aus dem Adressfeld. Ob daraus DOS- oder
 * ProDOS-Reihenfolge wird, entscheidet der Aufrufer — die beiden
 * Ordnungen sind eine Eigenschaft der Datei, nicht der Spur (MF-463,
 * MF-714).
 */
#ifndef UFT_APPLE_GCR_H
#define UFT_APPLE_GCR_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Nutzbytes je Apple-II-Sektor. */
#define UFT_A2_SECTOR_SIZE      256u

/** Diskettenbytes eines Datenfelds: 86 Hilfs- + 256 Haupt- + 1 Pruefbyte. */
#define UFT_A2_DATA_NIBBLES     343u

/** Sektoren je Spur bei 16-Sektor-Formatierung. */
#define UFT_A2_SECTORS_16       16u

/** Ein aus der Spur gelesener Sektor. */
typedef struct {
    uint8_t volume;                         /**< Datentraegernummer   */
    uint8_t track;                          /**< Spur laut Adressfeld */
    uint8_t sector;                         /**< PHYSISCHE Sektornr.  */
    bool    addr_checksum_ok;               /**< Adressfeld-Pruefsumme */
    bool    data_checksum_ok;               /**< Datenfeld-Pruefsumme  */
    bool    has_data;                       /**< Datenfeld gefunden    */
    uint8_t data[UFT_A2_SECTOR_SIZE];       /**< Nutzbytes             */
} uft_a2_sector_t;

/**
 * @brief Ein 6-and-2-Datenfeld in 256 Nutzbytes zurueckwandeln.
 *
 * @param nib   343 Diskettenbytes (86 Hilfs-, 256 Haupt-, 1 Pruefbyte),
 *              wie sie hinter dem Vorspann `D5 AA AD` stehen.
 * @param out   Ziel, 256 Byte.
 * @return true, wenn alle 343 Bytes in der Umsetzungstabelle stehen UND
 *         die laufende XOR-Pruefsumme aufgeht. Bei false ist @p out
 *         unveraendert — ein halb dekodierter Sektor waere eine stille
 *         Veraenderung (DESIGN_PRINCIPLES).
 */
bool uft_apple_gcr_denibblize_6_2(const uint8_t nib[UFT_A2_DATA_NIBBLES],
                                  uint8_t out[UFT_A2_SECTOR_SIZE]);

/**
 * @brief Zwei 4-and-4-kodierte Diskettenbytes in ein Nutzbyte wandeln.
 *
 * Adressfelder speichern jedes Byte als zwei Diskettenbytes: das erste
 * traegt die ungeraden Bits (mit gesetzten geraden), das zweite die
 * geraden. `Beneath Apple DOS`, Kapitel 3.
 */
uint8_t uft_apple_gcr_decode_4_4(uint8_t hi, uint8_t lo);

/**
 * @brief Eine Spur nach Adress-/Datenfeld-Paaren absuchen.
 *
 * @param bits      Bitstrom der Spur, MSB zuerst gepackt (WOZ-Anordnung).
 * @param bit_count Zahl der gueltigen Bits.
 * @param out       Ziel-Feld.
 * @param max       Platz in @p out.
 * @return Zahl der gefundenen Sektoren, oder -1 bei ungueltigen Argumenten.
 *
 * Die Spur wird als **Ring** gelesen: ein Feld, das ueber das Ende
 * hinausragt, wird am Anfang fortgesetzt. Eine echte Spur hat keine
 * Naht, und ein Sektor, der zufaellig auf dem Umbruch liegt, waere sonst
 * verloren.
 */
int uft_apple_gcr_scan_track(const uint8_t *bits, uint32_t bit_count,
                             uft_a2_sector_t *out, size_t max);

#ifdef __cplusplus
}
#endif

#endif /* UFT_APPLE_GCR_H */
