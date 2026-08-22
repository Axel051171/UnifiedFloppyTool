#ifndef UFT_FORMATS_ATX_H
#define UFT_FORMATS_ATX_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#include "uft/uft_error.h"
#include "uft/uft_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Eine ganze Diskette als ATX schreiben (MF-474).
 *
 * ATX (Atari 8-bit, auch VAPI) ist eine Kette von Spur-Datensätzen ohne
 * Offset-Tabelle. Deshalb gibt es hier keinen `write_track` je Spur: eine
 * Spur nachträglich zu ersetzen hieße, alles danach zu verschieben. Der
 * Export läuft über die ganze Diskette auf einmal.
 *
 * Geschrieben wird, was die Spuren tragen — Status je Sektor (CRC des
 * Daten- und des Adressfelds, fehlendes Datenfeld, Deleted-Mark, langer
 * Sektor, weak), doppelte Sektornummern, Weak-Masken und die
 * Winkelpositionen. Der Leser in `src/formats/atx/uft_atx.c` füllt all das
 * seit MF-467/MF-474, ein Rückweg ist damit Wiedergabe und keine Erfindung.
 *
 * **Grenze, ausdrücklich:** Spuren aus einer anderen Quelle tragen keine
 * Winkelpositionen (`uft_sector_t.has_angular_position`). Sie werden dann
 * gleichmäßig verteilt, und der Aufruf meldet das über `UFT_WARN` — bei ATX
 * ist die Position kopierschutzrelevant, ein gleichmäßiges Layout sieht
 * plausibel aus und ist es nicht.
 *
 * Layout und Feldsemantik sind gegen `src/a8rawconv/diskatx.cpp`
 * (`write_atx`, GPL-2-or-later, Referenz-Orakel, wird nicht gebaut) belegt;
 * die Belege stehen an den einzelnen Stellen in `uft_atx.c`.
 *
 * @param tracks            @p track_count Spuren, Index = Spurnummer
 * @param enhanced_density  Dichtebyte im Dateikopf: 1 = ED/MFM, 0 = SD/FM
 * @return UFT_OK, sonst ein Schreib-/Argumentfehler. Bei Fehler kann eine
 *         angefangene Datei zurückbleiben — der Aufrufer entscheidet, ob er
 *         sie behält.
 */
uft_error_t uft_atx_write(const char *path,
                          const uft_track_t *tracks,
                          size_t track_count,
                          bool enhanced_density);

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMATS_ATX_H */
