/**
 * @file uft_disk_create_image.h
 * @brief Ein neues, leeres Diskettenabbild anlegen (MF-991).
 *
 * Die Tuer zu `plugin->create`. Bis MF-991 hatte dieser Zeiger drei
 * Umsetzungen (`img`, `hfe`, `g64`) und **null Aufrufer**; die frueher
 * hier gedachte API war in MF-294 entfernt worden, weil ihr Name mit dem
 * 0-Argument-Allokator `uft_disk_create()` kollidierte — eine
 * Signaturbombe ohne Uebersetzerwarnung. MF-294 verlangte fuer den
 * Ersatz ausdruecklich einen **eigenen Namen**; das ist dieser.
 *
 * Begruendung im Detail: `src/core/uft_disk_create_image.c`.
 */

#ifndef UFT_DISK_CREATE_IMAGE_H
#define UFT_DISK_CREATE_IMAGE_H

#include <stdbool.h>

#include "uft/uft_types.h"
#include "uft/uft_error.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Legt ein neues, leeres Abbild im gewuenschten Format an.
 *
 * @param path                Zieldatei.
 * @param format_name         Plugin-Name wie in der Registry ("IMG",
 *                            "HFE", "G64"); Gross-/Kleinschreibung egal.
 * @param geometry            Gewuenschte Geometrie. Das Plugin darf
 *                            strenger sein (IMG besteht auf 512 Byte je
 *                            Sektor) und sagt dann selbst ab.
 * @param overwrite_existing  `false` = eine vorhandene Datei bleibt
 *                            unangetastet und die Funktion sagt ab.
 *                            **Das ist die Vorgabe der sicheren Seite:**
 *                            alle drei `create`-Umsetzungen beginnen mit
 *                            `fopen(path,"wb")` und wuerden eine
 *                            vorhandene Datei sofort auf null kuerzen.
 * @param out                 Bei `UFT_OK` das offene Abbild; der Aufrufer
 *                            gibt es mit `uft_disk_close()` frei. In
 *                            **jedem** Fehlerfall auf NULL gesetzt.
 *
 * @retval UFT_OK                     angelegt und geoeffnet
 * @retval UFT_ERROR_INVALID_ARG      Nullzeiger oder eine Geometrie ohne
 *                                    Spuren/Koepfe/Sektoren
 * @retval UFT_ERROR_UNKNOWN_FORMAT   kein Plugin dieses Namens
 * @retval UFT_ERROR_NOT_SUPPORTED    das Plugin kann nicht erzeugen —
 *                                    **es entsteht keine Datei**
 * @retval UFT_ERROR_FILE_EXISTS      Ziel vorhanden, keine Freigabe —
 *                                    **die Datei bleibt unveraendert**
 * @retval sonst                      der Fehler des Plugins, unveraendert
 *                                    durchgereicht
 *
 * Warum ein Fehlercode und kein Zeiger wie bei `uft_disk_open()`: dort
 * bedeutet NULL alles Moegliche. Beim Erzeugen sind „dieses Format kann
 * ich nicht anlegen" und „die Datei liess sich nicht schreiben" zwei
 * verschiedene Auskuenfte, und wer sie zusammenzieht, zwingt den Aufrufer
 * zum Raten.
 */
uft_error_t uft_disk_create_image(const char *path,
                                  const char *format_name,
                                  const uft_geometry_t *geometry,
                                  bool overwrite_existing,
                                  uft_disk_t **out);

#ifdef __cplusplus
}
#endif

#endif /* UFT_DISK_CREATE_IMAGE_H */
