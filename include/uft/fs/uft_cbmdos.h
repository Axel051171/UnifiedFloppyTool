/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file uft_cbmdos.h
 * @brief CBM-DOS-Verzeichnis lesen (Commodore 1541/1571/1581) — MF-683
 *
 * ── Wozu ─────────────────────────────────────────────────────────────────
 *
 * Bis MF-683 las UFT ein D64 in Sektoren und konnte einem Menschen nicht
 * sagen, welche **Dateien** darauf stehen. `src/fs/` kannte AmigaDOS und
 * FAT12, kein CBM. Das ist die Tür, hinter der der Leser wartete.
 *
 * Dieses Modul liest das Verzeichnis, nicht die Dateien. Der Unterschied
 * ist Absicht: „was ist drauf" ist die Frage, die vor jeder anderen
 * kommt, und sie lässt sich beantworten, ohne eine einzige Datei
 * anzufassen.
 *
 * ── Referenz ─────────────────────────────────────────────────────────────
 *
 * Das Verhalten ist gegen **VICE 3.10 `c1541`** geprüft, und zwar gegen
 * den Befehl, der das Korpus-Abbild erzeugt hat (wörtlich in
 * `tests/corpus_manifest/manifest.json`):
 *
 *     c1541 -format "uftcorpus,42" d64 <img> -write marker.txt "uft marker"
 *
 * Daraus stammen die vier Erwartungen in `tests/test_cbmdos_directory.c`
 * — Diskname, ID, Dateizahl, Dateiname. Keine davon ist aus dem Abbild
 * abgelesen; sie stehen im Befehl.
 *
 * Die Struktur des Verzeichnisses (Spur 18, Kette ab Sektor 1, 8
 * Einträge je Sektor à 32 Byte, Namen PETSCII mit 0xA0 aufgefüllt) folgt
 * der CBM-DOS-2.6-Beschreibung, wie sie in der VICE-Dokumentation und in
 * `lib1541img` (BSD-2, im Baum als Referenz aus MF-649 bekannt)
 * übereinstimmend steht.
 *
 * ── Was dieses Modul NICHT tut ───────────────────────────────────────────
 *
 * Es entpackt keine Dateien, es folgt keiner Datei-Sektorkette, es
 * schreibt nichts. Wer Inhalte will, braucht einen zweiten Schritt — und
 * der braucht seinen eigenen Rotbeweis gegen ein Oracle, das Inhalte
 * herausgibt.
 */

#ifndef UFT_FS_CBMDOS_H
#define UFT_FS_CBMDOS_H

#include "uft/uft_error.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Dateitypen nach CBM DOS 2.6 (die unteren vier Bit des Typbytes). */
typedef enum {
    UFT_CBMDOS_DEL = 0,   /**< geloescht */
    UFT_CBMDOS_SEQ = 1,   /**< sequentiell */
    UFT_CBMDOS_PRG = 2,   /**< Programm */
    UFT_CBMDOS_USR = 3,   /**< benutzerdefiniert */
    UFT_CBMDOS_REL = 4    /**< relativ */
} uft_cbmdos_type_t;

/** Ein Verzeichniseintrag. */
typedef struct {
    char                name[17];    /**< PETSCII->ASCII, 0xA0-Fuellung ab */
    uft_cbmdos_type_t   type;        /**< Dateityp */
    bool                closed;      /**< Bit 7 des Typbytes: sauber geschlossen */
    bool                locked;      /**< Bit 6: schreibgeschuetzt */
    uint16_t            blocks;      /**< belegte Bloecke laut Eintrag */
    uint8_t             track;       /**< erster Datensektor */
    uint8_t             sector;
} uft_cbmdos_entry_t;

/**
 * Ein gelesenes Verzeichnis.
 *
 * `entries` wird belegt und gehoert dem Aufrufer bis
 * @ref uft_cbmdos_free.
 */
typedef struct {
    char                 disk_name[17];
    char                 disk_id[3];    /**< die zwei ID-Zeichen (nicht der DOS-Typ) */
    uft_cbmdos_entry_t  *entries;
    int                  entry_count;
    int                  deleted_count; /**< uebersprungene DEL-Eintraege */
} uft_cbmdos_dir_t;

/**
 * Liest das Verzeichnis eines D64-Abbilds.
 *
 * Oeffnet die Datei nur lesend und veraendert sie nicht.
 *
 * @param path  Pfad zum D64-Abbild
 * @param out   Ergebnis; bei Erfolg mit @ref uft_cbmdos_free freigeben
 * @return UFT_OK, oder ein Fehler wenn die Datei fehlt, zu kurz ist oder
 *         das Verzeichnis nicht plausibel ist
 */
uft_error_t uft_cbmdos_read_directory(const char *path,
                                      uft_cbmdos_dir_t *out);

/** Gibt die Eintragsliste frei und nullt die Struktur. */
void uft_cbmdos_free(uft_cbmdos_dir_t *dir);

/** Kurzname eines Typs ("PRG", "SEQ", …) — nie NULL. */
const char *uft_cbmdos_type_name(uft_cbmdos_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FS_CBMDOS_H */
