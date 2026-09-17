/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_genesis_sucht_nicht_ueber_das_feld.c
 * @brief Der Konsolenname eines Mega-Drive-ROMs ist 16 Byte lang — und
 *        `strstr` weiss das nicht.
 *
 * ROTBEWEIS ZUERST (EINFRIER-REGEL, Posten `A-027`, MF-1232)
 * ---------------------------------------------------------
 * `uft_genesis.c` holte den Konsolennamen als ZEIGER ins Rohabbild:
 *
 *     const char *system = (const char *)(data + 0x100);
 *     if (strstr(system, "32X") != NULL) return GENESIS_TYPE_32X;
 *
 * Geprueft war nur `size >= 0x200`, also 0x100 Byte hinter `system`.
 * `strstr` laeuft bis zum ersten Nullbyte — enthaelt das ROM dort
 * keines, liest es ueber die Puffergrenze. Und schon INNERHALB des
 * Abbilds ist die Aussage falsch: eine Zeichenfolge irgendwo im
 * Programmkode wird fuer den Konsolennamen gehalten.
 *
 * Die Quelle ist der Baum selbst, es braucht keine fremde:
 *   `uft_genesis.h:43`  GENESIS_HEADER_OFFSET  0x100
 *   `uft_genesis.h:97`  char system[16]        das Feld IST 16 Byte
 *
 * Gegen den Vorzustand gemessen faellt dieser Test mit
 * `32X statt MD` und `FORMAT_32X statt FORMAT_BIN` — die Zeichenfolge
 * „32X" steht bei 0x180, also AUSSERHALB des Konsolennamens.
 *
 * Die Positivfaelle in der Mitte halten fest, dass die Begrenzung die
 * Regel nicht entkernt: ein echter 32X-, Mega-CD- und PICO-Name wird
 * weiter erkannt.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include "uft/formats/sega/uft_genesis.h"

static int fehler = 0;

static void zusage(int bedingung, const char *text)
{
    if (bedingung) {
        printf("  [ok ] %s\n", text);
    } else {
        printf("  [ROT] %s\n", text);
        fehler++;
    }
}

/* Ein 512-Byte-ROM-Kopf: Konsolenname bei 0x100, kein einziges
 * Nullbyte ab dort — genau die Lage, in der `strstr` keine Grenze hat.
 *
 * 512 Byte sind ausserdem zu klein fuer `genesis_is_smd()` (es
 * verlangt SMD_HEADER_SIZE + SMD_BLOCK_SIZE), also kommt die
 * Formaterkennung sicher am SMD-Zweig vorbei. */
static void bauen(uint8_t img[0x200], const char *name16,
                  size_t stelle, const char *koeder)
{
    memset(img, 0xFF, 0x200);            /* Vektortafel, egal */
    memcpy(img + 0x100, name16, 16);     /* Konsolenname, 16 Byte */
    memset(img + 0x110, 'A', 0x0F0);     /* kein Nullbyte dahinter */
    if (koeder)
        memcpy(img + stelle, koeder, strlen(koeder));
}

int main(void)
{
    uint8_t img[0x200];

    puts("Der Koeder steht AUSSERHALB des 16-Byte-Konsolennamens");

    /* „SEGA SSF        " ist 16 Byte, beginnt mit SEGA und ist keiner
     * der benannten Namen — damit laeuft die Erkennung bis zu den
     * Suchen durch, die hier geprueft werden. */
    bauen(img, "SEGA SSF        ", 0x180, "32X");
    zusage(genesis_detect_system(img, sizeof img) == GENESIS_TYPE_MD,
           "\"32X\" bei 0x180 macht daraus kein 32X (Rueckfall MD)");
    zusage(genesis_detect_format(img, sizeof img) == GENESIS_FORMAT_BIN,
           "und auch kein 32X-FORMAT");

    bauen(img, "SEGA SSF        ", 0x190, "MEGA-CD");
    zusage(genesis_detect_system(img, sizeof img) == GENESIS_TYPE_MD,
           "\"MEGA-CD\" bei 0x190 macht daraus kein Sega CD");

    bauen(img, "SEGA SSF        ", 0x190, "MEGA CD");
    zusage(genesis_detect_system(img, sizeof img) == GENESIS_TYPE_MD,
           "und die Schreibweise mit Leerzeichen auch nicht");

    bauen(img, "SEGA SSF        ", 0x1A0, "PICO");
    zusage(genesis_detect_system(img, sizeof img) == GENESIS_TYPE_MD,
           "\"PICO\" bei 0x1A0 macht daraus kein Pico");

    puts("Im Feld selbst wird weiter erkannt — die Begrenzung entkernt nichts");

    bauen(img, "SEGA 32X        ", 0, NULL);
    zusage(genesis_detect_system(img, sizeof img) == GENESIS_TYPE_32X,
           "\"SEGA 32X\" IM Konsolennamen ist ein 32X");
    zusage(genesis_detect_format(img, sizeof img) == GENESIS_FORMAT_32X,
           "und ergibt das 32X-Format");

    bauen(img, "SEGA MEGA-CD    ", 0, NULL);
    zusage(genesis_detect_system(img, sizeof img) == GENESIS_TYPE_SCD,
           "\"SEGA MEGA-CD\" ist ein Sega CD");

    bauen(img, "SEGA PICO       ", 0, NULL);
    zusage(genesis_detect_system(img, sizeof img) == GENESIS_TYPE_PICO,
           "\"SEGA PICO\" ist ein Pico");

    bauen(img, "SEGA MEGA DRIVE ", 0x180, "32X");
    zusage(genesis_detect_system(img, sizeof img) == GENESIS_TYPE_MD,
           "\"SEGA MEGA DRIVE\" bleibt MD, auch mit \"32X\" im Abbild");

    bauen(img, "SEGA GENESIS    ", 0x180, "PICO");
    zusage(genesis_detect_system(img, sizeof img) == GENESIS_TYPE_GENESIS,
           "\"SEGA GENESIS\" bleibt Genesis");

    puts("Raender");

    zusage(genesis_detect_system(NULL, 0x200) == GENESIS_TYPE_UNKNOWN,
           "NULL ergibt UNKNOWN");
    bauen(img, "SEGA SSF        ", 0, NULL);
    zusage(genesis_detect_system(img, 0x1FF) == GENESIS_TYPE_UNKNOWN,
           "ein Byte zu klein ergibt UNKNOWN — der Kopf muss ganz da sein");

    printf("\n%s (%d Fehler)\n", fehler ? "GEFALLEN" : "BESTANDEN", fehler);
    return fehler ? 1 : 0;
}
