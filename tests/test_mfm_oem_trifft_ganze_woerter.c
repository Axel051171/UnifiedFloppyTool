/*
 * @file test_mfm_oem_trifft_ganze_woerter.c
 * @brief Rotbeweis zu MF-1237 — „TOS" steckt in „TOSHIBA", und
 *        `mfm_detect_atari_st()` entschied damit ueber die
 *        Formaterkennung.
 *
 * REFERENZ (EINFRIER-REGEL, Bedingung a): der Befund ist am
 * PRODUKTIONSPFAD gemessen und steht VOR der Korrektur. Der Vorzustand
 * suchte im OEM-Feld des BPB mit
 *
 *     strstr(oem, "ATARI") || strstr(oem, "TOS") ||
 *     strstr(oem, "atari") || strstr(oem, "GEM")
 *
 * und entschied zwanzig Zeilen weiter:
 *
 *     if (has_atari_oem && !has_x86_jump) return true;
 *
 * Der Puffer ist dabei sauber begrenzt — `char oem[9]`, acht Byte
 * kopiert, Byte 8 bleibt 0 — es wird NICHTS ueberlesen. Der Fehler ist
 * semantisch, und er ist folgenreich, weil `mfm_detect.c:711` Atari ST
 * ausdruecklich VOR DOS prueft („weil BPB kompatibel") und bei einem
 * Treffer Konfidenz 80 setzt.
 *
 * Die Entscheidungskette der Funktion, vollstaendig:
 *
 *     if (has_68k_jump)                    return true;   // boot[0]==0x60
 *     if (has_atari_checksum)              return true;   // == 0x1234
 *     if (has_atari_oem && !has_x86_jump)  return true;
 *     return false;
 *
 * Damit die OEM-Zeile ueberhaupt entscheidet, muessen die beiden
 * darueber schweigen. Jeder Fall hier setzt deshalb das Sprungbyte auf
 * 0x00 — eine nicht bootfaehige Diskette, bei der das Byte beliebig ist
 * — und PRUEFT, dass die Pruefsumme nicht 0x1234 ist. Ohne diese
 * Sperre koennte eine Zusage gruen sein, ohne die OEM-Pruefung
 * beruehrt zu haben (Klasse MF-1014).
 *
 * WAS DIESER TEST NICHT BEHAUPTET: dass ein OEM-Feld wie „ATARITOS"
 * vorkommt. Nach der Korrektur trifft es nicht mehr, und ob es
 * existiert, ist NICHT gemessen — im Korpus liegt kein Beleg. Die Wahl
 * steht in `mfm_detect.c` begruendet: ein falsches JA bei Konfidenz 80
 * vor dem DOS-Zweig verdraengt die richtige Antwort, ein falsches NEIN
 * fuehrt in die BPB-kompatible FAT12-Lesart.
 */

#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "uft/detect/mfm_detect.h"

static int gruen = 0;
static int rot = 0;

#define ZUSAGE(bed, text)                                                   \
    do {                                                                    \
        if (bed) { gruen++; printf("   [ok ] %s\n", (text)); }              \
        else     { rot++;   printf("   [ROT] %s\n", (text)); }              \
    } while (0)

/* Ein gueltiger DD-BPB, Feld fuer Feld wie in `test_mfm_detect.c:183`
 * ff. — damit `mfm_parse_fat_bpb()` ihn annimmt und die Erkennung
 * ueberhaupt bis zur OEM-Frage kommt. */
static void bpb_fuellen(uint8_t *boot)
{
    memset(boot, 0, 512);
    boot[0x0B] = 0x00; boot[0x0C] = 0x02;   /* 512 Byte/Sektor */
    boot[0x0D] = 0x02;                       /* 2 Sektoren/Cluster */
    boot[0x0E] = 0x01; boot[0x0F] = 0x00;   /* 1 reserviert */
    boot[0x10] = 0x02;                       /* 2 FATs */
    boot[0x11] = 0x70; boot[0x12] = 0x00;   /* 112 Wurzeleintraege */
    boot[0x13] = 0xA0; boot[0x14] = 0x05;   /* 1440 Sektoren */
    boot[0x15] = 0xF9;                       /* Medienbyte */
    boot[0x16] = 0x05; boot[0x17] = 0x00;   /* 5 Sektoren/FAT */
    boot[0x18] = 0x09; boot[0x19] = 0x00;   /* 9 Sektoren/Spur */
    boot[0x1A] = 0x02; boot[0x1B] = 0x00;   /* 2 Koepfe */
}

/* OEM-Feld sind 8 Byte ab Versatz 3, mit Leerzeichen gepolstert — so
 * wie ein Formatierer es stempelt. */
static void oem_setzen(uint8_t *boot, const char *s)
{
    memset(boot + 3, ' ', 8);
    size_t n = strlen(s);
    if (n > 8) n = 8;
    memcpy(boot + 3, s, n);
}

/* Ein Fall: OEM setzen, Sprungbyte neutral, und die zwei Zweige ueber
 * der OEM-Zeile ausdruecklich stillstellen. */
static void fall(const char *oem, int erwartet, const char *was)
{
    uint8_t boot[512];
    char merk[180];

    bpb_fuellen(boot);
    boot[0] = 0x00;          /* nicht 0x60 (68k), nicht 0xEB/0xE9 (x86) */
    oem_setzen(boot, oem);

    /* SPERRE: die Pruefsumme darf nicht 0x1234 sein, sonst entscheidet
     * sie und die OEM-Zeile wird nie erreicht. */
    const uint16_t ps = mfm_atari_st_checksum(boot);
    snprintf(merk, sizeof merk,
             "\"%s\": Pruefsumme ist nicht 0x1234 (gemessen 0x%04X) — "
             "die OEM-Zeile entscheidet wirklich", oem, ps);
    ZUSAGE(ps != 0x1234, merk);

    const int ist = mfm_detect_atari_st(boot, 512) ? 1 : 0;
    snprintf(merk, sizeof merk, "\"%s\" -> %s  (%s)",
             oem, ist ? "Atari ST" : "NICHT Atari ST", was);
    ZUSAGE(ist == erwartet, merk);
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("MF-1237 — das OEM-Feld und die Wortgrenze\n");

    /* -- 1) Die Fallen: eine Marke im Wortinneren ---------------------- */
    printf("\n1) Marke steckt in einem anderen Wort (war der Defekt)\n");
    fall("TOSHIBA", 0, "\"TOS\" steckt darin — Toshiba stempelte Disketten");
    fall("PROTOS",  0, "\"TOS\" am Wortende");
    fall("GEMINI",  0, "\"GEM\" am Wortanfang");
    fall("ATARIX",  0, "\"ATARI\" am Wortanfang, aber nicht das Wort");

    /* -- 2) Die echten Marken muessen weiter treffen ------------------- */
    printf("\n2) die Marken als ganzes Wort (darf nicht verloren gehen)\n");
    fall("ATARI",    1, "das Wort selbst");
    fall("TOS",      1, "das Wort selbst");
    fall("GEM",      1, "das Wort selbst");
    fall("atari",    1, "klein geschrieben — zweite Abfrage im Quelltext");
    fall("TOS 1.4",  1, "Marke plus Versionsangabe, durch Leerzeichen "
                        "getrennt");
    fall("ATARI ST", 1, "Marke plus Modell");

    /* -- 3) TAUTOLOGIE-SPERRE: die anderen Pfade bleiben unberuehrt ---- */
    printf("\n3) Gegenproben — die uebrigen Entscheidungswege\n");
    {
        uint8_t boot[512];

        /* Ein LEERES OEM-Feld gilt weiter als Atari-typisch. Das ist der
         * Pfad, ueber den `test_mfm_detect.c` seit immer laeuft — er
         * darf sich nicht bewegt haben. */
        bpb_fuellen(boot);
        boot[0] = 0x00;
        memset(boot + 3, ' ', 8);
        ZUSAGE(mfm_detect_atari_st(boot, 512),
               "leeres OEM-Feld bleibt Atari-typisch (oem_empty)");

        /* Das 68k-Sprungbyte entscheidet VOR der OEM-Frage und ist
         * unberuehrt — auch mit einem PC-OEM. */
        bpb_fuellen(boot);
        boot[0] = 0x60;
        oem_setzen(boot, "MSDOS5.0");
        ZUSAGE(mfm_detect_atari_st(boot, 512),
               "68k-Sprungbyte 0x60 entscheidet weiter, trotz PC-OEM");

        /* Ein x86-Sprungbyte sperrt die OEM-Zeile ausdruecklich — auch
         * bei einer echten Atari-Marke. Das war vorher so und bleibt. */
        bpb_fuellen(boot);
        boot[0] = 0xEB;
        oem_setzen(boot, "ATARI");
        const uint16_t ps = mfm_atari_st_checksum(boot);
        ZUSAGE(ps != 0x1234,
               "Gegenprobe x86: Pruefsumme nicht 0x1234 (Sperre)");
        ZUSAGE(!mfm_detect_atari_st(boot, 512),
               "x86-Sprungbyte sperrt die OEM-Zeile weiterhin");

        /* Und ein gewoehnlicher PC-Datentraeger bleibt einer. */
        bpb_fuellen(boot);
        boot[0] = 0x00;
        oem_setzen(boot, "MSDOS5.0");
        ZUSAGE(!mfm_detect_atari_st(boot, 512),
               "\"MSDOS5.0\" ist kein Atari ST");
    }

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
