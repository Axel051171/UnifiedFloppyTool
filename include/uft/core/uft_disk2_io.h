/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_disk2_io.h
 * @brief UFTD — der Behaelter, der ALLES traegt.
 *
 * ── WARUM ─────────────────────────────────────────────────────────────────
 *
 * `uft_d2_check_loss()` sagt fuer jedes Zielformat, was verloren geht. Fuer
 * das EIGENE gab es keine Antwort, weil es keines gab: das Zentrum lebte
 * nur im Speicher, und jede Sicherung war eine verlustbehaftete Wandlung.
 *
 * UFTD ist das Format, in dem ein Abzug vollstaendig ist: vier Schichten,
 * Herkunft je Objekt, Zuversicht je Bit, Stimmen je Bit, Befunde,
 * Metadaten, Ableitungsregister. Nichts davon hat SCP, IPF oder WOZ.
 *
 * ── AUFBAU ────────────────────────────────────────────────────────────────
 *
 *   Kopf     "UFTD" + Version(u16) + Flags(u16) + Gesamtlaenge(u64)
 *   Bloecke  Kennung(4) + Laenge(u32) + Inhalt + CRC32(u32)
 *
 * Bloecke, in dieser Reihenfolge, alle little-endian:
 *   DERV  Ableitungsregister            META  Metadaten
 *   TRAK  eine Spur, mit Unterbloecken:   FLUX (je Umdrehung), BITS, SECT
 *   FSYS  ein Dateisystem mit Eintraegen
 *   DIAG  Befunde samt der Zahl derer, die gar nicht erst gespeichert wurden
 *   END   Ende, Gesamt-CRC32 ueber alles davor
 *
 * ── DREI EIGENSCHAFTEN, DIE NICHT VERHANDELBAR SIND ──────────────────────
 *
 *   1. Unbekannte Bloecke werden UEBERSPRUNGEN und als Befund vermerkt.
 *      Eine neue Fassung kann Bloecke hinzufuegen; eine alte liest die
 *      Datei trotzdem und sagt, was sie nicht kennt.
 *   2. Jeder Block traegt seine CRC. Ein umgekipptes Bit wird an der
 *      Stelle gefunden, nicht als "Datei kaputt".
 *   3. Laden ist NIE stiller als Speichern: was beim Laden fehlt oder
 *      nicht stimmt, steht danach in der Befundliste des Modells.
 *
 * ── WAS NICHT GESPEICHERT WIRD ────────────────────────────────────────────
 *
 * Der Merkmalcache. Er wird beim Laden neu gerechnet — gespeicherte
 * Merkmale koennten den Daten widersprechen, und dann wuesste niemand,
 * welches stimmt.
 *
 * ── WAS DIESE FASSUNG NICHT KANN (benannt statt verschwiegen) ────────────
 *
 *   * Keine Kompression. Ein Flussabzug mit fuenf Umdrehungen je Spur ist
 *     mehrere Megabyte. Die Blockstruktur traegt einen spaeteren
 *     `FLXZ`-Block ohne Aenderung an Lesern, die ihn nicht kennen.
 *   * Kein Streaming: `uftd_save()` baut alles im Speicher.
 *   * Die Versionspruefung ist GROB — `UFTD_VERSION` wird als Ganzes
 *     verglichen, eine Regel „Nebenversion darf hoeher sein" gibt es
 *     nicht. Sie kommt, wenn es eine zweite Version gibt; vorher waere
 *     sie eine Regel ohne Fall.
 */

#ifndef UFT_DISK2_IO_H
#define UFT_DISK2_IO_H

#include "uft/core/uft_disk2.h"

#ifdef __cplusplus
extern "C" {
#endif

#define UFTD_MAGIC    "UFTD"
#define UFTD_VERSION  1u

typedef enum {
    UFTD_OK = 0,
    UFTD_E_ARG,
    UFTD_E_NOMEM,
    UFTD_E_MAGIC,         /**< kein UFTD                                */
    UFTD_E_VERSION,       /**< Fassung zu neu                           */
    UFTD_E_TRUNCATED,     /**< Datei endet vor dem END-Block            */
    UFTD_E_CRC,           /**< Blockpruefsumme falsch                   */
    UFTD_E_STRUCT         /**< Blockinhalt widerspricht sich            */
} uftd_err_t;

typedef struct {
    uftd_err_t  code;
    const char *what;      /**< Klartext, statisch                      */
    size_t      offset;    /**< Byteversatz der Stelle                  */
    char        block[5];  /**< Blockkennung, wenn bekannt              */
} uftd_result_t;

/**
 * Serialisiert in einen Puffer, den der AUFRUFER mit free() freigibt.
 * @return UFTD_OK; @p out_buf und @p out_len sind dann gueltig.
 */
uftd_result_t uftd_save(const uft_disk2_t *d, uint8_t **out_buf,
                        size_t *out_len);

/**
 * Laedt aus einem Puffer in ein NEUES Modell.
 *
 * Bei UFTD_E_CRC oder UFTD_E_STRUCT ist @p out_disk trotzdem gesetzt und
 * enthaelt alles, was VOR der Stelle gelesen wurde — plus einen Befund,
 * der sagt, wo es abbrach. Ein halb geladenes Modell mit Befund ist mehr
 * wert als NULL. Der Aufrufer entscheidet, ob er es benutzt; freigeben
 * muss er es in jedem Fall.
 */
uftd_result_t uftd_load(const uint8_t *buf, size_t len,
                        uft_disk2_t **out_disk);

/** Dateihilfen. */
uftd_result_t uftd_save_file(const uft_disk2_t *d, const char *path);
uftd_result_t uftd_load_file(const char *path, uft_disk2_t **out_disk);

const char *uftd_err_name(uftd_err_t e);

/** CRC32 (IEEE), oeffentlich fuer Tests. */
uint32_t uftd_crc32(const uint8_t *p, size_t n);

#ifdef __cplusplus
}
#endif
#endif /* UFT_DISK2_IO_H */
