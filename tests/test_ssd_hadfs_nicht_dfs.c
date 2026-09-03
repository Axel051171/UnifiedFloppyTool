/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_ssd_hadfs_nicht_dfs.c
 * @brief Eine HADFS-Diskette ist kein DFS-Volume (MF-836).
 *
 * ── Der Befund ───────────────────────────────────────────────────────────
 *
 * HADFS (J. G. Harston, BBC/Electron/Master) schreibt beim ANLEGEN einer
 * Diskette einen **echten DFS-Katalog** in die Sektoren 0/1 — als
 * Kompatibilitaetsmassnahme, damit ein DFS-System die Diskette nicht fuer
 * unformatiert haelt. Quelle: HADFS 6.10 Quelltext, `S.HADFS8` §InstDFS:
 *
 *     LDA FSM+31:AND #&41:BNE InstNoDFS   \ Don't overwrite sectors 0/1
 *     .InstDFS
 *     JSR ClearDIR:JSR dirInit
 *     ...
 *     JSR InstCopyMem                     \ Save data to disk
 *
 * Nur wenn Flag-Bit 0 (NoDFS) oder Bit 6 (Locked) gesetzt ist, bleiben
 * die Sektoren 0/1 unberuehrt. Im Normalfall stehen dort Titel „HADFS"
 * und die Eintraege `!Boot`, `$`, `HADFSROM` (`S.HADFS9` §BootCode).
 *
 * `uft_ssd_plugin_probe()` prueft die Dateigroesse und dann genau diesen
 * Katalog — Sektorzahl-Byte bei 0x107 und Bootoption bei 0x106 — und
 * meldet bei Erfolg **85**. Auf der MF-729-Skala heisst das „Merkmal
 * getroffen". Das ist hier falsch: getroffen wurde ein Merkmal, das
 * HADFS absichtlich hinterlegt hat, und das eigentliche Dateisystem ab
 * Sektor 71 bleibt unsichtbar. Kein Absturz, keine Warnung, ein
 * **plausibles falsches Ergebnis**.
 *
 * ── Die Kennung ──────────────────────────────────────────────────────────
 *
 * HADFS prueft sich selbst an acht Byte (`S.HADFS9` §ChkJGH gegen
 * §JGHName, Puffer bei &F00, verglichen ab &F10 = Offset 16):
 *
 *     Sektor 70, Byte 16..23 = 00 28 43 29 4A 47 48 00   („\0(C)JGH\0")
 *
 * Bei 256 Byte je Sektor ist das Dateioffset 70*256 + 16 = **0x4610**,
 * und damit innerhalb des Sondenpuffers (`UFT_PROBE_BUFFER_SIZE` =
 * 65536). Eine Pruefung, acht Byte.
 *
 * ── Was dieser Test NICHT tut ────────────────────────────────────────────
 *
 * Er fuehrt **kein** HADFS-Lesen ein. Das waere ein neues Format und
 * fiele unter das Moratorium (EINFRIER-REGEL, MF-363/498) — auch als
 * Vorschlag. Geprueft wird allein, dass die BESTEHENDE SSD-Sonde ihren
 * Anspruch zurueckzieht: die Groesse stimmt weiter (30 = „nur die
 * Groesse"), das Merkmal wird nicht mehr beansprucht.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_ssd;

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-44s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); \
                        _fail++; return; } } while (0)

#define SEK        256u
#define SPT         10u
#define SPUREN      40u
#define BILDGROESSE (SPUREN * SPT * SEK)          /* 102400 */
#define HADFS_OFF   (70u * SEK + 16u)             /* 0x4610  */

static const uint8_t JGH[8] = { 0x00, '(', 'C', ')', 'J', 'G', 'H', 0x00 };

/* 40-Spur-Abbild mit plausiblem DFS-Katalog; `mit_hadfs` setzt zusaetzlich
 * die HADFS-Kennung in Sektor 70. */
static uint8_t *baue(bool mit_hadfs)
{
    uint8_t *b = (uint8_t *)calloc(1, BILDGROESSE);
    if (!b) return NULL;

    /* Sektor 0: Titel, dann Namenseintraege — wie HADFS es schreibt. */
    memcpy(b + 0, "HADFS\0 D", 8);
    memcpy(b + 8, "!Boot  \xA4", 8);   /* Verzeichnis '$' mit Bit 7 = locked */

    /* Sektor 1: die Felder, die die Sonde prueft.
     * 400 Sektoren = 0x190 -> High-Bits 0x01 bei 0x106, Low 0x90 bei 0x107.
     * Bootoption in Bit 4-5 von 0x106; hier 0. */
    b[0x105] = 8;        /* 8 Byte Eintraege = 1 Datei */
    b[0x106] = 0x01;
    b[0x107] = 0x90;

    if (mit_hadfs) {
        memcpy(b + 70u * SEK, "(Untitled_Disk) ", 16);
        memcpy(b + HADFS_OFF, JGH, sizeof JGH);
    }
    return b;
}

TEST(ohne_kennung_bleibt_die_dfs_erkennung_stark)
{
    /* Gegenprobe. Ohne diesen Fall zeigt der Rotbeweis nichts: er koennte
     * auch dann gruen sein, wenn die Sonde generell 30 meldet. */
    uint8_t *b = baue(false);
    ASSERT(b != NULL);
    int konf = -1;
    ASSERT(uft_format_plugin_ssd.probe(b, BILDGROESSE, BILDGROESSE, &konf));
    ASSERT(konf >= 80);          /* Merkmal getroffen — hier zu Recht */
    free(b);
}

TEST(mit_kennung_faellt_der_anspruch_auf_nur_die_groesse)
{
    /* Rotbeweis. Vor MF-836 meldete die Sonde hier ebenfalls 85 — also
     * „Merkmal getroffen" auf einem Merkmal, das HADFS absichtlich
     * hinterlegt hat. */
    uint8_t *b = baue(true);
    ASSERT(b != NULL);
    int konf = -1;
    ASSERT(uft_format_plugin_ssd.probe(b, BILDGROESSE, BILDGROESSE, &konf));

    /* Die Groesse stimmt weiter — die Datei ist ein 40-Spur-Acorn-Abbild.
     * Beansprucht werden darf nach der MF-729-Skala aber nur das:
     * 30..49 = „nur die Groesse". */
    ASSERT(konf >= 30);
    ASSERT(konf < 50);
    free(b);
}

TEST(die_kennung_wird_byteweise_geprueft_nicht_ungefaehr)
{
    /* Ein einzelnes falsches Byte an der Kennungsstelle darf die
     * Erkennung NICHT herabsetzen — sonst wuerde die Sonde echte
     * DFS-Disketten mit Zufallsinhalt in Sektor 70 abwerten. */
    for (size_t i = 0; i < sizeof JGH; i++) {
        uint8_t *b = baue(true);
        ASSERT(b != NULL);
        b[HADFS_OFF + i] ^= 0xFF;
        int konf = -1;
        ASSERT(uft_format_plugin_ssd.probe(b, BILDGROESSE, BILDGROESSE, &konf));
        ASSERT(konf >= 80);
        free(b);
    }
}

TEST(ein_zu_kurzer_puffer_setzt_nichts_herab)
{
    /* Wenn der Sondenpuffer Sektor 70 nicht enthaelt, ist die Frage
     * unbeantwortbar — und eine unbeantwortbare Frage darf keine
     * Antwort erzwingen. */
    uint8_t *b = baue(true);
    ASSERT(b != NULL);
    int konf = -1;
    /* Nur 0x200 Byte sichtbar, Dateigroesse aber korrekt gemeldet. */
    ASSERT(uft_format_plugin_ssd.probe(b, 0x200, BILDGROESSE, &konf));
    ASSERT(konf >= 80);
    free(b);
}

int main(void)
{
    printf("=== HADFS ist kein DFS-Volume (MF-836) ===\n");
    RUN(ohne_kennung_bleibt_die_dfs_erkennung_stark);
    RUN(mit_kennung_faellt_der_anspruch_auf_nur_die_groesse);
    RUN(die_kennung_wird_byteweise_geprueft_nicht_ungefaehr);
    RUN(ein_zu_kurzer_puffer_setzt_nichts_herab);
    printf("\nErgebnis: %d bestanden, %d fehlgeschlagen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
