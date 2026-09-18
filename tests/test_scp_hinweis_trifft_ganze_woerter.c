/* SPDX-License-Identifier: GPL-2.0-or-later */
/**
 * @file test_scp_hinweis_trifft_ganze_woerter.c
 * @brief „st" steckt in „fastest" — und entschied damit den Diskettentyp.
 *
 * ROTBEWEIS ZUERST (EINFRIER-REGEL, Posten `A-027`, MF-1233)
 * ---------------------------------------------------------
 * `scp_disk_type_from_hint()` waehlte den SCP-Diskettentyp mit acht
 * `strstr()`-Aufrufen, und die Reihenfolge machte das kuerzeste
 * Kennwort zur Falle:
 *
 *     if (strstr(hint, "atari") || strstr(hint, "st"))  return ATARI_ST;
 *     if (strstr(hint, "hd")    || strstr(hint, "1.44")) return PC_HD;
 *
 * Gemessen gegen den Vorzustand: `"pc-hd-fastest"` enthaelt `"st"` in
 * „fastest" und wurde **vor** der `hd`-Pruefung als Atari ST
 * beantwortet — 0x08 statt 0x30. Ebenso `"Bestand"`, `"test"`,
 * `"shd"` (enthaelt `"hd"`), `"xadfx"` (enthaelt `"adf"`) und `"ad64"`
 * (enthaelt `"d64"`).
 *
 * WAS HIER AUSDRUECKLICH NICHT ENTSCHIEDEN WIRD
 * ---------------------------------------------
 * Die Funktion hat **keinen Aufrufer** — `git grep` findet ausser ihrer
 * Deklaration in `uft_scp_writer.h:94` nichts. Ihr Vertrag ist damit
 * unbestimmt: ist `hint` eine Format-Kennung („adf") oder Fliesstext
 * („Amiga ADF 880K")? Das entscheidet dieser Commit NICHT (Stoppregel
 * S5).
 *
 * Geaendert ist nur, was unter BEIDEN Lesarten falsch war: ein Kennwort
 * trifft als ganzes WORT. Die Gross-/Kleinschreibung bleibt
 * unangetastet — `strstr` war case-sensitiv, und das zu aendern waere
 * eine zweite Entscheidung ohne Aufrufer.
 */

#include <stdio.h>
#include <stdint.h>

#include "uft/formats/uft_scp_writer.h"

static int fehler = 0;

static void zusage(const char *hint, uint8_t soll, const char *warum)
{
    const uint8_t ist = scp_disk_type_from_hint(hint);
    if (ist == soll) {
        printf("  [ok ] %-16s -> 0x%02X   %s\n",
               hint ? hint : "(NULL)", ist, warum);
    } else {
        printf("  [ROT] %-16s -> 0x%02X, erwartet 0x%02X   %s\n",
               hint ? hint : "(NULL)", ist, soll, warum);
        fehler++;
    }
}

int main(void)
{
    puts("Ein Kennwort im INNEREN eines Wortes entscheidet nicht mehr");
    zusage("pc-hd-fastest", SCP_TYPE_PC_HD,
           "\"st\" steckt in \"fastest\" — vorher 0x08 (Atari ST)");
    zusage("Bestand", SCP_TYPE_PC_DD, "\"st\" steckt in \"Bestand\"");
    zusage("test", SCP_TYPE_PC_DD, "\"st\" steckt in \"test\"");
    zusage("shd", SCP_TYPE_PC_DD, "\"hd\" steckt in \"shd\"");
    zusage("xadfx", SCP_TYPE_PC_DD, "\"adf\" steckt in \"xadfx\"");
    zusage("ad64", SCP_TYPE_PC_DD, "\"d64\" steckt in \"ad64\"");

    puts("Die echten Kennworte treffen weiter — die Regel ist nicht entkernt");
    zusage("adf", SCP_TYPE_AMIGA, "allein");
    zusage("amiga", SCP_TYPE_AMIGA, "allein");
    zusage("amiga-adf", SCP_TYPE_AMIGA, "als Wort in einer Kette");
    zusage("c64", SCP_TYPE_C64, "allein");
    zusage("d64", SCP_TYPE_C64, "allein");
    zusage("atari", SCP_TYPE_ATARI_ST, "allein");
    zusage("st", SCP_TYPE_ATARI_ST, "allein — das kuerzeste Kennwort");
    zusage("atari-st", SCP_TYPE_ATARI_ST, "beide Worte");
    zusage("hd", SCP_TYPE_PC_HD, "allein");

    puts("Der Punkt gehoert zum Wort, sonst zerfaellt \"1.44\"");
    zusage("1.44", SCP_TYPE_PC_HD, "ein Wort, nicht \"1\" und \"44\"");
    zusage("pc-1.44", SCP_TYPE_PC_HD, "als Wort in einer Kette");
    zusage("pc hd.", SCP_TYPE_PC_HD,
           "ein anhaengender Punkt macht \"hd\" nicht unkenntlich");

    puts("Raender");
    zusage(NULL, SCP_TYPE_PC_DD, "kein Hinweis -> Rueckfall");
    zusage("", SCP_TYPE_PC_DD, "leerer Hinweis -> Rueckfall");
    zusage("unbekannt", SCP_TYPE_PC_DD, "kein Kennwort -> Rueckfall");

    printf("\n%s (%d Fehler)\n", fehler ? "GEFALLEN" : "BESTANDEN", fehler);
    return fehler ? 1 : 0;
}
