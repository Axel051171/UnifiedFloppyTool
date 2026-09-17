/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * fixture_code_b.c — Koeder fuer code_audit.py, Datei 2 von 2.
 *
 * DIESE DATEI WIRD NICHT GEBAUT. Sie haelt die Gegenstuecke zu den
 * K4-Konstanten aus fixture_code_a.c — ohne zweite Datei gibt es keine
 * Doppelhaltung zu finden.
 *
 * ACHTUNG beim Bearbeiten: in dieser Datei darf kein Kommentarende als
 * Text vorkommen. Beim ersten Entwurf stand hier eine Erklaerung, die
 * die Zeichenfolge in Anfuehrungszeichen nannte — der Kommentar endete
 * dort, und das folgende Anfuehrungszeichen verschluckte als
 * Zeichenkette den Rest der Datei. Die Falle in der Datei, die sie
 * erklaert.
 */

#include <stdint.h>
#include <stddef.h>

/* ══ K4: dieselben Konstanten wie in fixture_code_a.c ════════════════ */

/* Die RICHTIGE Fassung: aus Rate und Drehzahl gerechnet. Genau das ist
 * der Punkt — hier steht sie begruendet, in Datei A geraten. */
static uint32_t track_capacity(uint16_t rate_kbps, uint16_t rpm) {
    return (uint32_t)(((uint64_t)rate_kbps * 1000u / 8u) * 60u / rpm);
}

/* Und trotzdem noch einmal als Zahl — das ist die Doppelhaltung. */
static const uint16_t cap_35hd = 12500u;
static const uint16_t cap_dd   = 6250u;
static const uint8_t  fill     = 0x4Eu;
static const uint32_t size_720 = 737280u;

/* 0xE5 steht nur hier -> kein Fund. */
static const uint8_t dirent_free = 0xE5u;

/* ══ P2: verschachtelte Kommentarklammer ═════════════════════════════

 * Der Dokublock unten enthaelt Beispielcode mit einem eigenen
 * Kommentar darin. Der aeussere Block endet am ersten inneren
 * Kommentarende, und alles dahinter wird Code.
 */

/**
 * Liest einen Sektor.
 *
 *   SetFilePointer(h, lba << 9, ...)   /* 512-Byte-Sektoren */
 *   ReadFile(h, buf, 0x200, ...)
 */
static int read_one(void *h, uint32_t lba) { (void)h; (void)lba; return 0; }

void fixture_code_b_use(void) {
    (void)track_capacity; (void)cap_35hd; (void)cap_dd; (void)fill;
    (void)size_720; (void)dirent_free; (void)read_one;
}
