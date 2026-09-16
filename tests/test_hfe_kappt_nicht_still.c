/**
 * @file test_hfe_kappt_nicht_still.c
 * @brief Ein ZUVIEL war still — bei einer 2,88-M-Diskette die Haelfte
 *        (MF-1170)
 *
 * ── Der Befund ──────────────────────────────────────────────────────────
 *
 * `uftc_convert_sectors_to_hfe()` leitet die IMG-Geometrie aus der
 * Dateigroesse ab, und der letzte Zweig ist ein `else` OHNE obere Schranke:
 * alles ueber 1 228 800 Byte wird 80 x 2 x 18 x 512. Geprueft wurde danach
 * nur
 *
 *     if (src_size < expected_size)  -> Warnung „padding with zeros"
 *
 * Ein ZUVIEL sieht diese Bedingung nicht. Die Sektorschleife rechnet
 *
 *     src_offset = ((cyl * heads * sectors) + hd * sectors + sec)
 *                  * sector_size,        mit cyl < cylinders
 *
 * erreicht also hoechstens `expected_size`. Gemessen an einer 2,88-M-Datei
 * (80 x 2 x 36 x 512 = 2 949 120 Byte): **1 474 560 Byte wurden nie
 * angefasst — genau die Haelfte, weil 36 = 2 x 18 — und `UFT_OK` kam
 * zurueck.** Klasse MF-1001/1022/1038/1040.
 *
 * ── Die schaerfste Form: derselbe Fehler war einen Zweig weiter behoben ─
 *
 * Die ADF-Seite DERSELBEN Funktion prueft das seit MF-1081 ausdruecklich —
 * dort steht „Der REST, nicht nur der Quotient", weil 901 119 Byte
 * ganzzahlig 79 Zylinder ergaben und still eine Spur verwarfen. Die
 * IMG-Seite hat diese Pruefung nie bekommen. Zwei Zweige einer Funktion,
 * einer gehaertet: Klasse MF-519/529/1026, und MF-1164 hat dieselbe Gestalt
 * schon einmal INNERHALB einer Datei gefunden.
 *
 * ── Und warum der naheliegende Fix verworfen ist ────────────────────────
 *
 * Der erste Entwurf von MF-1170 war ein 2,88-M-Zweig mit den Zahlen aus der
 * Profiltafel (`uft_fdc_detect_format(80, 2, 36, 512)` liefert eindeutig
 * „PC 2.88M" mit rpm 300 und raw_bits 400 000, und `HFE_IF_IBMPC_ED = 0x08`
 * liegt ungenutzt im Header). Die Messung hat ihn verworfen:
 * `hfe_track_entry_t.length` ist ein `uint16_t` und traegt die Laenge eines
 * SPURPAARS.
 *
 *      250 kbps / 300 U/min  ->  12 544 je Seite  ->  Paar  25 088  passt
 *      500 kbps / 360 U/min  ->  20 992           ->  Paar  41 984  passt
 *      500 kbps / 300 U/min  ->  25 088           ->  Paar  50 176  passt
 *     1000 kbps / 300 U/min  ->  50 176           ->  Paar 100 352  NICHT
 *
 * `(uint16_t)100352` ist **34 816** — eine Spurtabelle, die auf ein Drittel
 * der Daten zeigt, also ein ZWEITER stiller Verlust ueber dem ersten.
 * **HFE v1 kann eine 2,88-M-Spur nicht tragen**; die Grenze liegt bei
 * 32 767 Byte je Seite, etwa 655 kbps bei 300 U/min. Der Weg dorthin ist
 * HFE v3, nicht ein Zweig im Wandler.
 *
 * ── Zugang ──────────────────────────────────────────────────────────────
 *
 * Der Kopf wird BYTEWEISE nach der HxC-Feldlage gelesen, nicht ueber
 * `hfe_header_t`. Ein Test, der seine Erwartung aus derselben Struktur holt
 * wie der Pruefling, kann die Struktur nicht pruefen (MF-1000).
 *
 * ── Rotbeweis ───────────────────────────────────────────────────────────
 *
 * Vor MF-1170 fallen `zweikommaachtundachtzig_wird_abgesagt` und
 * `ein_byte_zu_viel_wird_abgesagt` (beide gaben `UFT_OK`), und
 * `absage_laesst_keine_datei_zurueck` fiel mit ihnen, weil eine Datei
 * entstand. Gruen vor UND nach: `eineinhalb_geht_weiterhin`,
 * `zu_kleine_datei_wird_weiter_gepolstert` und
 * `spurtabelle_grenze_ist_arithmetisch_belegt` — die ersten zwei belegen,
 * dass die Absage nicht alles ablehnt, die dritte ist reine Arithmetik.
 */
#include "uft/uft_format_plugin.h"
#include "uft/uft_format_convert.h"

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

extern uft_error_t uftc_convert_sectors_to_hfe(const uint8_t *src_data,
                                               size_t src_size,
                                               const char *dst_path,
                                               uft_format_t src_format,
                                               const uft_convert_options_ext_t *opts,
                                               uft_convert_result_t *result);

static int _pass = 0, _fail = 0, _last_fail = 0;
#define RUN(name)  do { printf("  [TEST] %-46s ... ", #name); test_##name(); \
                        if (_last_fail == _fail) { printf("OK\n"); _pass++; } \
                        _last_fail = _fail; } while (0)
#define TEST(name) static void test_##name(void)
#define ASSERT(c)  do { if (!(c)) { printf("FAIL @ %d: %s\n", __LINE__, #c); _fail++; return; } } while (0)

/* HxC-HFE-v1-Kopf, Feldlage aus der Formatbeschreibung — bewusst als
 * Versatzkonstanten und nicht ueber `hfe_header_t`. */
#define HFE_OFF_TRACKS        9u   /* uint8          */
#define HFE_OFF_SIDES        10u   /* uint8          */
#define HFE_OFF_BITRATE      12u   /* LE16, kbit/s */
#define HFE_OFF_RPM          14u   /* LE16          */
#define HFE_OFF_IFACE        16u
#define HFE_OFF_LUT_BLOCK    18u   /* LE16, in 512-Byte-Bloecken */

/* Die Groessen, um die es geht — je aus ihrer Geometrie gerechnet und nicht
 * als nackte Zahl hingeschrieben. */
#define GR_1440K  ((size_t)80u * 2u * 18u * 512u)   /* 1 474 560 */
#define GR_1200K  ((size_t)80u * 2u * 15u * 512u)   /* 1 228 800 */
#define GR_2880K  ((size_t)80u * 2u * 36u * 512u)   /* 2 949 120 */
#define GR_160K   ((size_t)40u * 1u *  8u * 512u)   /*   163 840 */
#define GR_BBC    ((size_t)80u * 1u * 10u * 256u)   /*   204 800 */

static uint8_t g_img[GR_2880K];        /* groesster Fall */
static uint8_t g_kopf[2048];           /* Kopf + erster LUT-Block */

static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }

/* Fuellt `g_img` bis `n` mit einem Muster, das JEDE Stelle unterscheidbar
 * macht — sonst saehe eine halb gelesene Datei wie eine ganze aus. */
static void muster(size_t n)
{
    for (size_t i = 0; i < n; i++)
        g_img[i] = (uint8_t)(i * 7u + (i >> 9) * 13u + 1u);
}

/* Schreibt einen echten FAT-BPB in die ersten 512 Byte von `g_img`.
 *
 * Jedes Feld steht an seiner AUSGESCHRIEBENEN Stelle und nicht ueber
 * `fat_create_boot_sector()` — sonst befragt die Pruefdatei dieselbe Quelle
 * wie der Pruefling (Klasse MF-1000, und bei `apridisk`/MF-1009 war genau
 * das der Grund, warum ein Rundlauf gruen war).
 *
 * Aufrufer setzt `muster()` VOR diesem Aufruf, sonst ueberschreibt das
 * Muster den Bootsektor wieder. */
static void bpb_schreiben(unsigned spt, unsigned heads, unsigned total,
                          unsigned bps, uint8_t media, int mit_kennung)
{
    memset(g_img, 0, 512);
    g_img[0x00] = 0xEB; g_img[0x01] = 0x3C; g_img[0x02] = 0x90;  /* JMP/NOP */
    memcpy(g_img + 0x03, "UFT-P423", 8);                         /* OEM     */
    g_img[0x0B] = (uint8_t)(bps & 0xFFu);                        /* bps     */
    g_img[0x0C] = (uint8_t)(bps >> 8);
    g_img[0x0D] = 1u;                                            /* spc     */
    g_img[0x0E] = 1u;                                            /* reserved*/
    g_img[0x10] = 2u;                                            /* FATs    */
    g_img[0x11] = 64u;                                           /* root    */
    g_img[0x13] = (uint8_t)(total & 0xFFu);                      /* total16 */
    g_img[0x14] = (uint8_t)(total >> 8);
    g_img[0x15] = media;                                         /* media   */
    g_img[0x16] = 1u;                                            /* spf     */
    g_img[0x18] = (uint8_t)(spt & 0xFFu);                        /* spt     */
    g_img[0x19] = (uint8_t)(spt >> 8);
    g_img[0x1A] = (uint8_t)(heads & 0xFFu);                      /* heads   */
    g_img[0x1B] = (uint8_t)(heads >> 8);
    if (mit_kennung) { g_img[0x1FE] = 0x55u; g_img[0x1FF] = 0xAAu; }
}

static int datei_existiert(const char *pfad)
{
    FILE *f = fopen(pfad, "rb");
    if (!f) return 0;
    fclose(f);
    return 1;
}

/* Wandelt und gibt den Rueckgabewert zurueck; liest bei Erfolg die
 * Kopfbytes nach `g_kopf`. */
static uft_error_t wandeln(size_t n, const char *pfad, size_t *gelesen)
{
    uft_convert_options_ext_t o;
    uft_convert_result_t r;

    memset(&o, 0, sizeof(o));
    o.accept_data_loss = true;        /* Sektor->Bitstrom ist verlustbehaftet */
    memset(&r, 0, sizeof(r));

    remove(pfad);
    uft_error_t rc = uftc_convert_sectors_to_hfe(g_img, n, pfad,
                                                 UFT_FORMAT_IMG, &o, &r);
    if (gelesen) *gelesen = 0;
    if (rc == UFT_OK && gelesen) {
        FILE *f = fopen(pfad, "rb");
        if (f) {
            *gelesen = fread(g_kopf, 1, sizeof(g_kopf), f);
            fclose(f);
        }
    }
    return rc;
}

/* ── ROTBEWEIS 1: die 2,88-M-Diskette ────────────────────────────────── */

TEST(zweikommaachtundachtzig_wird_abgesagt)
{
    /* 2 949 120 Byte. Vor MF-1170 kam UFT_OK zurueck und die Ausgabe war
     * eine 1,44-M-HFE: 1 474 560 Byte — genau die Haelfte — waren still
     * weg. Die Absage nennt jetzt den Grund. */
    muster(GR_2880K);
    const char *pfad = "uft_hfe_2880_absage.hfe";
    size_t gelesen = 0;
    uft_error_t rc = wandeln(GR_2880K, pfad, &gelesen);

    ASSERT(rc != UFT_OK);
    /* und die Haelfte ist keine Rundung: 36 = 2 x 18 */
    ASSERT(GR_2880K - GR_1440K == GR_1440K);
    remove(pfad);
}

/* ── ROTBEWEIS 2: EIN Byte zu viel ───────────────────────────────────── */

TEST(ein_byte_zu_viel_wird_abgesagt)
{
    /* Die schaerfere Fassung desselben Befunds. 1 474 561 Byte fallen in
     * denselben `else`-Zweig, `expected_size` ist 1 474 560 — vor MF-1170
     * verschwand das letzte Byte ohne ein Wort. */
    muster(GR_1440K + 1u);
    const char *pfad = "uft_hfe_plus1_absage.hfe";
    size_t gelesen = 0;
    ASSERT(wandeln(GR_1440K + 1u, pfad, &gelesen) != UFT_OK);
    remove(pfad);
}

TEST(absage_laesst_keine_datei_zurueck)
{
    /* Eine Absage, die eine halbe Datei hinterlaesst, ist ein zweiter
     * Befund (vgl. `test_convert_leaves_no_ghost.c`). Die Schranke greift
     * VOR dem Bau des Behaelters, also darf nichts entstehen. */
    muster(GR_2880K);
    const char *pfad = "uft_hfe_2880_geist.hfe";
    remove(pfad);
    ASSERT(wandeln(GR_2880K, pfad, NULL) != UFT_OK);
    ASSERT(!datei_existiert(pfad));
    remove(pfad);
}

/* ── Gegenprobe: was passt, geht weiterhin ───────────────────────────── */

TEST(eineinhalb_geht_weiterhin)
{
    /* Gruen vor UND nach MF-1170. 1 474 560 Byte gehen restlos in
     * 80 x 2 x 18 x 512 auf. Die Kopfwerte sind hier unabhaengig
     * hergeleitet: 500 kbit/s bei 300 U/min sind 500000 * 0,2 = 100 000
     * Datenbits, verdoppelt 200 000 Zellen, aufgerundet 25 000 Byte, auf
     * 256 ausgerichtet 25 088 — und die Spurtabelle fuehrt das PAAR, also
     * 50 176. */
    muster(GR_1440K);
    const char *pfad = "uft_hfe_1440_ok.hfe";
    size_t gelesen = 0;
    ASSERT(wandeln(GR_1440K, pfad, &gelesen) == UFT_OK);
    ASSERT(gelesen >= 1024u);
    ASSERT(memcmp(g_kopf, "HXCPICFE", 8) == 0);
    ASSERT(le16(g_kopf + HFE_OFF_BITRATE) == 500u);
    ASSERT(le16(g_kopf + HFE_OFF_RPM)     == 300u);

    /* Eintrag 0 der Spurtabelle: Versatz (LE16, in 512-Byte-Bloecken),
     * dann Laenge (LE16, in Byte). */
    unsigned lut_block = le16(g_kopf + HFE_OFF_LUT_BLOCK);
    ASSERT(lut_block >= 1u);
    size_t lut_off = (size_t)lut_block * 512u;
    ASSERT(lut_off + 4u <= gelesen);
    ASSERT(le16(g_kopf + lut_off + 2u) == 50176u);
    remove(pfad);
}

TEST(vier_groessen_gehen_weiterhin)
{
    /* Gruen vor UND nach MF-1174, und die eigentliche Absicherung: die vier
     * Groessen, die die Bereiche EXAKT treffen, gehen weiter durch. Ohne
     * diesen Fall waere „lehnt alles ab, was nicht genau passt" eine
     * genauso gruene Zusage. */
    struct { size_t n; unsigned rpm; const char *pfad; } f[] = {
        { (size_t)40u * 2u *  9u * 512u, 300u, "uft_hfe_360_ok.hfe"  },
        { (size_t)80u * 2u *  9u * 512u, 300u, "uft_hfe_720_ok.hfe"  },
        { GR_1200K,                      360u, "uft_hfe_1200_ok.hfe" },
        { GR_1440K,                      300u, "uft_hfe_1440b_ok.hfe"},
    };
    for (unsigned i = 0; i < sizeof(f) / sizeof(f[0]); i++) {
        size_t gelesen = 0;
        muster(f[i].n);
        ASSERT(wandeln(f[i].n, f[i].pfad, &gelesen) == UFT_OK);
        ASSERT(gelesen >= 1024u);
        /* 1,2 M laeuft im 5,25-Zoll-HD-Laufwerk mit 360 U/min (MF-1166) */
        ASSERT(le16(g_kopf + HFE_OFF_RPM) == f[i].rpm);
        remove(f[i].pfad);
    }
}

/* ── ROTBEWEIS 4 (MF-1174): zu kurz ist auch eine falsche Geometrie ──── */

TEST(zu_kurze_datei_wird_abgesagt)
{
    /* Bis MF-1174 stand hier das Gegenteil: dieser Test hat in MF-1170
     * festgenagelt, dass eine zu kurze Datei ANGENOMMEN wird. Gemessen war
     * das der einzige Abhaengige der Fuellfaehigkeit im ganzen Baum — und
     * die Messung dazu hat gezeigt, dass „zu kurz" fast immer eine FALSCHE
     * GEOMETRIE ist und nicht eine kurze Diskette: von zwoelf echten
     * Groessen treffen die vier Bereiche nur vier exakt.
     *
     * 1 228 288 Byte sind 512 weniger als der 1,2-M-Zweig erwartet. Vorher
     * kamen UFT_OK und 512 erfundene 0xE5-Byte heraus, als Sektordaten
     * gemeldet. */
    muster(GR_1200K - 512u);
    const char *pfad = "uft_hfe_kurz_absage.hfe";
    ASSERT(wandeln(GR_1200K - 512u, pfad, NULL) != UFT_OK);
    ASSERT(!datei_existiert(pfad));
    remove(pfad);

    /* Und die schaerfere Fassung, symmetrisch zum Byte zu viel oben:
     * EIN Byte zu wenig. */
    muster(GR_1440K - 1u);
    pfad = "uft_hfe_minus1_absage.hfe";
    ASSERT(wandeln(GR_1440K - 1u, pfad, NULL) != UFT_OK);
    remove(pfad);

    /* BERICHTIGT P3-423. Hier stand, die 160-K-Diskette werde abgesagt —
     * und diese Zusage nagelte die UNFAEHIGKEIT fest, nicht das richtige
     * Verhalten. Die Klasse, die MF-1174 gefunden hat, war echt: eine
     * 160-K-Diskette (40 x 1 x 8 x 512) wurde als 40 x 2 x 9 x 512 gelesen
     * — Kopfzahl und Sektorzahl falsch — und 204 800 der 368 640 Byte
     * waren erfunden, mehr als die Diskette selbst hat. Die Absage war die
     * richtige ERSTE Antwort darauf; sie war nie die richtige letzte.
     *
     * Seit P3-423 traegt die Achtzeilen-Tafel diese Groesse exakt, also
     * geht die Wandlung durch — mit 40 Spuren auf EINEM Kopf. Was der
     * Test hier weiter bewacht, ist die eigentliche Aussage von MF-1174:
     * **die Kopfzahl darf nicht erfunden werden.** Eine 2 statt einer 1
     * waere derselbe Befund in neuer Gestalt, und sie faellt hier auf.
     *
     * Dasselbe Muster wie MF-1151 und MF-1182: eine gruene Zusage ruhte
     * auf dem Mangel. Die beiden Haelften darueber bleiben unberuehrt —
     * 1 228 288 und 1 474 559 Byte treffen weder einen BPB noch eine
     * Tafelzeile und werden weiter abgesagt. */
    muster((size_t)40u * 1u * 8u * 512u);
    memset(g_img, 0, 512);                  /* kopflos: die Tafel traegt */
    pfad = "uft_hfe_160_tafel.hfe";
    size_t gelesen160 = 0;
    ASSERT(wandeln((size_t)40u * 1u * 8u * 512u, pfad, &gelesen160)
           == UFT_OK);
    ASSERT(g_kopf[HFE_OFF_TRACKS] == 40u);
    ASSERT(g_kopf[HFE_OFF_SIDES]  ==  1u);   /* NICHT 2 — das war der Fund */
    remove(pfad);
}

/* ── Die Grenze der Spurtabelle, arithmetisch ────────────────────────── */

TEST(spurtabelle_grenze_ist_arithmetisch_belegt)
{
    /* Die Absage im Wandler ist heute UNERREICHBAR, weil er nie mehr als
     * 500 kbps setzt. Getestet wird darum nicht der Zweig, sondern seine
     * AUSSAGE — und zwar in beide Richtungen, sonst waere „passt immer"
     * eine genauso gruene Zusage.
     *
     * Die Rechnung steht hier eigenstaendig, nicht aus dem Pruefling
     * geholt (MF-1000): Zellen = kbps * 1000 * 60 / rpm * 2, Byte
     * aufgerundet, auf 256 ausgerichtet, und die Tabelle fuehrt das PAAR. */
    struct { unsigned kbps, rpm, paar; int passt; } f[] = {
        {  250, 300,  25088, 1 },   /* DD, beide Seiten        */
        {  500, 360,  41984, 1 },   /* 1,2 M im 5,25-Zoll-HD   */
        {  500, 300,  50176, 1 },   /* 1,44 M                  */
        { 1000, 300, 100352, 0 },   /* 2,88 M ED — passt NICHT */
    };
    unsigned passend = 0u, unpassend = 0u;

    for (unsigned i = 0; i < sizeof(f) / sizeof(f[0]); i++) {
        unsigned zellen = f[i].kbps * 1000u * 60u / f[i].rpm * 2u;
        unsigned bytes  = (zellen + 7u) / 8u;
        unsigned ausg   = ((bytes + 255u) / 256u) * 256u;
        unsigned paar   = ausg * 2u;

        ASSERT(paar == f[i].paar);
        ASSERT((paar <= 0xFFFFu) == (f[i].passt != 0));
        if (f[i].passt) passend++; else unpassend++;
    }
    /* Beide Zahlen stehen absichtlich da: ohne die zweite koennte der Test
     * gruen bleiben, weil er keinen unpassenden Fall geprueft hat. */
    ASSERT(passend   == 3u);
    ASSERT(unpassend == 1u);

    /* Und die Kuerzung, die ohne die Schranke passieren wuerde. */
    ASSERT((uint16_t)100352u == 34816u);
}

/* ── ROTBEWEIS 5 (P3-423): die Geometrie steht IN der Diskette ───────── */

TEST(hundertsechzigk_mit_bpb_wird_gelesen)
{
    /* DER BEFUND. 163 840 Byte sind 40 x 1 x 8 x 512 — eine einseitige
     * 8-Sektor-Diskette. Die vier `else if`-Bereiche lesen sie als
     * 40 x 2 x 9 x 512 (368 640 Byte), also Kopfzahl UND Sektorzahl falsch,
     * und sagen seit MF-1174 deshalb ab. Absagen ist ehrlich, aber die
     * Diskette SAGT ihre Geometrie: der BPB fuehrt `sectors_per_track`,
     * `head_count`, `bytes_per_sector` und `total_sectors_16`.
     *
     * Die Zahlen sind von FREMDER HAND bestaetigt, ausgefuehrt und nicht
     * gelesen: hxcfe (HxCFloppyEmulator, im Baum gebaut) meldet fuer diese
     * Datei woertlich „Image Size:160kB, 40 tracks, 1 side(s), 8
     * sectors/track, interleave:1,rpm:300" und schreibt daraus eine IMD,
     * die je Sektor Zylinder, Kopf und Nummer ausdruecklich nennt — darin
     * stehen 319 von 319 selbstbenennenden Sektoren an ihrer eigenen
     * Ortsmarke, 0 fehlend, 0 unerwartet (der 320. ist der Bootsektor
     * selbst und traegt den BPB statt einer Marke).
     *
     * Die Bitrate ist unabhaengig hergeleitet, nicht aus dem Pruefling
     * geholt: 8 x 512 = 4096 Datenbyte je Spur, und 250 kbit/s bei
     * 300 U/min tragen 6250 (cw2dmk `jv3.h`, MF-1166) — es passt in DD,
     * also 250 kbit/s und 300 U/min. */
    muster(GR_160K);
    bpb_schreiben(8u, 1u, 320u, 512u, 0xFEu, 1);

    const char *pfad = "uft_hfe_160_bpb.hfe";
    size_t gelesen = 0;
    ASSERT(wandeln(GR_160K, pfad, &gelesen) == UFT_OK);
    ASSERT(gelesen >= 1024u);
    ASSERT(memcmp(g_kopf, "HXCPICFE", 8) == 0);
    ASSERT(g_kopf[HFE_OFF_TRACKS] == 40u);
    ASSERT(g_kopf[HFE_OFF_SIDES]  ==  1u);
    ASSERT(le16(g_kopf + HFE_OFF_BITRATE) == 250u);
    ASSERT(le16(g_kopf + HFE_OFF_RPM)     == 300u);
    remove(pfad);
}

TEST(ein_bpb_der_der_datei_widerspricht_wird_abgewiesen)
{
    /* ANTI-TAUTOLOGIE 1, und sie ist selbst ein Rotbeweis — die erste
     * Fassung war es nicht, und der Unterschied ist die Lehre aus MF-1014.
     *
     * Ohne diese Zusage koennte „nimm den BPB" heissen „glaube dem BPB
     * blind", und eine Geometrie, die nicht in die Datei passt, liest
     * hinter das Dateiende (MF-1027, wo UFT der VIB bewusst NICHT gegen
     * die Dateigroesse folgt). Bei Widerspruch wird deshalb ABGESAGT und
     * nicht zwischen zwei Aussagen derselben Diskette geraten — die Regel
     * aus MF-1039.
     *
     * Haelfte A, 720 K: 737 280 Byte treffen einen der vier alten
     * Bereiche EXAKT, heute geht die Wandlung also durch. Der BPB sagt
     * hier aber 2880 Sektoren (1,44 M) in einer 1440-Sektor-Datei. Heute
     * wird der BPB ignoriert und die Datei angenommen — die Zusage ist
     * also ROT vor dem Umbau und gruen danach.
     *
     * Die erste Fassung dieses Tests nahm 160 K, und das war gruen aus dem
     * falschen Grund: 160 K wird heute ohnehin abgesagt, die Zusage haette
     * den Widerspruch gar nicht gemessen. */
    size_t gelesen = 0;
    const size_t gr_720k = (size_t)80u * 2u * 9u * 512u;   /* 737 280 */
    muster(gr_720k);
    bpb_schreiben(18u, 2u, 2880u, 512u, 0xF0u, 1);
    const char *pfad_a = "uft_hfe_720_luegt.hfe";
    ASSERT(wandeln(gr_720k, pfad_a, &gelesen) != UFT_OK);
    ASSERT(!datei_existiert(pfad_a));

    /* Haelfte B, 160 K: der BPB sagt 640 Sektoren (40 x 2 x 8), die Datei
     * hat 320. Gruen vor UND nach dem Umbau — hier soll sich nichts
     * aendern, und ohne diese Haelfte waere „sagt bei Widerspruch ab"
     * nur an einer Groesse gemessen. */
    muster(GR_160K);
    bpb_schreiben(8u, 2u, 640u, 512u, 0xFFu, 1);
    const char *pfad_b = "uft_hfe_160_luegt.hfe";
    ASSERT(wandeln(GR_160K, pfad_b, &gelesen) != UFT_OK);
    ASSERT(!datei_existiert(pfad_b));
}

TEST(ohne_bpb_traegt_die_tafel_nur_bei_exakter_groesse)
{
    /* ANTI-TAUTOLOGIE 2, und sie hat ZWEI Haelften — eine allein waere
     * gruen aus dem falschen Grund.
     *
     * Haelfte A: eine kopflose Datei von 163 840 Byte darf durchgehen,
     * denn 320 Sektoren treffen GENAU EINE Zeile der benannten Tafel
     * `fat_geometry_160k` (src/formats/fat/uft_fat_bootsector.c), und die
     * acht Zeilen haben paarweise verschiedene Sektorsummen. Das ist nicht
     * dasselbe wie die alten `<=`-Bereiche: exakter Treffer in einer
     * benannten Tafel gegen Bereichsraten.
     *
     * Haelfte B: 204 800 Byte — die BBC-DFS-Groesse 80 x 1 x 10 x 256 —
     * muessen WEITER abgesagt werden. 204 800 / 512 = 400 Sektoren, und
     * 400 steht in keiner der acht Zeilen. Das ist die wichtigere Haelfte:
     * eine BBC-Diskette ist keine FAT-Diskette, und der alte Bereich hat
     * sie mit 40 x 2 x 9 beansprucht. Der Anspruch war der Fehler. */
    muster(GR_160K);
    memset(g_img, 0, 512);                 /* kein BPB, keine Kennung */
    const char *pfad_a = "uft_hfe_160_kopflos.hfe";
    size_t gelesen = 0;
    ASSERT(wandeln(GR_160K, pfad_a, &gelesen) == UFT_OK);
    ASSERT(g_kopf[HFE_OFF_TRACKS] == 40u);
    ASSERT(g_kopf[HFE_OFF_SIDES]  ==  1u);
    remove(pfad_a);

    muster(GR_BBC);
    memset(g_img, 0, 512);
    const char *pfad_b = "uft_hfe_bbc_abgesagt.hfe";
    ASSERT(wandeln(GR_BBC, pfad_b, &gelesen) != UFT_OK);
    ASSERT(!datei_existiert(pfad_b));
}

TEST(eine_ungedeckte_drehzahl_wird_nicht_erfunden)
{
    /* ANTI-TAUTOLOGIE 3, und sie haelt eine STOPPBEDINGUNG fest.
     *
     * Mit BPB-Geometrie waere die 2,88-M-Diskette (80 x 2 x 36 x 512)
     * plotzlich ableitbar — 36 x 512 = 18 432 Datenbyte je Spur. Die im
     * Baum benannte Quelle (cw2dmk `jv3.h`, MF-1166) fuehrt 250 kbit/s bei
     * 300 U/min (6250 Byte), 500 bei 360 (10 416) und 500 bei 300
     * (12 500) — 18 432 passt in KEINE davon, und die 1000 kbit/s der
     * ED-Diskette hat im Baum keine Quelle. Also wird abgesagt, nicht
     * gerechnet.
     *
     * Ohne diese Zusage koennte der Umbau eine Bitrate erfinden, und das
     * waere genau der Verstoss, den MF-1077 verbietet. Sie bewacht
     * zugleich MF-1170: die 2,88-M-Absage bleibt, nur ihr GRUND wird
     * genauer. */
    muster(GR_2880K);
    bpb_schreiben(36u, 2u, 5760u, 512u, 0xF0u, 1);

    const char *pfad = "uft_hfe_2880_bpb.hfe";
    size_t gelesen = 0;
    ASSERT(wandeln(GR_2880K, pfad, &gelesen) != UFT_OK);
    ASSERT(!datei_existiert(pfad));

    /* Und die Gegenrichtung, damit „sagt immer ab" nicht gruen ist: mit
     * 18 Sektoren statt 36 traegt dieselbe Herleitung. */
    muster(GR_1440K);
    bpb_schreiben(18u, 2u, 2880u, 512u, 0xF0u, 1);
    const char *pfad2 = "uft_hfe_1440_bpb.hfe";
    ASSERT(wandeln(GR_1440K, pfad2, &gelesen) == UFT_OK);
    ASSERT(le16(g_kopf + HFE_OFF_BITRATE) == 500u);
    ASSERT(le16(g_kopf + HFE_OFF_RPM)     == 300u);
    remove(pfad2);
}

TEST(zwoelf_groessen_einzeln_gemessen)
{
    /* P3-423 verlangt woertlich: „jede der acht Groessen braucht ihre
     * eigene Messung". Das ist sie — und sie steht bewusst als TAFEL da,
     * damit die Zahl am Ende nicht die einzige Aussage ist (MF-1026: eine
     * Summe, die aufgeht, sagt nichts ueber die Verteilung darin).
     *
     * Die zwoelf Zeilen sind dieselben, die MF-1174 im Wandler gemessen
     * hat. Je Zeile zwei Laeufe: MIT eigenem BPB und KOPFLOS. `soll_bpb`
     * und `soll_tafel` sind die erwarteten Ausgaenge, `rpm` die erwartete
     * Drehzahl im BPB-Lauf. */
    struct {
        const char *name;
        unsigned zyl, kopf, spt, bps;
        int soll_bpb;      /* 1 = wandelt, 0 = Absage */
        int soll_tafel;    /* kopflos: 1 = wandelt, 0 = Absage */
        unsigned rpm;      /* nur wenn soll_bpb */
    } t[] = {
      { "160K",  40, 1,  8,  512, 1, 1, 300 },
      { "180K",  40, 1,  9,  512, 1, 1, 300 },
      /* BBC DFS ist KEINE FAT-Diskette: mit erfundenem BPB waere sie eine,
       * kopflos treffen ihre 400 Sektoren keine Tafelzeile -> Absage. Der
       * alte Bereich hat sie als 40x2x9 beansprucht; der Anspruch war der
       * Fehler. */
      { "BBC",   80, 1, 10,  256, 1, 0, 300 },
      { "320K",  40, 2,  8,  512, 1, 1, 300 },
      { "360K",  40, 2,  9,  512, 1, 1, 300 },
      { "400K",  80, 1, 10,  512, 1, 0, 300 },
      { "640K",  80, 2, 16,  256, 1, 0, 300 },
      { "720K",  80, 2,  9,  512, 1, 1, 300 },
      { "800K",  80, 2, 10,  512, 1, 0, 300 },
      { "1,2M",  80, 2, 15,  512, 1, 1, 360 },
      /* PC-98 2HD: 8 x 1024 = 8192 Datenbyte je Spur passt nicht in die
       * 6250 der DD-Zeile, und 8 Sektoren sind weder die belegten 15 noch
       * 18. Die Drehzahl dieser Diskette ist im Baum ausdruecklich
       * widerspruechlich belegt (P3-431) — also Absage mit genannten
       * Zahlen, keine erfundene Rate (MF-1077). */
      { "PC-98", 77, 2,  8, 1024, 0, 0, 0   },
      { "1,44M", 80, 2, 18,  512, 1, 1, 300 },
    };
    unsigned bpb_ok = 0u, bpb_ab = 0u, tafel_ok = 0u, tafel_ab = 0u;

    for (unsigned i = 0; i < sizeof(t) / sizeof(t[0]); i++) {
        size_t n = (size_t)t[i].zyl * t[i].kopf * t[i].spt * t[i].bps;
        unsigned total = t[i].zyl * t[i].kopf * t[i].spt;
        size_t gelesen = 0;
        char pfad[64];

        /* Lauf 1: die Diskette sagt ihre Geometrie selbst. */
        snprintf(pfad, sizeof(pfad), "uft_hfe_z%02u_bpb.hfe", i);
        muster(n);
        bpb_schreiben(t[i].spt, t[i].kopf, total, t[i].bps, 0xF0u, 1);
        uft_error_t rc = wandeln(n, pfad, &gelesen);
        ASSERT((rc == UFT_OK) == (t[i].soll_bpb != 0));
        if (rc == UFT_OK) {
            /* Die GEOMETRIE, nicht nur der Erfolg — sonst waere „wandelt"
             * auch mit falscher Kopfzahl gruen (der Fund aus MF-1174). */
            ASSERT(g_kopf[HFE_OFF_TRACKS] == (uint8_t)t[i].zyl);
            ASSERT(g_kopf[HFE_OFF_SIDES]  == (uint8_t)t[i].kopf);
            ASSERT(le16(g_kopf + HFE_OFF_RPM) == t[i].rpm);
            bpb_ok++;
            remove(pfad);
        } else {
            ASSERT(!datei_existiert(pfad));
            bpb_ab++;
        }

        /* Lauf 2: kopflos — nur die benannte Achtzeilen-Tafel. */
        snprintf(pfad, sizeof(pfad), "uft_hfe_z%02u_tafel.hfe", i);
        muster(n);
        memset(g_img, 0, 512);
        rc = wandeln(n, pfad, &gelesen);
        ASSERT((rc == UFT_OK) == (t[i].soll_tafel != 0));
        if (rc == UFT_OK) {
            ASSERT(g_kopf[HFE_OFF_TRACKS] == (uint8_t)t[i].zyl);
            ASSERT(g_kopf[HFE_OFF_SIDES]  == (uint8_t)t[i].kopf);
            tafel_ok++;
            remove(pfad);
        } else {
            tafel_ab++;
        }
    }

    /* Die Summen stehen ABSICHTLICH zuletzt (MF-1026) — und beide
     * Richtungen, sonst waere „wandelt alles" genauso gruen. */
    ASSERT(bpb_ok   == 11u);
    ASSERT(bpb_ab   ==  1u);
    ASSERT(tafel_ok ==  7u);
    ASSERT(tafel_ab ==  5u);
}

int main(void)
{
    printf("=== HFE-Wandler: ein Zuviel ist keine Rundung (MF-1170) ===\n");
    RUN(zweikommaachtundachtzig_wird_abgesagt);
    RUN(ein_byte_zu_viel_wird_abgesagt);
    RUN(absage_laesst_keine_datei_zurueck);
    RUN(zu_kurze_datei_wird_abgesagt);
    printf("--- Gegenproben: was passt, geht weiterhin ---\n");
    RUN(eineinhalb_geht_weiterhin);
    RUN(vier_groessen_gehen_weiterhin);
    printf("--- Grenze der HFE-v1-Spurtabelle ---\n");
    RUN(spurtabelle_grenze_ist_arithmetisch_belegt);
    printf("--- P3-423: die Geometrie steht IN der Diskette ---\n");
    RUN(hundertsechzigk_mit_bpb_wird_gelesen);
    RUN(ein_bpb_der_der_datei_widerspricht_wird_abgewiesen);
    RUN(ohne_bpb_traegt_die_tafel_nur_bei_exakter_groesse);
    RUN(eine_ungedeckte_drehzahl_wird_nicht_erfunden);
    RUN(zwoelf_groessen_einzeln_gemessen);
    printf("\nErgebnis: %d bestanden, %d gefallen\n", _pass, _fail);
    return _fail == 0 ? 0 : 1;
}
