/* The diagnostics header and uft_types.h in ONE translation unit
 * (MF-1341).
 *
 * `uft_sector_status_t` is defined four times in this tree, with four
 * different meanings (measured with `git grep '} uft_sector_status_t;'`):
 *
 *   include/uft/uft_types.h                 bit mask, WEAK = 1 << 4 = 16
 *   include/uft/protection/uft_protection.h sequential,  WEAK = 7
 *   src/recovery/uft_sector_recovery.h      sequential,  WEAK = 4
 *   include/uft/diag/uft_disc_diagnostics.h scan result, WEAK = 2
 *
 * The first two share the guard UFT_SECTOR_STATUS_T_DEFINED (whichever
 * comes first wins; tracked in docs/shared_guard_baseline.txt). The
 * diagnostics header had NO guard, so it could not be compiled next to
 * uft_types.h at all: "redeclaration of enumerator 'UFT_SECTOR_WEAK'".
 * That is why the H-18 tests had to be split into two files (MF-1339).
 *
 * The scan result is not a sector status in the uft_types.h sense (it
 * counts retries: GOOD / WEAK = read after retry / BAD), so it gets its
 * own name instead of borrowing one with a different meaning.
 *
 * Red proof: before MF-1341 this file does not compile.
 */
#include "uft/uft_types.h"
#include "uft/diag/uft_disc_diagnostics.h"

#include <stdio.h>

int main(void)
{
    int rot = 0;

    /* uft_types.h keeps its meaning: WEAK is a flag bit. */
    if (UFT_SECTOR_WEAK != (1 << 4)) {
        printf("  [ROT] UFT_SECTOR_WEAK ist %d, erwartet 16 (Bitmaske aus uft_types.h)\n",
               (int)UFT_SECTOR_WEAK);
        rot++;
    } else {
        printf("  [ok ] UFT_SECTOR_WEAK behaelt die Bedeutung aus uft_types.h (16)\n");
    }

    /* The scan result keeps its values: the result arrays store them as
     * uint8_t, so a renumbering would change what old results mean. */
    if (UFT_DIAG_SECTOR_UNKNOWN != 0 || UFT_DIAG_SECTOR_GOOD != 1 ||
        UFT_DIAG_SECTOR_WEAK != 2 || UFT_DIAG_SECTOR_BAD != 3) {
        printf("  [ROT] die Werte der Scan-Ergebnisse haben sich verschoben\n");
        rot++;
    } else {
        printf("  [ok ] Scan-Ergebnisse unveraendert 0/1/2/3\n");
    }

    uft_diag_sector_status_t s = UFT_DIAG_SECTOR_BAD;
    printf("  [ok ] eigener Typname uebersetzt (BAD = %d)\n", (int)s);

    printf("\n%s\n", rot ? "ROT" : "GRUEN");
    return rot ? 1 : 0;
}
