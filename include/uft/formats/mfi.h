#ifndef UFT_FORMATS_MFI_H
#define UFT_FORMATS_MFI_H

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
 * Er las sich wie ein Gestaendnis: wer ihn in `mfi.h` fand, schloss daraus, die
 * Unterstuetzung fuer dieses Format sei unfertig. Bei den meisten der 67
 * Header mit jenem Satz war das falsch — die Module dahinter sind
 * vollstaendig, sie brauchen von hier nur die drei System-Includes.
 *
 * Was dieser Header IST: ein Aufhaenger. Er existiert, damit
 * `#include "uft/formats/mfi.h"` aufloest, und enthaelt bewusst keine
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


/* ─── Dateikennung (MF-863) ─────────────────────────────────────────
 *
 * MFI beginnt mit einer **16 Byte** langen Kennung. Sie stand bis MF-863
 * an zwei Stellen mit zwei verschiedenen Werten:
 *
 *   src/formats/mfi/uft_mfi.c:69   MFI_MAGIC "MAMEFLOPPYIMAGE"   richtig
 *   src/formats/flux/mfi.c:28      memcmp(sig, "MFI", 3)         falsch
 *
 * Die zweite prueft drei Byte, die es im Format nicht gibt: sie weist
 * JEDE echte MFI-Datei ab und nimmt jede an, die zufaellig mit "MFI"
 * beginnt — samt `dev->flux_supported = true`.
 *
 * `uft_mfi.c:8` traegt dazu einen MF-614-Vermerk: eine noch fruehere
 * Fassung nahm 8 Byte an und fuehrte das Feld dahinter als
 * „Form factor" — es liegt IN der Kennung.
 *
 * Deshalb steht sie jetzt hier, einmal, und beide Leser benutzen sie.
 * Zwei Fassungen derselben Groesse driften (P3-103, im Baum 11-mal
 * gemessen).
 *
 * Beide Werte sind 15 Zeichen plus abschliessende Null. */
#define UFT_MFI_MAGIC_LEN   16u
#define UFT_MFI_MAGIC       "MAMEFLOPPYIMAGE"   /* MAME    */
#define UFT_MFI_MAGIC_OLD   "MESSFLOPPYIMAGE"   /* MESS    */

#endif /* UFT_FORMATS_MFI_H */
