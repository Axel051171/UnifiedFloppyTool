/*
 * @file test_hfe_create_zellen_statt_bytes.c
 * @brief Rotbeweis zu MF-1242 — `hfe_create()` erklaerte eine Spur, die
 *        eine HALBE Umdrehung lang ist, und behauptete dazu 300 U/min
 *        fuer ein 360-U/min-Medium.
 *
 * HERKUNFT DES BEFUNDS: Posten `A-029`, Regel K4 („eine physikalische
 * Konstante in mehr als einer Datei"). Der Eigentuemer nennt den Fall in
 * seinem eigenen Werkzeugkopf und in seiner Koederdatei
 * `tests/formats/fixture_code_a.c:19-22` woertlich:
 *
 *     „12500 und 6250 — der belegte uft_hfe.c-Fall."
 *     return (bitrate >= 500u) ? 12500u : 6250u;
 *
 * Seine Begruendung lautete: `uft_hfe.c` errate die Spurkapazitaet,
 * waehrend `uft_fdc_gaps.h` sie fuer dasselbe Profil fuehre. Das ist
 * richtig — und beim Nachmessen war der Fehler GROESSER als notiert.
 *
 * ZWEI FEHLER IN EINER ZEILE
 * --------------------------
 * (1) EINHEIT. HFE speichert den ZELLSTROM, ein Bit je Zelle. Die
 *     Profiltafel fuehrt beides getrennt: `track_bytes` sind dekodierte
 *     Bytes, `raw_bits` sind Zellen. MFM legt ZWEI Zellen auf ein
 *     Datenbit, also ist `raw_bits == track_bytes * 16`.
 *     `hfe_create()` nahm `track_bytes`. Jede erzeugte HFE erklaerte
 *     damit eine halb so lange Spur.
 *
 * (2) DREHZAHL. `bitrate` kam aus der Sektorzahl und die Spurlaenge aus
 *     `bitrate` allein — die Drehzahl kam in der Rechnung nicht vor.
 *     Fuer die 360-U/min-Formate ist die Umdrehung 166,7 ms statt
 *     200 ms lang; der Kopf schrieb trotzdem unbedingt `rpm = 300`.
 *
 * DAS ORAKEL: DREI LEER ERZEUGTE HFE-DATEIEN VON HxC
 * --------------------------------------------------
 * `docs/OPEN_ITEMS.md` `P3-309` sagte, fuer `hfe_create` fehle das
 * Orakel: „Was fehlt, ist eine von HxC oder greaseweazle LEER erzeugte
 * HFE." Es gibt sie seit MF-1242 — `hxcfe.exe` (v2.16.15.2, im Baum
 * unter `tools/uft-scout/work/`, Kanal *Oracle* seit MF-1020) legt mit
 * `-uselayout:X -conv:HXC_HFE -foutput:Y` eine leere Diskette an, ganz
 * ohne Eingabedatei. Gemessen, je Seite:
 *
 *     Layout                 HxC      UFT-Tafel raw_bits/8   vorher
 *     DOS_DD_720KB          12504         12500              6250
 *     DOS_HD_1M44           25016         25000             12500
 *     X68000_2HD_1232KB     20840         20833             12500
 *
 * HxC liegt 4 bis 7 Byte ueber der Tafel — dieselbe Groesse, etwas Luft.
 * Und HxCs eigene Zahlen sind genau `bitRate * 2 * (60/rpm) / 8`, was
 * die Rechnung von fremder Hand bestaetigt.
 *
 * WO DIESER TEST DEM ORAKEL NICHT FOLGT, UND WARUM
 * ------------------------------------------------
 * Alle drei HxC-Dateien tragen `floppyRPM = 0`, auch die mit 360 U/min;
 * greaseweazle ebenso (`gw_amigados.hfe`). Trotzdem schreibt UFT seit
 * MF-1242 die BEKANNTE Drehzahl, und der Grund ist gemessen: aus
 * `bitRate = 500` und `rpm = 0` (was „300" heisst) rechnet ein Leser
 * 25000 Byte je Seite, waehrend HxCs eigene Spurtafel dort 20840 sagt —
 * die X68000-Datei widerspricht sich selbst. Ein Orakel ist eine
 * Referenz, kein Beweis (MF-1015). Was UFT NICHT tut, ist eine Drehzahl
 * erfinden: ohne Profil steht dort `0` (unbestimmt) statt der frueheren
 * falschen 300.
 *
 * WARUM ER ROT WERDEN KANN: gegen den Vorzustand fallen die
 * Laengenzusagen aller drei Geometrien und die Drehzahl-Zusage der
 * 1,2-M-Geometrie. Die Sperren davor bleiben in beiden Zustaenden
 * gruen — sie messen, dass ueberhaupt eine Datei mit lesbarer
 * Spurtafel entsteht.
 */

#include "uft/uft_format_plugin.h"
#include "uft/uft_types.h"
#include "uft/formats/uft_fdc_gaps.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern const uft_format_plugin_t uft_format_plugin_hfe;

static int gruen = 0;
static int rot = 0;

#define ZUSAGE(bed, text)                                                   \
    do {                                                                    \
        if (bed) { gruen++; printf("   [ok ] %s\n", (text)); }              \
        else     { rot++;   printf("   [ROT] %s\n", (text)); }              \
    } while (0)

/* Feldlagen des HFE-v1-Kopfs. Im Baum belegt: `write_allowed` bei 20
 * durch `tests/test_hfe_write_allowed.c`, die Reihenfolge durch
 * `hfe_header_t` in `src/formats/hfe/uft_hfe.c`. */
#define HFE_OFF_BITRATE    12
#define HFE_OFF_RPM        14
#define HFE_OFF_LUTBLOCK   18
#define HFE_BLOCK         512

static uint16_t le16(const uint8_t *b, size_t o)
{
    return (uint16_t)(b[o] | ((uint16_t)b[o + 1] << 8));
}

/* Was eine erzeugte Datei ueber ihre Spur sagt. */
typedef struct {
    int      ok;            /* Datei da, Kennung stimmt, Tafel lesbar */
    uint16_t bitrate;
    uint16_t rpm;
    uint16_t lut_laenge;    /* beide Seiten zusammen */
} hfe_befund_t;

static hfe_befund_t erzeugen_und_lesen(const char *pfad,
                                       uint16_t zyl, uint16_t koepfe,
                                       uint16_t sektoren, uint16_t groesse)
{
    hfe_befund_t b;
    memset(&b, 0, sizeof(b));

    remove(pfad);

    uft_disk_t disk;
    memset(&disk, 0, sizeof(disk));
    uft_geometry_t geo;
    memset(&geo, 0, sizeof(geo));
    geo.cylinders = zyl;
    geo.heads = koepfe;
    geo.sectors = sektoren;
    geo.sector_size = groesse;

    if (!uft_format_plugin_hfe.create) return b;
    if (uft_format_plugin_hfe.create(&disk, pfad, &geo) != UFT_OK) return b;
    if (uft_format_plugin_hfe.close) uft_format_plugin_hfe.close(&disk);

    FILE *f = fopen(pfad, "rb");
    if (!f) return b;

    uint8_t kopf[32];
    if (fread(kopf, 1, sizeof(kopf), f) != sizeof(kopf)) { fclose(f); return b; }
    if (memcmp(kopf, "HXCPICFE", 8) != 0) { fclose(f); return b; }

    b.bitrate = le16(kopf, HFE_OFF_BITRATE);
    b.rpm     = le16(kopf, HFE_OFF_RPM);

    const long lut = (long)le16(kopf, HFE_OFF_LUTBLOCK) * HFE_BLOCK;
    uint8_t eintrag[4];
    if (fseek(f, lut, SEEK_SET) != 0
        || fread(eintrag, 1, sizeof(eintrag), f) != sizeof(eintrag)) {
        fclose(f);
        return b;
    }
    fclose(f);

    b.lut_laenge = le16(eintrag, 2);
    b.ok = 1;
    return b;
}

int main(void)
{
    printf("=== MF-1242: HFE speichert ZELLEN, nicht dekodierte Bytes ===\n");

    /* ---- 1) DIE TAFEL DES BAUMS SAGT, WAS DER TEST ERWARTET ------- */
    /* Die Sollzahlen kommen aus derselben Tafel, die das Plugin
     * benutzt — und genau deshalb steht darunter eine Herleitung aus
     * der Physik, die sie PRUEFT. Ein Test, der nur die Quelle seines
     * Pruefgegenstands befragt, ist ein geschlossener Kreis (MF-1000). */
    printf("\n1) die Profiltafel und die Physik sagen dasselbe\n");

    const uft_fdc_format_t *p720  = uft_fdc_detect_format(80, 2,  9, 512);
    const uft_fdc_format_t *p1440 = uft_fdc_detect_format(80, 2, 18, 512);
    const uft_fdc_format_t *p1200 = uft_fdc_detect_format(80, 2, 15, 512);

    ZUSAGE(p720 && p1440 && p1200,
           "SPERRE: alle drei Profile sind auffindbar");
    if (!p720 || !p1440 || !p1200) {
        printf("\n%d gruen, %d rot\n", gruen, rot);
        return 1;
    }

    /* Herleitung, unabhaengig von der Tafel:
     *   Zellen je Umdrehung = Datenrate[bit/s] * (60/rpm) * 2
     *   (MFM legt ZWEI Zellen auf ein Datenbit)
     *   250 kbit/s @ 300 U/min : 250000 * 0,2 * 2 = 100000 Zellen
     *   500 kbit/s @ 300 U/min : 500000 * 0,2 * 2 = 200000 Zellen
     *   500 kbit/s @ 360 U/min : 500000 / 6  * 2  = 166666 Zellen  */
    ZUSAGE(p720->raw_bits  == 100000u && p720->rpm  == 300,
           "720K:  100000 Zellen bei 300 U/min");
    ZUSAGE(p1440->raw_bits == 200000u && p1440->rpm == 300,
           "1.44M: 200000 Zellen bei 300 U/min");
    ZUSAGE(p1200->raw_bits == 166666u && p1200->rpm == 360,
           "1.2M:  166666 Zellen bei 360 U/min");

    /* Und die Gegenprobe zur Einheit: `raw_bits` ist genau das
     * 16-fache von `track_bytes`. Ohne sie koennte „Zellen" hier auch
     * nur ein zweiter Name fuer dieselbe Zahl sein. */
    ZUSAGE(p720->raw_bits == p720->track_bytes * 16u
           && p1440->raw_bits == p1440->track_bytes * 16u,
           "GEGENPROBE: raw_bits ist das 16-fache von track_bytes");

    const uint16_t soll720  = (uint16_t)(p720->raw_bits  / 8u);
    const uint16_t soll1440 = (uint16_t)(p1440->raw_bits / 8u);
    const uint16_t soll1200 = (uint16_t)(p1200->raw_bits / 8u);

    /* ---- 2) SPERREN: es entsteht ueberhaupt etwas Lesbares -------- */
    printf("\n2) Sperren — die Dateien entstehen und sind lesbar\n");

    hfe_befund_t dd   = erzeugen_und_lesen("uft_hfe_1242_dd.hfe",  80, 2,  9, 512);
    hfe_befund_t hd   = erzeugen_und_lesen("uft_hfe_1242_hd.hfe",  80, 2, 18, 512);
    hfe_befund_t hd12 = erzeugen_und_lesen("uft_hfe_1242_12m.hfe", 80, 2, 15, 512);

    ZUSAGE(dd.ok,   "SPERRE: 720K erzeugt, Kennung und Spurtafel lesbar");
    ZUSAGE(hd.ok,   "SPERRE: 1.44M erzeugt, Kennung und Spurtafel lesbar");
    ZUSAGE(hd12.ok, "SPERRE: 1.2M erzeugt, Kennung und Spurtafel lesbar");
    ZUSAGE(dd.lut_laenge != 0 && hd.lut_laenge != 0 && hd12.lut_laenge != 0,
           "SPERRE: keine der drei Spurlaengen ist 0");
    ZUSAGE(dd.lut_laenge != hd.lut_laenge,
           "SPERRE: DD und HD unterscheiden sich ueberhaupt");

    /* ---- 3) DER BEWEIS: die Spur ist eine GANZE Umdrehung lang ---- */
    printf("\n3) die erklaerte Spurlaenge ist der Zellstrom, nicht die Nutzlast\n");

    ZUSAGE(dd.lut_laenge == (uint16_t)(soll720 * 2u),
           "720K:  Spurtafel nennt 2 x 12500 Byte");
    ZUSAGE(hd.lut_laenge == (uint16_t)(soll1440 * 2u),
           "1.44M: Spurtafel nennt 2 x 25000 Byte");
    ZUSAGE(hd12.lut_laenge == (uint16_t)(soll1200 * 2u),
           "1.2M:  Spurtafel nennt 2 x 20833 Byte");

    /* Der Vorzustand in einer Zeile: er nannte fuer BEIDE HD-Formate
     * dieselbe Zahl, obwohl sie sich um die Drehzahl unterscheiden. */
    ZUSAGE(hd.lut_laenge != hd12.lut_laenge,
           "1.44M und 1.2M sind VERSCHIEDEN lang — die Drehzahl zaehlt");

    /* ---- 4) DIE DREHZAHL WIRD NICHT MEHR FALSCH BEHAUPTET --------- */
    printf("\n4) der Kopf behauptet keine falsche Drehzahl mehr\n");

    ZUSAGE(dd.rpm == 300,   "720K:  rpm = 300");
    ZUSAGE(hd.rpm == 300,   "1.44M: rpm = 300");
    ZUSAGE(hd12.rpm == 360, "1.2M:  rpm = 360 — vorher stand hier 300");

    /* GEGENPROBE zum bitrate-Feld: es ist NICHT Teil der Korrektur.
     * Gemessen stimmt es mit allen drei HxC-Dateien ueberein (250 /
     * 500 / 500), also gibt es dort keinen Befund — und diese Zeile
     * haelt fest, dass die Aenderung es nicht nebenbei verschoben hat. */
    ZUSAGE(dd.bitrate == 250 && hd.bitrate == 500 && hd12.bitrate == 500,
           "GEGENPROBE: bitrate bleibt 250/500/500 wie bei HxC");

    /* ---- 5) DIE GRENZE: was nicht in ein u16 passt, wird ABGESAGT - */
    printf("\n5) die Grenze des Formats wird gemeldet, nicht gekappt\n");

    /* ED (2.88M) haette 400000 Zellen je Seite / 8 = 50000 Byte, beide
     * Seiten 100000 — das Laengenfeld der Spurtafel ist ein u16 und
     * fasst 65535. Gekappt ergaebe 34464, also eine Spur, die kuerzer
     * ist als ihre eigenen Daten. D5: melden, nicht klemmen. */
    const uft_fdc_format_t *ped = uft_fdc_detect_format(80, 2, 36, 512);
    ZUSAGE(ped != NULL, "SPERRE: das ED-Profil ist auffindbar");
    if (ped) {
        ZUSAGE(ped->raw_bits / 8u * 2u > 0xFFFFu,
               "SPERRE: ED braucht mehr, als ein u16 fassen kann");
    }

    const char *ed_pfad = "uft_hfe_1242_ed.hfe";
    remove(ed_pfad);
    uft_disk_t ed_disk;
    memset(&ed_disk, 0, sizeof(ed_disk));
    uft_geometry_t ed_geo;
    memset(&ed_geo, 0, sizeof(ed_geo));
    ed_geo.cylinders = 80; ed_geo.heads = 2;
    ed_geo.sectors = 36;   ed_geo.sector_size = 512;
    const uft_error_t ed_rc =
        uft_format_plugin_hfe.create(&ed_disk, ed_pfad, &ed_geo);
    ZUSAGE(ed_rc != UFT_OK,
           "2.88M wird ABGESAGT statt mit gekappter Laenge geschrieben");
    remove(ed_pfad);

    /* ---- 6) UNBEKANNTE GEOMETRIE: erfindet keine Drehzahl --------- */
    printf("\n6) ohne Profil wird nichts behauptet\n");

    /* BERICHTIGT WAEHREND DES ROTBEWEIS-LAUFS. Hier stand zuerst:
     * „77 x 1 x 26 x 128 ist die klassische 8-Zoll-FM-Diskette und
     * steht nicht in der Tafel — gemessen gibt `uft_fdc_detect_format`
     * dort NULL." Das war eine ANNAHME und keine Messung: die Tafel
     * fuehrt sie als `UFT_FDC_FM_SD` (`uft_fdc_gaps.h:607`), und die
     * Sperre fiel im ersten Lauf. Der Test hat damit meinen eigenen
     * Irrtum gefangen, nicht den des Prueflings.
     *
     * Genommen wird stattdessen die im Baum bereits BELEGTE
     * Nicht-Treffer-Geometrie: `tests/test_fdc_profil_verdrahtet.c:180`
     * haelt fest, dass 42 x 2 x 18 x 512 nichts trifft und im zweiten
     * Durchlauf zwei Profile findet (PC 1.44M und Atari ST HD), also
     * abgesagt wird. Der Rueckfall muss weiterhin eine Datei anlegen
     * und darf dabei KEINE Drehzahl behaupten. */
    const uft_fdc_format_t *pfremd = uft_fdc_detect_format(42, 2, 18, 512);
    ZUSAGE(pfremd == NULL, "SPERRE: 42x2x18x512 hat kein Profil");

    hfe_befund_t fremd =
        erzeugen_und_lesen("uft_hfe_1242_fremd.hfe", 42, 2, 18, 512);
    ZUSAGE(fremd.ok, "ohne Profil entsteht trotzdem eine lesbare Datei");
    ZUSAGE(fremd.rpm == 0,
           "und der Kopf sagt `unbestimmt` (0) statt der falschen 300");

    remove("uft_hfe_1242_dd.hfe");
    remove("uft_hfe_1242_hd.hfe");
    remove("uft_hfe_1242_12m.hfe");
    remove("uft_hfe_1242_fremd.hfe");

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
