/**
 * @file uft_amiga_syncs.c
 * @brief Die eine Tabelle der Amiga-Sync-Muster (MF-453)
 *
 * Begruendung und Herkunft: include/uft/formats/uft_amiga_syncs.h
 */

#include "uft/formats/uft_amiga_syncs.h"

/* Quelle fuer alle Namen: X-Copy Professional 5.3, xcop.s:2347-2351
 * (auskommentierte `synctab` neben der Suchschleife xcop.s:2113-2117). */
#define XCOPY_SRC "X-Copy Pro 5.3, xcop.s:2347"

const uft_amiga_sync_t UFT_AMIGA_SYNCS[] = {
    { 0x4489, "AmigaDOS",              "Commodore-Standard" },
    { 0x9521, "Arkanoid",              XCOPY_SRC },
    { 0xA245, "Beyond the Ice Palace", XCOPY_SRC },
    { 0xA89A, "Mercenary / Backlash",  XCOPY_SRC },
    /* Die Quelle fuehrt 0x448A in der Suchschleife und in der Tabellenzeile,
     * ordnet ihm aber keinen Titel zu. NULL sagt genau das — es ist kein
     * fehlender Eintrag, sondern ein Muster ohne belegten Namen. */
    { 0x448A, NULL,                    XCOPY_SRC },
};

const size_t UFT_AMIGA_SYNC_COUNT =
    sizeof(UFT_AMIGA_SYNCS) / sizeof(UFT_AMIGA_SYNCS[0]);

const uint16_t UFT_AMIGA_SYNC_PATTERNS[] = {
    0x4489, 0x9521, 0xA245, 0xA89A, 0x448A,
};

const uft_amiga_sync_t *uft_amiga_sync_lookup(uint16_t pattern)
{
    for (size_t i = 0; i < UFT_AMIGA_SYNC_COUNT; i++) {
        if (UFT_AMIGA_SYNCS[i].pattern == pattern)
            return &UFT_AMIGA_SYNCS[i];
    }
    return NULL;
}

int uft_amiga_sync_is_known(uint16_t pattern)
{
    return uft_amiga_sync_lookup(pattern) != NULL;
}
