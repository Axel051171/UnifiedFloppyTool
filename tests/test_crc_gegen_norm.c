/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_crc_gegen_norm.c
 * @brief Die CRCs gegen die NORM — und deshalb braucht `crc.c` keine
 *        Ableitung (MF-1126, Stufe 1)
 *
 * ── Der Auftrag und seine Antwort ─────────────────────────────────────────
 *
 * Die Eigentuemer-Weisung zu `src/dtc_components/` ordnet die Module
 * nach Beweisbarkeit und stellt `crc.c` auf **Stufe 1 — normbestimmt,
 * sofort ableitbar (auch in Fall A)**, mit der bindenden Reihenfolge:
 * „die Norm zuerst, der Fremdbestand als Vergleich. Nie umgekehrt."
 * Und: „deine Ableitung muss mit der oeffentlichen Quelle
 * uebereinstimmen, nicht mit dem Fremdbestand. Weicht der Fremdbestand
 * ab, ist er der Befund."
 *
 * Gemessen ist die Antwort: **eine Ableitung ist nicht noetig.** Der
 * Fremdbestand trifft die Normpruefwerte, und UFTs eigene Umsetzung
 * trifft sie auch. Eine neue Datei `src/formats/crc/uft_crc16_ccitt.c`
 * anzulegen waere die **vierte** Umsetzung eines Wertes, den der Baum
 * schon richtig rechnet — genau die Duplikatsklasse, die dieser Baum
 * wiederholt als Defekt gemessen hat (MF-1015 drei Pruefsummen,
 * MF-1026 drei Victor-Geometrien, die GCR-Tafel sechsfach). Statt einer
 * Kopie steht hier die Messung, und sie laeuft mit.
 *
 * ── Die Norm-Referenzen (Reihenfolge ist bindend) ─────────────────────────
 *
 * Referenz A: Greg Cook, „Catalogue of parametrised CRC algorithms",
 *             https://reveng.sourceforge.io/crc-catalogue/16.htm
 *             und .../17plus.htm
 * Referenz B: Philip Koopman, „Best CRC Polynomials", CMU,
 *             https://users.ece.cmu.edu/~koopman/crc/
 *
 * Der Pruefwert der Norm ist stets die Zeichenfolge `"123456789"`
 * (9 Byte ASCII):
 *
 *   CRC-32/ISO-HDLC   poly 0x04C11DB7 (gespiegelt 0xEDB88320),
 *                     init 0xFFFFFFFF, refin/refout true,
 *                     xorout 0xFFFFFFFF            -> **0xCBF43926**
 *   CRC-16/IBM-3740   poly 0x1021, init 0xFFFF,
 *                     refin/refout false, xorout 0 -> **0x29B1**
 *                     (im Feld oft „CCITT-FALSE" genannt)
 *   CRC-16/ARC        poly 0x8005 (gespiegelt 0xA001),
 *                     init 0x0000, refin/refout true,
 *                     xorout 0                     -> **0xBB3D**
 *
 * Die drei Normwerte werden hier **doppelt** gehalten: einmal als
 * Konstante (das ist die Norm) und einmal von einer in dieser Datei
 * eigenstaendig geschriebenen, bitweisen Umsetzung nachgerechnet. Faellt
 * diese Selbsteichung, ist die Messung kaputt und nicht der Prueflings —
 * ein Vergleich gegen eine ungeeichte Referenz waere wertlos.
 *
 * ── Was geprueft wird und woher es kommt ──────────────────────────────────
 *
 *   `air_crc32_buffer()`    UFT, `include/uft/formats/uft_air_crc32.h`
 *                           (kopfeigen, tabellengetrieben)
 *   `flux_crc16_ccitt()`    UFT, `src/flux/uft_flux_decoder.c` — die
 *                           Pruefsumme, mit der MF-864 die FM-Spur an
 *                           `fluxtoimd` abgenommen hat. Sie war gegen
 *                           eine fremde Hand gehalten, aber nie gegen
 *                           den Normpruefwert.
 *   `dtc_crc32`, `dtc_crc16_ccitt`, `dtc_crc16_ibm`
 *                           Fremdbestand, `src/dtc_components/src/crc.c`
 *                           — **ausgefuehrt, nicht uebernommen**. Die
 *                           Datei wird nicht angefasst; wer sie aendert,
 *                           macht aus einem Beleg eine Ableitung.
 *
 * ── Was dieser Test NICHT sagt ────────────────────────────────────────────
 *
 * Er sagt **nichts ueber die Herkunft**. Dass zwei Umsetzungen denselben
 * Normwert treffen, ist kein Herkunftsindiz — der Wert ist durch die
 * Norm bestimmt, jede korrekte Umsetzung trifft ihn. Genau deshalb steht
 * `crc.c` auf Stufe 1 und nicht auf Stufe 3.
 *
 * Er sagt auch nichts ueber `uft_crc_polys.h`. Dort liegt eine
 * 481-zeilige CRC-Datenbank im Zustand `UFT_SKELETON_PARTIAL` — 4
 * Prototypen, 3 ohne Koerper, **null Aufrufer**. Das ist die eigentliche
 * Luecke der CRC-Ebene dieses Baums, und sie ist Stummel-Arbeit, keine
 * Ableitung. Benannt statt stillschweigend uebergangen.
 */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

#include "uft/formats/uft_air_crc32.h"

/* UFT, aus dem Flusspfad */
uint16_t flux_crc16_ccitt(const uint8_t *data, size_t len);

/* Fremdbestand — nur AUSGEFUEHRT */
uint32_t dtc_crc32(const void *p, size_t n, uint32_t init);
uint16_t dtc_crc16_ccitt(const void *p, size_t n, uint16_t init);
uint16_t dtc_crc16_ibm(const void *p, size_t n, uint16_t init);

#define NORM_CRC32      0xCBF43926uL
#define NORM_CRC16_3740 0x29B1uL
#define NORM_CRC16_ARC  0xBB3DuL

static int gruen = 0, rot = 0;

static void pruefe(const char *was, unsigned long ist, unsigned long soll)
{
    const int ok = (ist == soll);
    printf("  %-4s %-52s 0x%08lX (soll 0x%08lX)\n",
           ok ? "ok" : "ROT", was, ist, soll);
    if (ok) gruen++; else rot++;
}

/* ── die Norm, in DIESER Datei eigenstaendig und bitweise ─────────────── */

static uint32_t norm_crc32(const uint8_t *d, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= d[i];
        for (int j = 0; j < 8; j++)
            c = (c & 1u) ? ((c >> 1) ^ 0xEDB88320u) : (c >> 1);
    }
    return c ^ 0xFFFFFFFFu;
}

static uint16_t norm_crc16_ibm3740(const uint8_t *d, size_t n)
{
    uint16_t c = 0xFFFFu;
    for (size_t i = 0; i < n; i++) {
        c ^= (uint16_t)((uint16_t)d[i] << 8);
        for (int j = 0; j < 8; j++)
            c = (c & 0x8000u) ? (uint16_t)((c << 1) ^ 0x1021u)
                              : (uint16_t)(c << 1);
    }
    return c;
}

static uint16_t norm_crc16_arc(const uint8_t *d, size_t n)
{
    uint16_t c = 0x0000u;
    for (size_t i = 0; i < n; i++) {
        c ^= d[i];
        for (int j = 0; j < 8; j++)
            c = (c & 1u) ? (uint16_t)((c >> 1) ^ 0xA001u) : (uint16_t)(c >> 1);
    }
    return c;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    const uint8_t p[] = "123456789";
    const size_t n = 9;          /* ohne die abschliessende Null */

    printf("CRC gegen die Norm - Pruefwert \"123456789\" (MF-1126)\n");
    printf("====================================================\n");

    printf("\n1) Selbsteichung: die Norm, hier eigenstaendig nachgerechnet\n");
    pruefe("CRC-32/ISO-HDLC", norm_crc32(p, n), NORM_CRC32);
    pruefe("CRC-16/IBM-3740", norm_crc16_ibm3740(p, n), NORM_CRC16_3740);
    pruefe("CRC-16/ARC", norm_crc16_arc(p, n), NORM_CRC16_ARC);

    printf("\n2) UFT gegen die Norm\n");
    pruefe("air_crc32_buffer (uft_air_crc32.h)",
           air_crc32_buffer(p, 0, n), NORM_CRC32);
    pruefe("flux_crc16_ccitt (uft_flux_decoder.c)",
           flux_crc16_ccitt(p, n), NORM_CRC16_3740);

    printf("\n3) Der Fremdbestand gegen die Norm (ausgefuehrt, nicht "
           "uebernommen)\n");
    pruefe("dtc_crc32 ^ 0xFFFFFFFF",
           dtc_crc32(p, n, 0xFFFFFFFFu) ^ 0xFFFFFFFFu, NORM_CRC32);
    pruefe("dtc_crc16_ccitt (init 0xFFFF)",
           dtc_crc16_ccitt(p, n, 0xFFFFu), NORM_CRC16_3740);
    pruefe("dtc_crc16_ibm (init 0x0000)",
           dtc_crc16_ibm(p, n, 0x0000u), NORM_CRC16_ARC);

    printf("\n%d gruen, %d rot\n", gruen, rot);
    if (!rot)
        printf("Ergebnis: der Fremdbestand stimmt mit der Norm ueberein, "
               "UFT auch — eine Ableitung von crc.c waere die vierte "
               "Umsetzung desselben Wertes.\n");
    return rot ? 1 : 0;
}
