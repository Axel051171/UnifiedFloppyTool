#ifndef UFT_FORMATS_AGAT_H
#define UFT_FORMATS_AGAT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* MF-558: dieser Header ist LEER, und das ist keine Aussage ueber das
 * Format.
 *
 * Hier stand ein Satz, der dieses Modul als unfertig bezeichnete.
 * Er las sich wie ein Gestaendnis: wer ihn in `agat.h` fand, schloss daraus, die
 * Unterstuetzung fuer dieses Format sei unfertig. Bei den meisten der 67
 * Header mit jenem Satz war das falsch — die Module dahinter sind
 * vollstaendig, sie brauchen von hier nur die drei System-Includes.
 *
 * Was dieser Header IST: ein Aufhaenger. Er existiert, damit
 * `#include "uft/formats/agat.h"` aufloest, und enthaelt bewusst keine
 * Deklarationen. Wo das Format wirklich implementiert ist, sagt die
 * `.c`-Datei, die ihn einbindet.
 *
 * Was in `docs/PLANNED_APIS.md` als "Banner-Header" gefuehrt wird, meint
 * die ANDERE Sorte: Header mit Prototypen, die keine Definition haben.
 * Davon gibt es acht, und die stehen dort einzeln. Diese 67 gehoerten nie
 * dazu und haben die Zahl nur aufgeblaeht. */

#ifdef __cplusplus
}
#endif

#endif /* UFT_FORMATS_AGAT_H */
