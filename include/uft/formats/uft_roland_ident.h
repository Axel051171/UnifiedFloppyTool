/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_roland_ident.h
 * @brief Inhaltsbasierte Erkennung von Roland-S-Serie-Disketten.
 *
 * WARUM INHALTSBASIERT UND NICHT GROESSENBASIERT
 * ---------------------------------------------
 * Roland S-50/S-330/S-550/W-30 schreiben auf physikalisch voellig normale
 * PC-720-KB-Disketten: 80 Zylinder x 2 Koepfe x 9 Sektoren x 512 Byte =
 * 737.280 Byte. Diese Zahl ist im UFT-Baum bereits neunfach beansprucht
 * (uft_meritum.c, uft_robotron.c, uft_pravetz.c, dsk_generic DSK_MSX /
 * DSK_UNI / DSK_EMU / DSK_FLEX / DSK_OS9 / DSK_DC42, uft_dim.c 2DD9).
 *
 * Eine groessenbasierte Roland-Probe waere der zehnte Anspruch auf dieselbe
 * Zahl und wuerde auf JEDER PC-720-KB-Diskette anschlagen. Deshalb entscheidet
 * hier ausschliesslich der Inhalt von Sektor 0.
 *
 * SPEC_STATUS
 *   Quelle A: SDISK for Windows v1.7, (c) 2011 Miroslav Svetlik, MIT-Lizenz.
 *             Statische Analyse von SDISKW.EXE (23.552 Byte,
 *             md5 49fe78cc64d8d86d331d9e612b807ec0):
 *               - Erkennungsroutine    VA 0x401a82
 *               - Deskriptortabelle    VA 0x4069bf, 7 Eintraege, Schritt 0x3B
 *               - Zahl der Eintraege   VA 0x4069bb (= 7)
 *               - Ersatzdatensatz      VA 0x406b5c ("Unknown", Feld +16 = -1)
 *               - Aufrufer             VA 0x402711 (Laufwerk), 0x4032b8 (Datei)
 *                 lesen beide Sektor 0, Laenge 1, in einen 112-Byte-Puffer
 *             MIT erlaubt Nutzung, Veraenderung und Weitergabe; der
 *             Urheberrechtsvermerk gehoert in NOTICE.
 *   Quelle B: x50conv v1.0 readme (2003, Christian Keck) — belegt unabhaengig,
 *             dass S-50 und S-550 zwei UNTERSCHEIDBARE logische Formate auf
 *             identischem Medium sind und dass Abbilder kopflos und
 *             unkomprimiert 737.280 Byte gross sind.
 *             ACHTUNG: die x50conv-Lizenz untersagt Disassemblierung
 *             ausdruecklich. Aus diesem Werkzeug wurde NICHTS ausser der
 *             mitgelieferten Dokumentation verwendet.
 *
 * NICHT VERIFIZIERT: Verhalten an einem echten Roland-Abbild. Kein Exemplar
 * im Korpus. Daher T2 und nicht T1 — siehe VERIFICATION_TIERS.md.
 */

#ifndef UFT_ROLAND_IDENT_H
#define UFT_ROLAND_IDENT_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Physikalische Geometrie aller vier Modelle (DD). */
#define UFT_ROLAND_CYLINDERS      80u
#define UFT_ROLAND_HEADS           2u
#define UFT_ROLAND_SPT             9u
#define UFT_ROLAND_SECTOR_SIZE   512u
#define UFT_ROLAND_IMAGE_SIZE    737280u   /* 0xB4000 */

/** Laenge des Blocks, den das Orakel zur Erkennung heranzieht. */
#define UFT_ROLAND_IDENT_LEN     112u      /* 0x70 */

/** Versatz der beiden verglichenen 32-Bit-Worte in Sektor 0. */
#define UFT_ROLAND_KEY_OFF_LO      4u
#define UFT_ROLAND_KEY_OFF_HI     12u

typedef struct {
    uint8_t     key_lo[4];   /**< erwartet bei Sektor 0 + 4  */
    uint8_t     key_hi[4];   /**< erwartet bei Sektor 0 + 12 */
    const char *model;       /**< "S330", "S50", "S550", "W30" */
    const char *content;     /**< "Sound", "Utility", "Song", ... */
} uft_roland_id_t;

/**
 * Erkennt die Diskette aus Sektor 0.
 *
 * @param sector0  mindestens UFT_ROLAND_IDENT_LEN Byte ab Sektoranfang
 * @param len      Laenge des uebergebenen Puffers
 * @return         Zeiger auf den Tabelleneintrag oder NULL, wenn keiner passt.
 *                 NULL heisst "keine Roland-Diskette" — nicht "unbekannte
 *                 Roland-Diskette". Das Orakel setzt an dieser Stelle einen
 *                 Ersatzdatensatz "Unknown"; UFT tut das absichtlich NICHT,
 *                 weil eine Behauptung ueber das Medium sonst ohne Messung
 *                 entsteht.
 */
const uft_roland_id_t *uft_roland_identify(const uint8_t *sector0, size_t len);

/** Zahl der Tabelleneintraege (fuer Tests und Aufzaehlung). */
size_t uft_roland_id_count(void);

/** Tabelleneintrag nach Index, NULL bei Ueberlauf. */
const uft_roland_id_t *uft_roland_id_at(size_t index);

/**
 * Probe fuer die Plugin-Schnittstelle.
 *
 * @param data       Anfang des Abbilds (mindestens ein Sektor)
 * @param data_len   verfuegbare Bytes in @p data
 * @param file_size  Gesamtgroesse der Datei
 * @param confidence 0..100, nur bei Rueckgabe true belegt
 *
 * Die Groesse ist eine NOTWENDIGE, aber keine hinreichende Bedingung: passt
 * sie nicht, ist es keine Roland-DD-Diskette; passt sie, entscheidet
 * ausschliesslich Sektor 0. Ohne Treffer in Sektor 0 gibt die Probe false
 * zurueck, auch wenn die Groesse stimmt.
 */
bool uft_roland_probe(const uint8_t *data, size_t data_len,
                      size_t file_size, int *confidence);

#ifdef __cplusplus
}
#endif

#endif /* UFT_ROLAND_IDENT_H */
