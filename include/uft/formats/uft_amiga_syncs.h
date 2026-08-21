/**
 * @file uft_amiga_syncs.h
 * @brief Die Amiga-MFM-Sync-Muster — eine Quelle (MF-453)
 *
 * Bis MF-453 lagen dieselben Werte an drei Stellen im Baum, und sie
 * widersprachen sich:
 *
 *   src/analysis/uft_track_analysis.h:41-44   0xA245 = "Ocean/Imagine"
 *   src/formats/amiga/uft_amiga_protection.h  0xA245 = "Beyond the Ice Palace"
 *   src/protection/uft_amiga_protection.c     kennt keinen davon, dafuer
 *                                             0x8a91 und 0x8914
 *
 * Und der Decoder benutzte keine davon: `flux_decode_amiga_bits()` suchte
 * ausschliesslich MFM_SYNC_PATTERN (0x4489). Eine Amiga-Diskette mit
 * Arkanoid- oder Mercenary-Sync dekodierte damit zu null Sektoren, waehrend
 * zwei andere Module wussten, dass es diese Syncs gibt.
 *
 * ── Herkunft der Namen ──────────────────────────────────────────────────────
 *
 * Aus dem 68k-Quelltext von X-Copy Professional 5.3, `xcop.s:2347-2351`. Die
 * Tabelle steht dort auskommentiert neben der Suchschleife, die dieselben
 * Werte in Registern haelt (`xcop.s:2114-2118`):
 *
 *     ;synctab
 *     ;   DC.W  $9521,$A245,$A89A,$448A,$4489,$0000,...
 *     ;   DC.W  $9521     ; ARKANOID SYNC
 *     ;   DC.W  $A245     ; BEYOND THE ICE PALACE
 *     ;   DC.W  $A89A     ; MERCENERY/BACKLASH
 *
 * Damit ist der Namensstreit entschieden: 0xA245 ist Beyond the Ice Palace.
 * Fuer 0x448A gibt die Quelle keinen Namen — es steht in der Suchschleife und
 * in der Tabellenzeile, ohne Zuordnung. Das wird hier so gesagt und nicht
 * ausgefuellt.
 *
 * ── Was NICHT hier steht ────────────────────────────────────────────────────
 *
 * 0xF8BC. Der Wert ist in X-Copy `INDEXCOPY` (`xcopy.i`) und dient als
 * Modus-Sentinel — "kein Custom-Sync, index-synchron kopieren" (`xcop.s:2112`).
 * Er steht nie auf einer Diskette. Siehe MF-452.
 *
 * 0x8a91 und 0x8914 aus `src/protection/uft_amiga_protection.c` (CopyLock bzw.
 * Psygnosis Type B). Sie kommen nicht aus der X-Copy-Quelle und sind hier
 * nicht verifizierbar; sie bleiben, wo sie sind, und sind dort als unbelegt
 * markiert.
 */

#ifndef UFT_AMIGA_SYNCS_H
#define UFT_AMIGA_SYNCS_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Standard-AmigaDOS-Sync (0xA1 mit fehlendem Taktbit). */
#define UFT_AMIGA_SYNC_STANDARD   0x4489u

/**
 * @brief Ein bekanntes Amiga-Sync-Muster.
 *
 * `name` ist NULL, wenn die Quelle keinen Namen nennt — das ist eine Aussage,
 * kein fehlender Eintrag.
 */
typedef struct uft_amiga_sync {
    uint16_t    pattern;
    const char *name;        /**< z.B. "Arkanoid", oder NULL wenn unbenannt */
    const char *source;      /**< Woher der Name stammt */
} uft_amiga_sync_t;

/** Alle bekannten Muster, Standard-Sync zuerst. */
extern const uft_amiga_sync_t UFT_AMIGA_SYNCS[];
extern const size_t           UFT_AMIGA_SYNC_COUNT;

/** Nur die Zahlenwerte, fuer Decoder-Optionen. Reihenfolge wie oben. */
extern const uint16_t         UFT_AMIGA_SYNC_PATTERNS[];

/**
 * @brief Eintrag zu einem Muster, oder NULL wenn unbekannt.
 *
 * Unbekannt heisst unbekannt — die Funktion raet nicht und liefert keinen
 * Ersatzeintrag.
 */
const uft_amiga_sync_t *uft_amiga_sync_lookup(uint16_t pattern);

/** true, wenn @p pattern ein Muster ist, das auf einer Diskette stehen kann. */
int uft_amiga_sync_is_known(uint16_t pattern);

#ifdef __cplusplus
}
#endif

#endif /* UFT_AMIGA_SYNCS_H */
