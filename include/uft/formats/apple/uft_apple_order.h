/**
 * @file uft_apple_order.h
 * @brief DO gegen PO: die eine Probe, die im Puffer erreichbar ist (MF-724)
 *
 * ── Das Problem, und was es NICHT ist ───────────────────────────────────
 *
 * `.do` und `.po` sind beide **143 360 Byte** gross und enthalten
 * dieselbe Diskette; sie unterscheiden sich **nur in der Reihenfolge der
 * Sektoren 1..14**. Das ist kein Erkennungs-, sondern ein
 * **Mehrdeutigkeitsproblem**: die Groesse kann es nicht entscheiden, und
 * beide Sonden nahmen darum bis MF-724 jede Datei dieser Groesse an —
 * `do` mit 60, `po` mit 55. `do` gewann **immer**, an einer Konstanten.
 *
 * ── Was NICHT hilft: die VTOC ───────────────────────────────────────────
 *
 * Der naheliegende Gedanke (MF-713) war, die DOS-3.3-VTOC auf Spur 17
 * Sektor 0 zu pruefen. Er ist **falsch**, und der Baum sagte es bereits
 * (MF-463): Sektor 0 und Sektor 15 liegen in **beiden** Ordnungen an
 * derselben Stelle. Die VTOC steht damit bei DO und PO auf demselben
 * Dateiversatz — sie sagt „DOS-3.3-Diskette", nicht „DOS-Reihenfolge".
 * Nachgemessen an den Interleave-Tabellen von `a8rawconv`
 * (`diska2.cpp:3-9`), berichtigt in MF-714.
 *
 * ── Was hilft: das ProDOS-Datentraegerverzeichnis ───────────────────────
 *
 * ProDOS-Block 2 ist der Kopf des Datentraegerverzeichnisses. In einer
 * `.po` liegt Block N schlicht auf Versatz `N * 512` — Block 2 also auf
 * **0x400**, mitten im Sondenpuffer (`UFT_PROBE_BUFFER_SIZE` = 65536).
 *
 * In einer `.do` steht dort etwas anderes: ProDOS-Block 2 sind die
 * ProDOS-logischen Sektoren 4 und 5, physisch 8 und 10; in der
 * DOS-Anordnung liegen die auf den Versaetzen 0xB00 und 0xA00. Auf 0x400
 * steht in einer `.do` der DOS-logische Sektor 4.
 *
 * **Ein gueltiger Verzeichniskopf auf 0x400 ist damit ein positiver
 * Beleg fuer ProDOS-Reihenfolge** — die eine Richtung, die im Puffer
 * ueberhaupt entscheidbar ist.
 *
 * ── Die drei Bytes, zweifach belegt ─────────────────────────────────────
 *
 *     0x400  Rueckzeiger    == 0x0000  (kein Vorgaengerblock)
 *     0x402  Vorzeiger      == 0x0003  (Block 3 folgt)
 *     0x404  Speichertyp    oberes Nibble == 0xF
 *
 * QUELLE 1: die ProDOS-8-Beschreibung des Verzeichniskopfs.
 * QUELLE 2, unabhaengig: `mamedev/mame@c0d3677674`
 * `src/lib/formats/fs_prodos.cpp:269-271` (Dateikopf
 * `license:BSD-3-Clause`) schreibt genau diese drei Werte. Beschafft und
 * lokal nachgemessen in MF-720; bis dahin war der Weg ausdruecklich
 * gesperrt, weil **eine** Quelle nicht genuegt (MF-714).
 *
 * ── Und wenn die Probe NICHT anschlaegt ─────────────────────────────────
 *
 * Dann bleibt es mehrdeutig, und das wird **gesagt statt geraten**: beide
 * Sonden melden dieselbe Konfidenz, `uft_probe_ranking.tied` wird 2, und
 * `uft_smart_open()` gibt das als `equally_ranked` weiter (samt Warnung,
 * `uft_smart_open.c:430`). Der Kopf von `uft_probe_ranking` sagt selbst,
 * was das heisst: „der Gewinner steht durch Registrierungsreihenfolge
 * fest, nicht durch Evidenz."
 *
 * Ein Abbild, dessen Sortierung nicht entscheidbar ist, ist eine
 * **ehrliche Aussage**. Eine stillschweigend gewaehlte Sortierung waere
 * eine stille Falschaussage ueber **jede Datei darin**.
 *
 * Ausdruecklich NICHT vorgesehen: dass ein Leser den Benutzer fragt. Ein
 * Leser, der fragt, haengt in CI. Er reicht `tied` weiter; die
 * Oberflaeche fragt, ein Skript bricht ab.
 */
#ifndef UFT_APPLE_ORDER_H
#define UFT_APPLE_ORDER_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/** Dateigroesse einer 35x16x256-Apple-II-Diskette, in beiden Ordnungen. */
#define UFT_A2_140K_SIZE        143360u

/** Versatz des ProDOS-Datentraegerverzeichnisses in ProDOS-Anordnung. */
#define UFT_A2_PRODOS_VOLDIR    0x400u

/** Konfidenz, wenn der Verzeichniskopf gefunden wurde (Evidenz). */
#define UFT_A2_CONF_EVIDENZ     90

/** Konfidenz, wenn die Ordnung NICHT entscheidbar ist. Beide Sonden
 *  melden denselben Wert, damit `tied` anschlaegt statt einer Konstanten
 *  zu folgen. */
#define UFT_A2_CONF_UNKLAR      55

/** Konfidenz der jeweils widerlegten Lesart. Nicht 0: die Datei bleibt
 *  ein Kandidat (eine ProDOS-geordnete Diskette KANN ein DOS-Dateisystem
 *  tragen) — sie ist nur die schlechtere Erklaerung. */
#define UFT_A2_CONF_WIDERLEGT   20

/**
 * @brief Traegt der Puffer auf 0x400 einen ProDOS-Verzeichniskopf?
 *
 * @param d     Sondenpuffer.
 * @param size  Wie viel davon lesbar ist.
 * @return true nur, wenn alle drei Felder passen. Bei zu kleinem Puffer
 *         false — „nicht gesehen" ist nicht „nicht da", und der Aufrufer
 *         behandelt beides als *unentschieden*, nicht als Widerlegung.
 */
static inline bool uft_a2_has_prodos_voldir(const uint8_t *d, size_t size)
{
    if (!d || size < UFT_A2_PRODOS_VOLDIR + 5u) return false;
    const uint8_t *b = d + UFT_A2_PRODOS_VOLDIR;
    if (b[0] != 0x00 || b[1] != 0x00) return false;     /* Rueckzeiger */
    if (b[2] != 0x03 || b[3] != 0x00) return false;     /* Vorzeiger   */
    return (uint8_t)(b[4] & 0xF0u) == 0xF0u;            /* Speichertyp */
}

#endif /* UFT_APPLE_ORDER_H */
