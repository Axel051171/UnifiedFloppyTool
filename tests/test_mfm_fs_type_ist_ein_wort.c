/*
 * @file test_mfm_fs_type_ist_ein_wort.c
 * @brief Rotbeweis zu MF-1240 — `strstr(bpb.fs_type, "FAT12")` gab
 *        seine fuenf Konfidenzpunkte auch an `"XFAT12"`.
 *
 * WARUM DIESER TEST EINE EIGENE GESTALT HAT, UND WARUM ER SPAETER KOMMT
 * ALS SEIN BEFUND: MF-1237 hat diese Stelle ausdruecklich LIEGEN
 * GELASSEN und den Grund benannt — sie bewegt keine Ja/Nein-Antwort,
 * sondern nur einen Zuschlag `conf += 5`. Ein Rotbeweis muss also die
 * KONFIDENZ beobachten, nicht das Erkennungsergebnis. Genau das tut er
 * hier: zwei Bootsektoren, die sich in NICHTS unterscheiden ausser im
 * achtstelligen `fs_type`-Feld des EBPB, und die Frage ist der
 * ABSTAND ihrer Konfidenzen.
 *
 * Vorzustand (gemessen): `strstr("XFAT12  ", "FAT12")` trifft, also
 * bekommen beide denselben Zuschlag und der Abstand ist 0.
 * Nachzustand: `uft_wort_treffer()` sieht in `"XFAT12  "` das EINE Wort
 * `XFAT12`, das nicht `FAT12` ist — der Abstand ist 5.
 *
 * WARUM DIE WORTGRENZE HIER RICHTIG IST: das Feld IST der Typ. Die
 * Beschreibung fuellt es mit `"FAT12   "`, `"FAT16   "` oder
 * `"FAT     "`. Es ist KEIN Praefixstempel wie `"MSDOS5.0"` im
 * OEM-Feld, wo der Punkt als Wortzeichen alles zu einem Wort macht —
 * dort waere eine Wortgrenze falsch, und die DOS-Marken behalten
 * deshalb ihr `strstr` (siehe den Kommentar in
 * `mfm_detect_atari_st()`).
 *
 * Der Test benutzt die BENANNTEN Versaetze aus `mfm_detect.h`
 * (`EBPB_BOOT_SIGNATURE`, `EBPB_FS_TYPE`, `BOOT_SIGNATURE_OFFSET`)
 * statt Zahlen zu wiederholen — eine zweite Kopie der Versaetze waere
 * „eine Groesse, zwei Rechnungen" (MF-1177) in einem Test.
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

/* Ein gueltiger DD-BPB mit x86-Sprung und EBPB — so, dass der
 * MS-DOS-Zweig ihn annimmt und der `fs_type`-Zuschlag ueberhaupt
 * erreicht wird. */
static void bootsektor_bauen(uint8_t *boot, const char *fs_type)
{
    memset(boot, 0, 512);

    /* x86-Sprung: bringt im DOS-Zweig +10 und haelt den Atari-Zweig
     * fern (`!has_x86_jump` ist dort Bedingung). */
    boot[BPB_JMP + 0] = 0xEB;
    boot[BPB_JMP + 1] = 0x3C;
    boot[BPB_JMP + 2] = 0x90;

    memcpy(boot + BPB_OEM, "MSDOS5.0", 8);

    boot[BPB_BYTES_PER_SECTOR]     = 0x00;
    boot[BPB_BYTES_PER_SECTOR + 1] = 0x02;   /* 512 */
    boot[BPB_SECTORS_PER_CLUSTER]  = 0x02;
    boot[BPB_RESERVED_SECTORS]     = 0x01;
    boot[BPB_NUM_FATS]             = 0x02;
    boot[BPB_ROOT_ENTRIES]         = 0x70;   /* 112 */
    boot[BPB_MEDIA_DESCRIPTOR]     = 0xF9;
    boot[BPB_SECTORS_PER_FAT]      = 0x05;
    boot[BPB_SECTORS_PER_TRACK]    = 0x09;
    boot[BPB_NUM_HEADS]            = 0x02;
    boot[0x13]                     = 0xA0;   /* 1440 Sektoren, LE16 */
    boot[0x14]                     = 0x05;

    /* EBPB: ohne die Signatur 0x29 wird `fs_type` gar nicht gelesen,
     * und der Zuschlag haengt an `bpb.has_ebpb`. */
    boot[EBPB_BOOT_SIGNATURE] = 0x29;
    memset(boot + EBPB_VOLUME_LABEL, ' ', 11);
    memset(boot + EBPB_FS_TYPE, ' ', 8);
    size_t n = strlen(fs_type);
    if (n > 8) n = 8;
    memcpy(boot + EBPB_FS_TYPE, fs_type, n);

    boot[BOOT_SIGNATURE_OFFSET]     = 0x55;
    boot[BOOT_SIGNATURE_OFFSET + 1] = 0xAA;
}

/* Die Konfidenz des FAT12-DOS-Kandidaten, oder 0, wenn es keinen gibt. */
static int dos_konfidenz(const char *fs_type, int *gefunden)
{
    uint8_t boot[512];
    bootsektor_bauen(boot, fs_type);

    mfm_detect_result_t *r = mfm_detect_create();
    if (!r) { *gefunden = 0; return 0; }

    mfm_detect_analyze_boot_data(r, boot, 512);

    int conf = 0;
    *gefunden = 0;
    for (uint8_t i = 0; i < r->num_candidates; i++) {
        if (r->candidates[i].fs_type == MFM_FS_FAT12_DOS) {
            conf = (int)r->candidates[i].confidence;
            *gefunden = 1;
            break;
        }
    }
    mfm_detect_free(r);
    return conf;
}

int main(void)
{
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("MF-1240 — das fs_type-Feld und die Wortgrenze\n");

    int g1 = 0, g2 = 0, g3 = 0;
    const int mit    = dos_konfidenz("FAT12",  &g1);
    const int falsch = dos_konfidenz("XFAT12", &g2);
    const int ohne   = dos_konfidenz("FAT16",  &g3);

    printf("\n   fs_type \"FAT12   \"  -> Konfidenz %d (gefunden %d)\n",
           mit, g1);
    printf("   fs_type \"XFAT12  \"  -> Konfidenz %d (gefunden %d)\n",
           falsch, g2);
    printf("   fs_type \"FAT16   \"  -> Konfidenz %d (gefunden %d)\n",
           ohne, g3);

    /* SPERRE: ohne Kandidaten sagt der Abstand nichts. Diese Zusagen
     * sind KEINE Entdeckung, sie stellen sicher, dass die folgenden
     * ueberhaupt etwas messen (Klasse MF-1014). */
    printf("\n1) Sperren — es gibt ueberhaupt einen Kandidaten\n");
    ZUSAGE(g1, "\"FAT12\": ein FAT12-DOS-Kandidat entsteht");
    ZUSAGE(g2, "\"XFAT12\": ein FAT12-DOS-Kandidat entsteht");
    ZUSAGE(g3, "\"FAT16\": ein FAT12-DOS-Kandidat entsteht");
    ZUSAGE(mit > 0, "und seine Konfidenz ist nicht 0");

    /* DER BEWEIS: der Abstand. */
    printf("\n2) der Zuschlag gehoert dem ganzen Wort\n");
    ZUSAGE(falsch < mit,
           "\"XFAT12\" bekommt WENIGER Konfidenz als \"FAT12\"");
    ZUSAGE(mit - falsch == 5,
           "und zwar genau die 5 Punkte des fs_type-Zuschlags");

    /* GEGENPROBE: ein anderer, gueltiger Typ darf den Zuschlag ebenso
     * nicht bekommen — sonst waere die Zusage oben auch mit einer
     * kaputten Regel gruen. */
    printf("\n3) Gegenprobe — die Regel trifft nicht alles\n");
    ZUSAGE(ohne == falsch,
           "\"FAT16\" steht wie \"XFAT12\": kein FAT12-Zuschlag");
    ZUSAGE(mit - ohne == 5,
           "und derselbe Abstand von 5 zu \"FAT12\"");

    printf("\n%d gruen, %d rot\n", gruen, rot);
    return rot ? 1 : 0;
}
