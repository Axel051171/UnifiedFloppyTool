/**
 * @file test_mfi_entpackt.c
 * @brief MFI liefert entpackte Zellzeiten statt eines gepackten Stroms (MF-1070)
 *
 * `mfi` stand auf **T2**, obwohl sein Fremdabbild seit MF-1020 im Korpus
 * liegt. Der Grund war kein Formatfehler, sondern eine **Luecke im
 * Bausystem**, und sie stand als P3-329 seit MF-1023 offen:
 *
 *     zlib wird gefunden, gelinkt, mit einem Makro versehen —
 *     und erreicht keine einzige C-Datei.
 *
 * `src/core/CMakeLists.txt` setzte `UFT_HAS_ZLIB=1` **PRIVATE fuer
 * `uft_core`**, und **keine** `.c`-Datei las es; `src/formats/imz/uft_imz.c`
 * prueft `HAVE_ZLIB`, das **niemand** definiert; die `.pro`-Datei des
 * PRIMAEREN Baus kannte zlib ueberhaupt nicht. Zwei Makronamen fuer
 * dieselbe Faehigkeit, keiner davon wirksam.
 *
 * **Gemessen am Vorzustand** ueber den Produktionspfad
 * (plugin->probe/open/read_track) an `tests/corpus_free/hxcfe_pc160.mfi`:
 *
 *     Sonde     : probe=1 konf=70
 *     Geometrie : 42 x 1
 *     Gelesen   : **0 Spuren, 0 Sektoren, 0 mit Rohdaten**
 *
 * Der Leser oeffnete die Datei und lieferte fuer **keine** der 42 Spuren
 * etwas. (Davor, bis MF-1023, war es schlimmer: er gab den **zlib-Kopf
 * `78 9C ED`** als Sektordaten aus und kuerzte bei mehr als 65535 Byte
 * still — die Klasse MF-864, wo das Wissen im Kommentar stand und nicht
 * im Code.)
 *
 * ── Die Sollwerte kommen aus der DATEI und aus fremden Beschreibungen ──
 *
 * Je Spur nennt der 16-Byte-Eintrag `uncompressed_size`; geteilt durch 4
 * ist das die **Zellzahl**, und sie ist nachpruefbar, ohne UFT zu fragen.
 * Jede Zelle ist ein LE32 mit **28 Bit Zeit** (`& 0x0FFFFFFF`) und 4 Bit
 * magnetischer Ausrichtung.
 *
 * Die Zeitsumme einer Spur soll **200 000 000** ergeben — eine Umdrehung
 * in Einheiten von 1/200 000 000, laut fluxfox „1 nanosecond increments
 * at 300RPM".
 *
 * ── Warum hier eine TOLERANZ steht und keine Gleichheit ────────────────
 *
 * `src/samdisk/mfi.cpp:89` wirft eine Ausnahme, wenn die Summe nicht
 * **exakt** 200 000 000 ist. Gemessen an zwei Erzeugnissen **fremder
 * Hand** haelt das keines:
 *
 *   * `hxcfe_pc160.mfi` (hxcfe): alle 42 Spuren **200 064 000**
 *   * fluxfox' eigenes `sector_test_360k.mfi`: **199 999 972 … 975**
 *
 * SAMdisks strikte Pruefung lehnt also **beide** ab. Die beiden anderen
 * Umsetzungen im Baum bestehen nicht darauf: fluxfox rechnet die Summe
 * aus und prueft sie nicht (die Konstante steht auskommentiert),
 * hxcfes eigener Lader erwaehnt sie gar nicht. **Zwei von drei nehmen
 * die Abweichung hin, und „Kein Bit verloren" spricht fuers Lesen** —
 * eine Datei abzuweisen, die ihr eigener Erzeuger fuer gut haelt, waere
 * die Gestalt von MF-1015 (`udi`), wo ein Orakel begruendet ueberstimmt
 * wurde.
 *
 * Die Naehe ist trotzdem eine harte Zusage: **0,05 %**. Ein Leser, der
 * den gepackten Strom durchreicht oder falsch entpackt, trifft das nie —
 * er liefert Zufallszahlen aus Deflate-Kodewoertern.
 *
 * ── Die Mutation, die NICHT isolierbar ist, und warum ──────────────────
 *
 * Mutationsmatrix **5 von 6**, Grundlauf gruen. Die sechste — „die
 * Zielgroesse nicht gegenpruefen" — entkommt, und der Grund ist
 * **gemessen statt erklaert**:
 *
 *     Ziel 50 statt 100 (Spur ergibt MEHR): rc = -5  (Z_BUF_ERROR)
 *     Ziel 200 statt 100 (Spur ergibt WENIGER): rc = 0, ziel = 100
 *
 * `uncompress()` faengt den einen Fall also allein; den anderen meldet es
 * als Erfolg und setzt nur `ziel` kleiner. **Die Zusatzpruefung
 * `ziel != uncompressed_size` ist damit NICHT redundant** — sie ist an
 * diesem Erzeugnis nur nicht ausloesbar, weil keine der 42 Spuren kurz
 * entpackt. Das ist die Form aus MF-1031, wo zwei Schranken sich
 * gegenseitig deckten und die dreizehnte Mutation nachgemessen statt
 * behauptet wurde; hier ist das Ergebnis das umgekehrte, und deshalb
 * bleibt die Pruefung stehen.
 *
 * ── Was NICHT geprueft ist ──────────────────────────────────────────────
 *
 * * **Sektoren.** MFI ist ein FLUSSFORMAT; Sektoren entstehen dort erst
 *   durch einen Dekoder. P3-326 haelt genau diesen Fehlschluss fest —
 *   eine erste Fassung von `test_fremde_hand_liest.c` hat MFI mit einer
 *   SEKTOR-Zusicherung geprueft. Geprueft wird der **Fluss**.
 * * **Die Ausrichtungsbits.** Die oberen 4 Bit je Zelle traegt UFT
 *   unveraendert weiter; ihre Bedeutung ist hier nicht abgenommen.
 * * **Der Schreibpfad.**
 * * **Die Zeitbasis.** `flux_tick_ns` steht auf 1, was laut fluxfox bei
 *   300 RPM stimmt. MFI speichert **keine** RPM im Kopf, die Angabe ist
 *   also eine Konvention und keine Messung — deshalb steht sie hier und
 *   wird nicht festgenagelt.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_track.h"   /* uft_track_release — implizite Deklaration
                              * ist auf macOS-Clang ein FEHLER */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#ifndef UFT_CORPUS_DIR
#define UFT_CORPUS_DIR ""
#endif

extern const uft_format_plugin_t uft_format_plugin_mfi;

#define MFI_KOPF        32u
#define MFI_EINTRAG     16u
#define SPUREN          42u
#define ZEIT_SOLL       200000000.0
#define ZEIT_TOLERANZ   0.0005      /* 0,05 % - siehe Kopf */

static int gruen = 0, rot = 0;

static void pruefe(const char *was, int bedingung, const char *detail)
{
    if (bedingung) { gruen++; printf("  ok   %s\n", was); }
    else { rot++; printf("  FAIL %s - %s\n", was, detail ? detail : ""); }
}

static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

int main(void)
{
    const uft_format_plugin_t *p = &uft_format_plugin_mfi;
    uft_disk_t disk;
    char pfad[600], det[300], erster[220];
    uint8_t *datei = NULL;
    FILE *f;
    long gr = 0;
    unsigned c, spuren = 0, mit_fluss = 0, zellzahl_ok = 0;
    unsigned zeit_ok = 0, gepackt_gesehen = 0;
    int konf = -1, ok;

    setvbuf(stdout, NULL, _IONBF, 0);
    printf("MFI entpackt - MF-1070\n");
    printf("======================\n");

    if (UFT_CORPUS_DIR[0] == '\0') {
        printf("SKIP: UFT_CORPUS_DIR nicht gesetzt.\n");
        return 77;      /* SKIP_RETURN_CODE, seit MF-598 */
    }
    snprintf(pfad, sizeof pfad, "%s/hxcfe_pc160.mfi", UFT_CORPUS_DIR);

    f = fopen(pfad, "rb");
    if (!f) { printf("SKIP: %s fehlt.\n", pfad); return 77; }
    fseek(f, 0, SEEK_END); gr = ftell(f); fseek(f, 0, SEEK_SET);
    datei = (uint8_t *)malloc((size_t)(gr > 0 ? gr : 1));
    if (!datei || fread(datei, 1, (size_t)gr, f) != (size_t)gr) {
        fclose(f); free(datei); printf("SKIP: %s unlesbar.\n", pfad);
        return 77;
    }
    fclose(f);

    snprintf(det, sizeof det, "%ld Byte, Kennung \"%.15s\"", gr,
             (const char *)datei);
    pruefe("das hxcfe-Erzeugnis traegt \"MAMEFLOPPYIMAGE\"",
           gr > (long)MFI_KOPF && memcmp(datei, "MAMEFLOPPYIMAGE", 15) == 0,
           det);

    {
        uint32_t roh = le32(datei + 16);
        snprintf(det, sizeof det, "%u Zylinder, Aufloesung %u, %u Koepfe",
                 (unsigned)(roh & 0x3FFFFFFFu), (unsigned)(roh >> 30),
                 (unsigned)le32(datei + 20));
        pruefe("der Kopf nennt 42 Zylinder, 1 Kopf, Aufloesung 0",
               (roh & 0x3FFFFFFFu) == SPUREN && (roh >> 30) == 0
               && le32(datei + 20) == 1, det);
    }

    ok = p->probe(datei, (size_t)(gr < 65536 ? gr : 65536), (size_t)gr,
                  &konf) ? 1 : 0;
    snprintf(det, sizeof det, "probe=%d konf=%d", ok, konf);
    pruefe("die Sonde erkennt es", ok && konf >= 50, det);

    memset(&disk, 0, sizeof disk);
    if (p->open(&disk, pfad, true) != UFT_OK) {
        pruefe("open liest das Fremderzeugnis", 0, pfad);
        free(datei);
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    erster[0] = 0;
    for (c = 0; c < SPUREN; c++) {
        uft_track_t t;
        const uint8_t *eintrag = datei + MFI_KOPF + (size_t)c * MFI_EINTRAG;
        uint32_t entpackt_soll = le32(eintrag + 8);
        uint32_t zellen_soll = entpackt_soll / 4u;
        memset(&t, 0, sizeof t);
        if (p->read_track(&disk, (int)c, 0, &t) != UFT_OK) {
            if (!erster[0])
                snprintf(erster, sizeof erster, "Spur %u nicht lesbar", c);
            continue;
        }
        spuren++;
        if (t.flux && t.flux_count) {
            double summe = 0.0;
            size_t k;
            mit_fluss++;
            if (t.flux_count == (size_t)zellen_soll) zellzahl_ok++;
            else if (!erster[0])
                snprintf(erster, sizeof erster,
                         "Spur %u: %u Zellen, erwartet %u", c,
                         (unsigned)t.flux_count, (unsigned)zellen_soll);
            for (k = 0; k < t.flux_count; k++)
                summe += (double)(t.flux[k] & 0x0FFFFFFFu);
            if (summe >= ZEIT_SOLL * (1.0 - ZEIT_TOLERANZ)
                && summe <= ZEIT_SOLL * (1.0 + ZEIT_TOLERANZ))
                zeit_ok++;
            else if (!erster[0])
                snprintf(erster, sizeof erster,
                         "Spur %u: Zeitsumme %.0f", c, summe);
            /* Der gepackte Strom beginnt mit dem zlib-Kopf 0x78 0x9C.
             * Steht er in der ERSTEN Zelle, wurde nicht entpackt. */
            if (t.flux_count > 0 && (t.flux[0] & 0xFFFFu) == 0x9C78u)
                gepackt_gesehen++;
        } else if (!erster[0]) {
            snprintf(erster, sizeof erster,
                     "Spur %u liefert keinen Fluss (sector_count=%u)", c,
                     (unsigned)t.sector_count);
        }
        uft_track_release(&t);
    }
    p->close(&disk);

    snprintf(det, sizeof det, "%u von %u Spuren lesbar%s%s", spuren, SPUREN,
             erster[0] ? " - " : "", erster);
    pruefe("alle 42 Spuren sind lesbar", spuren == SPUREN, det);

    snprintf(det, sizeof det, "%u von %u Spuren mit Fluss%s%s", mit_fluss,
             SPUREN, erster[0] ? " - " : "", erster);
    pruefe("jede Spur liefert Zellzeiten - MFI ist ein FLUSSformat, "
           "Sektoren waeren hier die falsche Frage (P3-326)",
           mit_fluss == SPUREN, det);

    snprintf(det, sizeof det, "%u von %u Spuren mit richtiger Zellzahl%s%s",
             zellzahl_ok, SPUREN, erster[0] ? " - " : "", erster);
    pruefe("die Zellzahl jeder Spur ist `uncompressed_size / 4` - die Zahl "
           "steht in der DATEI, nicht in UFT",
           zellzahl_ok == SPUREN, det);

    snprintf(det, sizeof det,
             "%u von %u Spuren innerhalb 0,05 %% von 200 000 000%s%s",
             zeit_ok, SPUREN, erster[0] ? " - " : "", erster);
    pruefe("die Zeitsumme jeder Spur ist eine Umdrehung - ein falsch "
           "entpackter Strom trifft das nie",
           zeit_ok == SPUREN, det);

    snprintf(det, sizeof det, "%u Spuren beginnen mit dem zlib-Kopf 78 9C",
             gepackt_gesehen);
    pruefe("KEINE Spur reicht den gepackten Strom durch (der Befund aus "
           "MF-1020: \"Sektor 0\" begann mit 78 9C ED)",
           gepackt_gesehen == 0, det);

    free(datei);
    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
