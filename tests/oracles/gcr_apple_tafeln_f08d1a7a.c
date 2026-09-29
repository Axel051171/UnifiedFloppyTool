/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file gcr_apple_tafeln_f08d1a7a.c
 * @brief Die Apple-GCR-Tafeln, wie sie bis `f08d1a7a` in
 *        `src/formats/apple/uft_apple_gcr.c` standen — als ORAKEL
 *        (MF-1515, Schritt 3 von P3-666).
 *
 * ── Warum sie hier liegen und nicht gelöscht wurden ───────────────────
 *
 * Seit MF-1515 erzeugt `src/core/uft_gcr.c` diese Wortmengen aus einem
 * Praedikat; die Tafeln im Produktionscode sind damit entfallen. Sie zu
 * loeschen waere aber der Verlust ihres Belegs — und Loeschen ist keine
 * Behebung (MF-1077).
 *
 * Hier werden sie zur **Zusage**: `tests/test_gcr_apple_gegen_tafel.c`
 * verlangt, dass die erzeugte Menge byteweise gleich dieser Fassung ist.
 * Damit sind Erzeuger (Praedikat) und Pruefer (diese Tafel) zwei
 * verschiedene Haende — die Bedingung aus MF-644, die ein Test gegen die
 * eigene Quelle nicht erfuellt.
 *
 * Der Commit-Hash im Dateinamen ist Absicht: er sagt, WELCHE Fassung hier
 * eingefroren ist, statt es der Prosa zu ueberlassen. Wer sie ersetzt,
 * legt eine neue Datei mit neuem Hash daneben.
 *
 * **Diese Datei wird nie gegen das Register gepflegt.** Aendert sich das
 * Praedikat, faellt der Test — und das ist der Zweck. Ein Orakel, das man
 * nachzieht, ist keins.
 *
 * ── Die Herkunft der Zahlen, wörtlich mitgenommen ─────────────────────
 *
 * **6&2 (64 Werte).** Aus `Beneath Apple DOS`, Kapitel 3, und in UFT seit
 * MF-719 in Gebrauch. Der Kommentar an der alten Stelle sagte bis
 * MF-1515, eine hinreichende Regel sei „ohne die gedruckte Quelle nicht
 * feststellbar" — sie ist es doch, siehe den berichtigten Kopf von
 * `uft_apple_gcr.c`. Die Tafel bleibt trotzdem der Zeuge.
 *
 * **5&3 (32 Werte).** GEMESSEN (MF-719), nicht erinnert: aus einem
 * `to_woz2`-WOZ, dessen Eingabe alle 256 Bytewerte enthielt, kamen ueber
 * 13 Datenfelder genau diese 32 heraus — und keinen mehr. Ein erster Lauf
 * mit einem schwaecheren Muster hatte nur 24 ausgeloest; eine Teilmenge
 * kann eine Tabelle bestaetigen, aber nicht vervollstaendigen.
 * Drittbestaetigt durch `mamedev/mame` `ap2_dsk.cpp` (`translate5`,
 * BSD-3-Clause) — eine Quelle, die die Messung nicht kannte.
 *
 * Diese Herkunft ist der Grund, warum die Tafeln als Orakel taugen: sie
 * stammen nicht aus dem Praedikat, das sie pruefen.
 */
#include <stdint.h>

/** 6&2, Index k -> Diskettenbyte. Stand `f08d1a7a`. */
const uint8_t ORAKEL_A2_WRITE_TAB[64] = {
    0x96, 0x97, 0x9A, 0x9B, 0x9D, 0x9E, 0x9F, 0xA6,
    0xA7, 0xAB, 0xAC, 0xAD, 0xAE, 0xAF, 0xB2, 0xB3,
    0xB4, 0xB5, 0xB6, 0xB7, 0xB9, 0xBA, 0xBB, 0xBC,
    0xBD, 0xBE, 0xBF, 0xCB, 0xCD, 0xCE, 0xCF, 0xD3,
    0xD6, 0xD7, 0xD9, 0xDA, 0xDB, 0xDC, 0xDD, 0xDE,
    0xDF, 0xE5, 0xE6, 0xE7, 0xE9, 0xEA, 0xEB, 0xEC,
    0xED, 0xEE, 0xEF, 0xF2, 0xF3, 0xF4, 0xF5, 0xF6,
    0xF7, 0xF9, 0xFA, 0xFB, 0xFC, 0xFD, 0xFE, 0xFF
};

/** 5&3 (DOS 3.2), Index k -> Diskettenbyte. Stand `f08d1a7a`. */
const uint8_t ORAKEL_A2_TAB5[32] = {
    0xAB, 0xAD, 0xAE, 0xAF, 0xB5, 0xB6, 0xB7, 0xBA,
    0xBB, 0xBD, 0xBE, 0xBF, 0xD6, 0xD7, 0xDA, 0xDB,
    0xDD, 0xDE, 0xDF, 0xEA, 0xEB, 0xED, 0xEE, 0xEF,
    0xF5, 0xF6, 0xF7, 0xFA, 0xFB, 0xFD, 0xFE, 0xFF
};
