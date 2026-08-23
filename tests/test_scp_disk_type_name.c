/**
 * @file test_scp_disk_type_name.c
 * @brief uft_scp_disk_type_name() gegen die benannte SCP-Spec (MF-510)
 *
 * REFERENZ (MF-498(a), benannt und zitiert):
 *   SuperCard Pro Image File Specification v2.5, 11. Februar 2024,
 *   https://www.cbmstuff.com/downloads/scp/scp_image_specs.txt
 *   Abschnitt "DISK TYPE":
 *       UPPER 4 BITS ARE USED TO DEFINE A DISK CLASS (MANUFACTURER)
 *       LOWER 4 BITS ARE USED TO DEFINE A DISK SUB-CLASS (MACHINE)
 *       0000 = COMMODORE   0001 = ATARI   0010 = APPLE   0011 = PC
 *       0100 = TANDY       0101 = TEXAS INSTRUMENTS
 *       0110 = ROLAND      1000 = OTHER
 *
 * WARUM ES DIESEN TEST GIBT
 *
 * `uft_scp_disk_type_name` existierte ZWEIMAL unter demselben Namen:
 * als `static inline` in `uft/uft_scp_format.h` und als gelinkte
 * Funktion, deklariert in `uft/flux/uft_scp_parser.h`. Beide standen
 * unter demselben Waechter `UFT_SCP_DISK_TYPE_NAME_DECLARED` — welche
 * ein Uebersetzungslauf sah, entschied allein die Include-Reihenfolge.
 * Der Waechter hat den Widerspruch nicht gemeldet, sondern verdeckt.
 *
 * Und sie widersprachen sich: 0x04 hiess "Amiga DD" gegen "Amiga",
 * 0x24 "Apple 400K" gegen "Macintosh 400K", 0x30 "IBM PC 360K" gegen
 * "PC 360KB". Aufgefallen ist es nie, weil **keine der beiden einen
 * Aufrufer hatte**. Dieser Test ist der erste.
 *
 * Er prueft NUR die Codes, die die Spec oben ausdruecklich nennt.
 * Was die Spec nicht nennt, prueft er nicht — siehe "NICHT GEPRUEFT"
 * am Dateiende.
 */
#include <stdio.h>
#include <string.h>

#include "uft/flux/uft_scp_parser.h"

static int failures = 0;

/* Die Spec nennt Codes, keine Anzeigetexte. Geprueft wird deshalb, dass
 * der Name den Rechner nennt, den die Spec dem Code zuordnet — nicht ein
 * woertlicher Stringvergleich, der nur die Implementierung gegen sich
 * selbst prueft. */
static void expect_contains(uint8_t code, const char *needle, const char *spec_name)
{
    const char *got = uft_scp_disk_type_name(code);
    if (got && strstr(got, needle) != NULL) {
        printf("  ok   0x%02X  %-16s -> \"%s\"\n", code, spec_name, got);
        return;
    }
    printf("  FAIL 0x%02X  %-16s -> \"%s\" (erwartet: enthaelt \"%s\")\n",
           code, spec_name, got ? got : "(NULL)", needle);
    failures++;
}

int main(void)
{
    printf("SCP DISK TYPE gegen Spec v2.5 (2024-02-11)\n");

    printf("\nCOMMODORE (0000):\n");
    expect_contains(0x00, "Commodore 64", "disk_C64");
    expect_contains(0x04, "Amiga",        "disk_Amiga");
    expect_contains(0x08, "Amiga",        "disk_AmigaHD");

    printf("\nATARI (0001):\n");
    expect_contains(0x10, "Atari FM SS",  "disk_AtariFMSS");
    expect_contains(0x11, "Atari FM DS",  "disk_AtariFMDS");
    expect_contains(0x12, "Atari FM",     "disk_AtariFMEx");
    expect_contains(0x14, "Atari ST SS",  "disk_AtariSTSS");
    expect_contains(0x15, "Atari ST DS",  "disk_AtariSTDS");
    expect_contains(0x16, "Atari ST SS",  "disk_AtariSTSSHD");
    expect_contains(0x17, "Atari ST DS",  "disk_AtariSTDSHD");

    printf("\nAPPLE (0010):\n");
    expect_contains(0x20, "Apple II",     "disk_AppleII");
    expect_contains(0x21, "Apple II",     "disk_AppleIIPro");
    expect_contains(0x24, "400K",         "disk_Apple400K");
    expect_contains(0x25, "800K",         "disk_Apple800K");
    expect_contains(0x26, "1.44",         "disk_Apple144");

    printf("\nPC (0011):\n");
    expect_contains(0x30, "360K",         "disk_PC360K");
    expect_contains(0x31, "720K",         "disk_PC720K");
    expect_contains(0x32, "1.2M",         "disk_PC12M");
    expect_contains(0x33, "1.44M",        "disk_PC144M");

    printf("\nTANDY (0100):\n");
    expect_contains(0x40, "SS/SD",        "disk_TRS80SSSD");
    expect_contains(0x41, "SS/DD",        "disk_TRS80SSDD");
    expect_contains(0x42, "DS/SD",        "disk_TRS80DSSD");
    expect_contains(0x43, "DS/DD",        "disk_TRS80DSDD");

    printf("\nHERSTELLER-NIBBLE (uft_scp_manufacturer_name):\n");
    {
        const struct { uint8_t code; const char *name; } man[] = {
            {0x00, "Commodore"}, {0x10, "Atari"}, {0x20, "Apple"},
            {0x30, "IBM PC"},    {0x40, "Tandy"}, {0x50, "Texas Instruments"},
            {0x60, "Roland"},    {0x80, "Other"},
        };
        for (size_t i = 0; i < sizeof(man) / sizeof(man[0]); i++) {
            const char *got = uft_scp_manufacturer_name(man[i].code);
            if (got && strcmp(got, man[i].name) == 0) {
                printf("  ok   0x%02X -> \"%s\"\n", man[i].code, got);
            } else {
                printf("  FAIL 0x%02X -> \"%s\" (erwartet \"%s\")\n",
                       man[i].code, got ? got : "(NULL)", man[i].name);
                failures++;
            }
        }
    }

    /* Ein unbelegter Code darf nicht als bekannter Rechner durchgehen.
     * 0x0F ist Commodore-Klasse mit einer Unterklasse, die die Spec nicht
     * kennt — der Name muss das zugeben. */
    printf("\nUNBELEGTER CODE:\n");
    {
        const char *got = uft_scp_disk_type_name(0x0F);
        if (got && strstr(got, "Unknown") != NULL) {
            printf("  ok   0x0F -> \"%s\"\n", got);
        } else {
            printf("  FAIL 0x0F -> \"%s\" (muss \"Unknown\" enthalten)\n",
                   got ? got : "(NULL)");
            failures++;
        }
    }

    /* NICHT GEPRUEFT (MF-498(b): Ungemessenes steht als ungemessen da):
     *  - 0x7x (Amstrad CPC), 0xEx (Tape), 0xFx (HDD): die zitierte
     *    Spec-Fassung nennt fuer das obere Nibble nur 0000-0110 und 1000.
     *    Diese Konstanten stehen im Enum von uft/uft_scp_format.h; ob sie
     *    aus einer spaeteren Spec, aus einem anderen Werkzeug oder aus
     *    einer Annahme stammen, ist unbelegt.
     *  - 0x8x-Unterklassen (OTHER_320K/1M2/720K/1M44): die Spec belegt
     *    1000 = OTHER als Klasse, nicht diese Unterklassen.
     * Beides bleibt deshalb ungeprueft statt falsch geprueft. */

    printf("\n%s (%d Abweichungen)\n", failures ? "FEHLGESCHLAGEN" : "OK", failures);
    return failures ? 1 : 0;
}
